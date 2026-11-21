#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

namespace esphome {
namespace jura_z2 {

static const char *const TAG = "jura_z2";

class JuraZ2 : public uart::UARTDevice {
 public:
  explicit JuraZ2(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}

  void set_espresso(sensor::Sensor *s) { espresso_ = s; }
  void set_double_espresso(sensor::Sensor *s) { double_espresso_ = s; }
  void set_coffee(sensor::Sensor *s) { coffee_ = s; }
  void set_double_coffee(sensor::Sensor *s) { double_coffee_ = s; }
  void set_cappuccino(sensor::Sensor *s) { cappuccino_ = s; }
  void set_latte(sensor::Sensor *s) { latte_ = s; }
  void set_double_ristretto(sensor::Sensor *s) { double_ristretto_ = s; }
  void set_brew_cycles(sensor::Sensor *s) { brew_cycles_ = s; }
  void set_clean(sensor::Sensor *s) { clean_ = s; }
  void set_descale(sensor::Sensor *s) { descale_ = s; }
  void set_grounds(sensor::Sensor *s) { grounds_ = s; }

  void set_tray_sensor(text_sensor::TextSensor *s) { tray_ = s; }
  void set_tank_sensor(text_sensor::TextSensor *s) { tank_ = s; }
  void set_status_sensor(text_sensor::TextSensor *s) { status_ = s; }
  void set_firmware_sensor(text_sensor::TextSensor *s) { firmware_ = s; }
  void set_ic_bits_sensor(text_sensor::TextSensor *s) { ic_bits_ = s; }
  void set_hz_sensor(text_sensor::TextSensor *s) { hz_ = s; }
  void set_cs_sensor(text_sensor::TextSensor *s) { cs_ = s; }
  void set_counter_changes_sensor(text_sensor::TextSensor *s) { counter_changes_ = s; }

  void set_power_switch(switch_::Switch *s) { power_switch_ = s; }

  void set_powered(bool powered) { this->apply_power_state_(powered); }

  void set_event_sensor(text_sensor::TextSensor *s) { event_ = s; }

  bool queue_command(const std::string &command) {
    if (command.empty() || command.size() > 32 ||
        command.find_first_of("\r\n") != std::string::npos) {
      ESP_LOGW(TAG, "Ungueltiges Kommando verworfen: '%s'", command.c_str());
      return false;
    }
    if (commands_.size() >= 8) {
      ESP_LOGW(TAG, "Kommando-Queue voll, %s verworfen", command.c_str());
      return false;
    }
    commands_.push_back(command);
    ESP_LOGD(TAG, "Kommando '%s' eingereiht (%u/8)",
             command.c_str(), static_cast<unsigned>(commands_.size()));
    return true;
  }

  void queue_firmware_read() { firmware_read_pending_ = true; }

  void poll() {
    if (poll_rt_pending_ || poll_ic_pending_ || firmware_read_pending_ ||
        (state_ != State::IDLE && active_kind_ != Kind::ACTION)) {
      ESP_LOGD(TAG, "Poll uebersprungen (laeuft bereits)");
      return;
    }
    poll_rt_pending_ = poll_ic_pending_ = true;
    ESP_LOGD(TAG, "Poll angefordert (RT + IC)");
  }

  void loop() {
    uint32_t now = millis();

    for (size_t count = 0; count < 128 && available(); ++count) {
      uint8_t raw = read();
      last_rx_ms_ = millis();
      if (state_ == State::IDLE || response_complete_) {
        continue;
      }
      if ((raw & 0xDBu) != 0xDBu) {
        this->finish_(false, "ungueltige UART-Kodierung");
        return;
      }
      rx_byte_ = static_cast<uint8_t>(
          rx_byte_ | ((((raw >> 2) & 1u) << rx_shift_) |
                      (((raw >> 5) & 1u) << (rx_shift_ + 1))));
      rx_shift_ += 2;
      if (rx_shift_ == 8) {
        response_.push_back(static_cast<char>(rx_byte_));
        rx_shift_ = 0;
        rx_byte_ = 0;
        const size_t n = response_.size();
        if (n > 512) {
          this->finish_(false, "Antwort zu lang");
          return;
        }
        if (n >= 2 && response_[n - 2] == '\r' && response_[n - 1] == '\n') {
          response_.resize(n - 2);
          response_complete_ = true;
        }
      }
    }

    now = millis();
    if (state_ == State::IDLE) {
      if (available() || static_cast<uint32_t>(now - last_rx_ms_) < 100 ||
          static_cast<uint32_t>(now - finished_ms_) < recovery_ms_) {
        return;
      }
      if (!commands_.empty()) {
        std::string command = commands_.front();
        commands_.pop_front();
        this->start_(command, Kind::ACTION, now);
      } else if (poll_rt_pending_) {
        poll_rt_pending_ = false;
        this->start_("RT:0000", Kind::COUNTERS, now);
      } else if (poll_ic_pending_) {
        poll_ic_pending_ = false;
        this->start_("IC:", Kind::STATUS, now);
      } else if (firmware_read_pending_) {
        firmware_read_pending_ = false;
        this->start_("TY:", Kind::FIRMWARE, now);
      }
      return;
    }

    if (state_ == State::SENDING) {
      if (static_cast<uint32_t>(now - started_ms_) >= 5000) {
        this->finish_(false, "Sende-Timeout");
        return;
      }
      if (tx_pos_ != 0 && static_cast<uint32_t>(now - last_tx_ms_) < 8) {
        return;
      }
      if (tx_pos_ < tx_.size()) {
        const uint8_t value = static_cast<uint8_t>(tx_[tx_pos_++]);
        uint8_t encoded[4];
        for (unsigned i = 0; i < 4; ++i) {
          encoded[i] = static_cast<uint8_t>(
              0xDBu | (((value >> (2 * i)) & 1u) << 2) |
              (((value >> (2 * i + 1)) & 1u) << 5));
        }
        write_array(encoded, sizeof(encoded));
        last_tx_ms_ = millis();
        return;
      }
      state_ = State::WAITING;
      response_started_ms_ = now;
      ESP_LOGD(TAG, "TX fertig, warte auf Antwort");
    }

    if (response_complete_) {
      this->finish_(true, "");
    } else if (static_cast<uint32_t>(now - response_started_ms_) >= 5000) {
      this->finish_(false, "Antwort-Timeout");
    }
  }

 protected:
  enum class State { IDLE, SENDING, WAITING };
  enum class Kind { ACTION, COUNTERS, STATUS, FIRMWARE };

  enum CounterIndex : size_t {
    IDX_ESPRESSO = 0,
    IDX_DOUBLE_ESPRESSO = 1,
    IDX_COFFEE = 2,
    IDX_DOUBLE_COFFEE = 3,
    IDX_CAPPUCCINO = 4,
    IDX_LATTE = 5,
    IDX_DOUBLE_RISTRETTO = 6,
    IDX_BREW_CYCLES = 7,
    IDX_CLEAN = 8,
    IDX_DESCALE = 9,
    IDX_GROUNDS = 14,
  };

  static const char *kind_name_(Kind kind) {
    switch (kind) {
      case Kind::ACTION: return "Aktion";
      case Kind::COUNTERS: return "Zaehler";
      case Kind::STATUS: return "Status";
      case Kind::FIRMWARE: return "Firmware";
    }
    return "?";
  }

  void start_(const std::string &command, Kind kind, uint32_t now) {
    active_command_ = command;
    active_kind_ = kind;
    tx_ = command + "\r\n";
    tx_pos_ = 0;
    response_.clear();
    response_complete_ = false;
    rx_shift_ = 0;
    rx_byte_ = 0;
    started_ms_ = now;
    state_ = State::SENDING;
    ESP_LOGI(TAG, "TX %s [%s]", command.c_str(), kind_name_(kind));
  }

  static bool valid_hex_payload_(const std::string &value, const char *prefix,
                                 size_t min_length) {
    if (value.size() < min_length) {
      return false;
    }
    for (size_t i = 0; i < 3; ++i) {
      char a = value[i];
      char b = prefix[i];
      if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
      if (a != b) return false;
    }
    for (size_t i = 3; i < value.size(); ++i) {
      const char c = value[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'))) {
        return false;
      }
    }
    return true;
  }

  static bool valid_firmware_payload_(const std::string &value) {
    if (value.size() < 4) return false;
    return value[0] == 't' || value[0] == 'T';
  }

  void finish_(bool success, const char *reason) {
    if (success && active_kind_ == Kind::COUNTERS) {
      success = valid_hex_payload_(response_, "rt:", 7) &&
                ((response_.size() - 3) % 4 == 0);
    } else if (success && active_kind_ == Kind::STATUS) {
      success = valid_hex_payload_(response_, "ic:", 7);
    } else if (success && active_kind_ == Kind::FIRMWARE) {
      success = valid_firmware_payload_(response_);
    }

    if (!success) {
      ESP_LOGW(TAG, "RX Fehler bei %s: %s (Antwort '%s', %u Bytes)",
               active_command_.c_str(),
               reason[0] ? reason : "unerwartete Antwort",
               response_.c_str(), static_cast<unsigned>(response_.size()));
      this->publish_event_(std::string("RX-Fehler: ") + active_command_);
    } else {
      ESP_LOGI(TAG, "RX '%s' [%s]", response_.c_str(),
               kind_name_(active_kind_));
    }

    if (!success) {
      // Fehler bereits geloggt.
    } else if (active_kind_ == Kind::COUNTERS) {
      this->handle_counters_();
    } else if (active_kind_ == Kind::STATUS) {
      this->handle_status_();
    } else if (active_kind_ == Kind::FIRMWARE) {
      const std::string fw = response_.substr(3);
      if (firmware_ != nullptr) firmware_->publish_state(fw);
      ESP_LOGI(TAG, "Firmware/Modell: %s", fw.c_str());
    } else {
      if (active_command_ == "HZ:" && hz_ != nullptr) {
        hz_->publish_state(response_);
      } else if (active_command_ == "CS:" && cs_ != nullptr) {
        cs_->publish_state(response_);
      }
      ESP_LOGD(TAG, "%s Antwort: %s", active_command_.c_str(),
               response_.c_str());
    }

    state_ = State::IDLE;
    finished_ms_ = millis();
    recovery_ms_ = success ? 100 : 500;
  }

  static void publish_counter_(sensor::Sensor *s, const std::vector<long> &c,
                               size_t idx) {
    if (s != nullptr && idx < c.size()) {
      s->publish_state(static_cast<float>(c[idx]));
    }
  }

  static void publish_text_(text_sensor::TextSensor *s, const std::string &v) {
    if (s != nullptr) s->publish_state(v);
  }

  void handle_counters_() {
    std::vector<long> c;
    for (size_t pos = 3; pos + 4 <= response_.size(); pos += 4) {
      c.push_back(std::strtol(response_.substr(pos, 4).c_str(), nullptr, 16));
    }

    publish_counter_(espresso_, c, IDX_ESPRESSO);
    publish_counter_(double_espresso_, c, IDX_DOUBLE_ESPRESSO);
    publish_counter_(coffee_, c, IDX_COFFEE);
    publish_counter_(double_coffee_, c, IDX_DOUBLE_COFFEE);
    publish_counter_(cappuccino_, c, IDX_CAPPUCCINO);
    publish_counter_(latte_, c, IDX_LATTE);
    publish_counter_(double_ristretto_, c, IDX_DOUBLE_RISTRETTO);
    publish_counter_(brew_cycles_, c, IDX_BREW_CYCLES);
    publish_counter_(clean_, c, IDX_CLEAN);
    publish_counter_(descale_, c, IDX_DESCALE);
    publish_counter_(grounds_, c, IDX_GROUNDS);

    this->publish_counter_changes_(c);
  }

  void publish_counter_changes_(const std::vector<long> &current) {
    if (!last_counters_initialized_) {
      last_counters_ = current;
      last_counters_initialized_ = true;
      return;
    }
    if (counter_changes_ == nullptr) {
      last_counters_ = current;
      return;
    }

    std::string msg;
    char buf[40];
    const size_t max_n =
        last_counters_.size() > current.size() ? last_counters_.size()
                                               : current.size();
    for (size_t i = 0; i < max_n; ++i) {
      const long prev = i < last_counters_.size() ? last_counters_[i] : -1;
      const long now = i < current.size() ? current[i] : -1;
      if (prev == now) continue;
      if (!msg.empty()) msg += ", ";
      std::snprintf(buf, sizeof(buf), "counter_%u %ld>%ld",
                    static_cast<unsigned>(i + 1), prev, now);
      msg += buf;
    }
    if (!msg.empty()) {
      counter_changes_->publish_state(msg);
      ESP_LOGD(TAG, "Zaehleraenderung: %s", msg.c_str());
    }
    last_counters_ = current;
  }

  static void byte_to_bits_(uint8_t v, char out[9]) {
    for (int i = 7; i >= 0; --i) {
      out[7 - i] = (v & (1u << i)) ? '1' : '0';
    }
    out[8] = '\0';
  }

  void publish_ic_bits_(uint8_t a, uint8_t b, uint8_t c, bool has_c) {
    if (ic_bits_initialized_ && a == last_ic_a_ && b == last_ic_b_ &&
        (!has_c || c == last_ic_c_)) {
      return;
    }
    char abits[9];
    char bbits[9];
    char cbits[9];
    char buf[40];
    byte_to_bits_(a, abits);
    byte_to_bits_(b, bbits);
    byte_to_bits_(c, cbits);
    if (has_c) {
      std::snprintf(buf, sizeof(buf), "A=%s B=%s C=%s", abits, bbits, cbits);
    } else {
      std::snprintf(buf, sizeof(buf), "A=%s B=%s", abits, bbits);
    }
    publish_text_(ic_bits_, std::string(buf));
    last_ic_a_ = a;
    last_ic_b_ = b;
    last_ic_c_ = c;
    ic_bits_initialized_ = true;
    ESP_LOGD(TAG, "IC-Bits: %s", buf);
  }

  void publish_event_(const std::string &message) {
    ESP_LOGI(TAG, "Ereignis: %s", message.c_str());
    if (event_ != nullptr) event_->publish_state(message);
  }

  void publish_power_switch_() {
    if (power_switch_ != nullptr) power_switch_->publish_state(powered_);
  }

  void apply_power_state_(bool powered) {
    if (powered_ != powered) {
      this->publish_event_(powered ? "Power: AN" : "Power: AUS");
    }
    powered_ = powered;
    this->publish_power_switch_();
  }

  void handle_status_() {
    const uint8_t a = static_cast<uint8_t>(
        std::strtol(response_.substr(3, 2).c_str(), nullptr, 16));
    const uint8_t b = static_cast<uint8_t>(
        std::strtol(response_.substr(5, 2).c_str(), nullptr, 16));

    // Neuere Modelle (u. a. Z2) liefern ein drittes Eingangsbyte C,
    // das bisher nicht ausgewertet wurde.
    const std::string payload = response_.substr(3);
    const bool has_c = payload.size() >= 6;
    const uint8_t c = has_c
                          ? static_cast<uint8_t>(std::strtol(
                                payload.substr(4, 2).c_str(), nullptr, 16))
                          : 0;

    const bool changed = !ic_bits_initialized_ || a != last_ic_a_ ||
                         b != last_ic_b_ || (has_c && c != last_ic_c_);

    // Verifiziert per Trigger-Test (2026-11-14):
    //   A-Bit4 = 1 -> Auffangschale fehlt
    //   B-Bit5 = 1 -> Wassertank fehlt/leer
    //   B-Bit4 = 1 -> Spuel-/Pflegeanforderung
    //   B-Bit7 = 1 -> Maschine antwortet
    //   B-Bit0 spiegelt B-Bit4 -> ignorieren
    // Nicht ausgewertet, weil lokal nicht bestaetigt: A-Bit2, B-Bit3, B-Bit6, C
    const bool tray_missing = ((a >> 4) & 1u) != 0;
    const bool tank_missing = ((b >> 5) & 1u) != 0;
    const bool rinse_request = ((b >> 4) & 1u) != 0;

    if (changed) {
      char abits[9];
      char bbits[9];
      char cbits[9];
      byte_to_bits_(a, abits);
      byte_to_bits_(b, bbits);
      byte_to_bits_(c, cbits);
      ESP_LOGI(TAG,
               "Status IC A=%s B=%s C=%s | Schale=%s Tank=%s Spuelen=%s",
               abits, bbits, has_c ? cbits : "-",
               tray_missing ? "fehlt" : "ok",
               tank_missing ? "leer" : "ok",
               rinse_request ? "angefordert" : "nein");
    } else {
      ESP_LOGD(TAG, "Status unveraendert (A=0x%02X B=0x%02X)", a, b);
    }

    this->publish_ic_bits_(a, b, c, has_c);

    publish_text_(tray_, tray_missing ? "Fehlt" : "OK");
    publish_text_(tank_, tank_missing ? "Leer" : "OK");

    if (!powered_) {
      publish_text_(status_, "Ausgeschaltet");
      return;
    }

    std::string st = "Bereit";
    if (tray_missing) {
      st = "Auffangschale fehlt oder voll";
    } else if (tank_missing) {
      st = "Wassertank leer";
    } else if (rinse_request) {
      st = "Spuelung erforderlich";
    }
    publish_text_(status_, st);
  }

  sensor::Sensor *espresso_{nullptr};
  sensor::Sensor *double_espresso_{nullptr};
  sensor::Sensor *coffee_{nullptr};
  sensor::Sensor *double_coffee_{nullptr};
  sensor::Sensor *cappuccino_{nullptr};
  sensor::Sensor *latte_{nullptr};
  sensor::Sensor *double_ristretto_{nullptr};
  sensor::Sensor *brew_cycles_{nullptr};
  sensor::Sensor *clean_{nullptr};
  sensor::Sensor *descale_{nullptr};
  sensor::Sensor *grounds_{nullptr};

  text_sensor::TextSensor *tray_{nullptr};
  text_sensor::TextSensor *tank_{nullptr};
  text_sensor::TextSensor *status_{nullptr};
  text_sensor::TextSensor *firmware_{nullptr};
  text_sensor::TextSensor *ic_bits_{nullptr};
  text_sensor::TextSensor *hz_{nullptr};
  text_sensor::TextSensor *cs_{nullptr};
  text_sensor::TextSensor *counter_changes_{nullptr};
  text_sensor::TextSensor *event_{nullptr};

  State state_{State::IDLE};
  Kind active_kind_{Kind::ACTION};
  std::deque<std::string> commands_;
  bool poll_rt_pending_{false};
  bool poll_ic_pending_{false};
  bool firmware_read_pending_{false};
  std::string active_command_;
  std::string tx_;
  std::string response_;
  size_t tx_pos_{0};
  uint8_t rx_byte_{0};
  uint8_t rx_shift_{0};
  bool response_complete_{false};
  bool powered_{false};
  switch_::Switch *power_switch_{nullptr};
  uint32_t last_rx_ms_{0};
  uint32_t finished_ms_{0};
  uint32_t recovery_ms_{100};
  uint32_t started_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t response_started_ms_{0};

  std::vector<long> last_counters_;
  bool last_counters_initialized_{false};
  uint8_t last_ic_a_{0};
  uint8_t last_ic_b_{0};
  uint8_t last_ic_c_{0};
  bool ic_bits_initialized_{false};
};

}  // namespace jura_z2
}  // namespace esphome

esphome::jura_z2::JuraZ2 *g_jura = nullptr;

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>

namespace esphome {
namespace jura_z2 {

static const char *const TAG = "jura_z2";

class JuraZ2 : public uart::UARTDevice {
 public:
  explicit JuraZ2(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}

  void set_tray_sensor(text_sensor::TextSensor *s) { tray_ = s; }
  void set_tank_sensor(text_sensor::TextSensor *s) { tank_ = s; }
  void set_status_sensor(text_sensor::TextSensor *s) { status_ = s; }
  void set_state_raw_sensor(text_sensor::TextSensor *s) { state_raw_ = s; }
  void set_firmware_sensor(text_sensor::TextSensor *s) { firmware_ = s; }
  void set_ic_bits_sensor(text_sensor::TextSensor *s) { ic_bits_ = s; }

  void set_power_switch(switch_::Switch *s) { power_switch_ = s; }

  void set_powered(bool powered) {
    state_known_ = false;
    power_command_active_ = true;
    power_command_target_ = powered;
    power_command_ms_ = millis();
    this->apply_power_state_(powered);
  }

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
    if (poll_state_pending_ || poll_ic_pending_ || firmware_read_pending_ ||
        (state_ != State::IDLE && active_kind_ != Kind::ACTION)) {
      return;
    }
    poll_state_pending_ = true;
    // IC (Schale/Tank/Spuelhinweis) reicht alle 10 s; RR:1B laeuft alle 2 s.
    if (++poll_ic_divider_ >= 5) {
      poll_ic_divider_ = 0;
      poll_ic_pending_ = true;
    }
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
      } else if (poll_state_pending_) {
        poll_state_pending_ = false;
        this->start_("RR:1B", Kind::POWER_STATE, now);
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
  enum class Kind { ACTION, STATUS, POWER_STATE, FIRMWARE };

  static const char *kind_name_(Kind kind) {
    switch (kind) {
      case Kind::ACTION: return "Aktion";
      case Kind::STATUS: return "Status";
      case Kind::POWER_STATE: return "Betriebszustand";
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
    if (success && active_kind_ == Kind::STATUS) {
      success = valid_hex_payload_(response_, "ic:", 7);
    } else if (success && active_kind_ == Kind::POWER_STATE) {
      success = valid_hex_payload_(response_, "rr:", 7);
    } else if (success && active_kind_ == Kind::FIRMWARE) {
      success = valid_firmware_payload_(response_);
    }

    if (!success) {
      if (active_kind_ == Kind::POWER_STATE) {
        // RR:1B ist bei aelteren Modellen evtl. nicht vorhanden. Nicht alle
        // 2 s einen Warnhinweis spammen; der Rohzustand zeigt es an.
        state_known_ = false;
        if (state_raw_ != nullptr) state_raw_->publish_state("keine Antwort");
        ESP_LOGD(TAG, "RR:1B keine Antwort (%s)", reason);
      } else {
        ESP_LOGW(TAG, "RX Fehler bei %s: %s (Antwort '%s', %u Bytes)",
                 active_command_.c_str(),
                 reason[0] ? reason : "unerwartete Antwort",
                 response_.c_str(), static_cast<unsigned>(response_.size()));
        this->publish_event_(std::string("RX-Fehler: ") + active_command_);
      }
    } else {
      ESP_LOGI(TAG, "RX '%s' [%s]", response_.c_str(),
               kind_name_(active_kind_));
    }

    if (!success) {
      // Fehler bereits geloggt.
    } else if (active_kind_ == Kind::STATUS) {
      this->handle_status_();
    } else if (active_kind_ == Kind::POWER_STATE) {
      this->handle_power_state_();
    } else if (active_kind_ == Kind::FIRMWARE) {
      const std::string fw = response_.substr(3);
      if (firmware_ != nullptr) firmware_->publish_state(fw);
      ESP_LOGI(TAG, "Firmware/Modell: %s", fw.c_str());
    } else {
      ESP_LOGD(TAG, "%s Antwort: %s", active_command_.c_str(),
               response_.c_str());
    }

    state_ = State::IDLE;
    finished_ms_ = millis();
    recovery_ms_ = success ? 100 : 500;
  }

  static void publish_text_(text_sensor::TextSensor *s, const std::string &v) {
    if (s != nullptr) s->publish_state(v);
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
    this->publish_status_();
  }

  void publish_status_() {
    std::string st;

    if (state_known_) {
      switch (machine_state_) {
        case 0x0000: st = "Aus"; break;
        case 0x1000: st = "Bereit"; break;
        case 0x1001: st = "Spült"; break;
        case 0x1002: st = "Reinigt"; break;
        case 0x1008: st = "Brüht"; break;
        default: {
          char buf[24];
          std::snprintf(buf, sizeof(buf), "Zustand 0x%04X", machine_state_);
          st = buf;
          break;
        }
      }
      // Laufende Vorgaenge nicht durch Schale/Tank ueberschreiben.
      if (machine_state_ == 0x0000 || machine_state_ == 0x1001 ||
          machine_state_ == 0x1002 || machine_state_ == 0x1008) {
        publish_text_(status_, st);
        return;
      }
    } else {
      st = powered_ ? "Bereit" : "Aus";
    }

    if (tray_missing_) {
      st = "Auffangschale fehlt oder voll";
    } else if (tank_missing_) {
      st = "Wassertank leer";
    } else if (rinse_request_) {
      st = "Spülung erforderlich";
    }
    publish_text_(status_, st);
  }

  void handle_power_state_() {
    const std::string raw = response_.substr(3);
    if (state_raw_ != nullptr) state_raw_->publish_state(raw);
    const unsigned long value =
        std::strtoul(raw.substr(0, 4).c_str(), nullptr, 16);
    const uint16_t state = static_cast<uint16_t>(value & 0xFFFFu);

    if (power_command_active_) {
      const uint32_t age = millis() - power_command_ms_;
      // Nach AN:01 kann RR:1B noch kurz 0000 liefern, bis die Maschine wach ist.
      if (power_command_target_ && state == 0x0000 && age < 15000) {
        ESP_LOGD(TAG, "RR:1B noch 0000 nach Power-ON (%.1f s)",
                 age / 1000.0f);
        return;
      }
      // Nach AN:02 kann RR:1B noch kurz einen An-Zustand liefern.
      if (!power_command_target_ && state != 0x0000 && age < 5000) {
        ESP_LOGD(TAG, "RR:1B noch 0x%04X nach Power-OFF (%.1f s)", state,
                 age / 1000.0f);
        return;
      }
      power_command_active_ = false;
    }

    machine_state_ = state;
    state_known_ = true;
    const bool on = machine_state_ != 0x0000;
    ESP_LOGI(TAG, "RR:1B Zustand 0x%04X (%s)", machine_state_,
             on ? "an" : "aus");
    this->apply_power_state_(on);
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
    tray_missing_ = ((a >> 4) & 1u) != 0;
    tank_missing_ = ((b >> 5) & 1u) != 0;
    rinse_request_ = ((b >> 4) & 1u) != 0;

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
               tray_missing_ ? "fehlt" : "ok",
               tank_missing_ ? "leer" : "ok",
               rinse_request_ ? "angefordert" : "nein");
    } else {
      ESP_LOGD(TAG, "Status unveraendert (A=0x%02X B=0x%02X)", a, b);
    }

    this->publish_ic_bits_(a, b, c, has_c);

    publish_text_(tray_, tray_missing_ ? "Fehlt" : "OK");
    publish_text_(tank_, tank_missing_ ? "Leer" : "OK");
    this->publish_status_();
  }

  text_sensor::TextSensor *tray_{nullptr};
  text_sensor::TextSensor *tank_{nullptr};
  text_sensor::TextSensor *status_{nullptr};
  text_sensor::TextSensor *state_raw_{nullptr};
  text_sensor::TextSensor *firmware_{nullptr};
  text_sensor::TextSensor *ic_bits_{nullptr};
  text_sensor::TextSensor *event_{nullptr};

  State state_{State::IDLE};
  Kind active_kind_{Kind::ACTION};
  std::deque<std::string> commands_;
  bool poll_state_pending_{false};
  bool poll_ic_pending_{false};
  uint8_t poll_ic_divider_{0};
  bool firmware_read_pending_{false};
  std::string active_command_;
  std::string tx_;
  std::string response_;
  size_t tx_pos_{0};
  uint8_t rx_byte_{0};
  uint8_t rx_shift_{0};
  bool response_complete_{false};
  bool powered_{false};
  bool state_known_{false};
  bool power_command_active_{false};
  bool power_command_target_{false};
  uint32_t power_command_ms_{0};
  uint16_t machine_state_{0};
  bool tray_missing_{false};
  bool tank_missing_{false};
  bool rinse_request_{false};
  switch_::Switch *power_switch_{nullptr};
  uint32_t last_rx_ms_{0};
  uint32_t finished_ms_{0};
  uint32_t recovery_ms_{100};
  uint32_t started_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t response_started_ms_{0};

  uint8_t last_ic_a_{0};
  uint8_t last_ic_b_{0};
  uint8_t last_ic_c_{0};
  bool ic_bits_initialized_{false};
};

}  // namespace jura_z2
}  // namespace esphome

esphome::jura_z2::JuraZ2 *g_jura = nullptr;

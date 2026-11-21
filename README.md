# Jura Z2 – ESPHome-Komponente mit WebUI

Diese Version baut auf [Henrik-Aalto/esphome-jura-component](https://github.com/Henrik-Aalto/esphome-jura-component) auf (ursprünglich von [ryanalden](https://github.com/ryanalden/esphome-jura-component)) und ist auf die **Jura Z2** mit ESP8266/D1 mini zugeschnitten.

## Funktionen

- Ein/Aus über Home Assistant (`AN:01` / `AN:02`)
- **Spülen auf Knopfdruck** (`FA:02`) – bewusst **kein automatisches Spülen**
- Bezüge: Espresso, Kaffee, Heißwasser, Milchschaum, Latte Macchiato
- Status: Auffangschale, Wassertank, Spül-/Pflegeanforderung, Bereit
- Zähler aus dem EEPROM (`RT:`)
- WebUI mit Status, Zählern, Diagnose, Live-Log und freier Kommandozeile

## Warum kein automatisches Spülen?

Die Maschine schaltet sich selbst ab. Steht dann ein Glas unter dem linken Auslauf, sorgt ein automatisch ausgelöstes Spülen für eine Sauerei. Deshalb wird **nur manuell** gespült. Das Ausschalten nutzt `AN:02` (ohne Spülen); `FA:01` würde beim Ausschalten spülen.

## Befehle

Die `FA`-Codes sind **modellabhängig**. Die Defaults unten stammen aus der bisherigen Z2-Konfiguration. Beim ersten Test bitte prüfen und bei Bedarf die Substitutions in `jura_z2.yaml` anpassen.

| Funktion | Substitution | Default | Hinweis |
| --- | --- | --- | --- |
| Einschalten | `cmd_power_on` | `AN:01` | |
| Ausschalten | `cmd_power_off` | `AN:02` | ohne Spülen; `FA:01` = aus inkl. Spülen |
| Spülen | `cmd_rinse` | `FA:02` | manuell; auf manchen Modellen heizt `FA:02` nur Wasser auf |
| Espresso | `cmd_espresso` | `FA:04` | |
| Kaffee | `cmd_coffee` | `FA:05` | |
| Heißwasser | `cmd_hot_water` | `FA:06` | |
| Milchschaum | `cmd_milk_foam` | `FA:07` | |
| Latte Macchiato | `cmd_latte` | `FA:08` | |

Für eigene Tests gibt es die Text-Entity **„Jura Kommando“**: Befehl eingeben (z. B. `FA:09`), wird direkt gesendet. Alternativ in den Substitutions ändern und neu flashen.

## Hardware

Wemos D1 mini am 7-poligen Jura-Serviceport über einen 3,3-V-↔-5-V-Pegelwandler. Der D1 mini wird von der Jura versorgt. Bei Problemen TX/RX tauschen.

```
Jura Serviceport          D1 mini
   TX  ── Level Shifter ── RX (GPIO2)
   RX  ── Level Shifter ── TX (GPIO14)
   GND ────────────────── GND
   5V  ────────────────── 5V
```

Die Pins sind in `jura_z2.yaml` über `jura_tx_pin` / `jura_rx_pin` konfigurierbar.

## Installation

1. `credentials.example.yaml` nach `secrets.yaml` kopieren und ausfüllen:
   ```bash
   cp credentials.example.yaml secrets.yaml
   ```
2. Kompilieren und flashen:
   ```bash
   esphome run jura_z2.yaml
   ```
3. In Home Assistant über die ESPHome-Integration hinzufügen.

`use_address` ist absichtlich nicht gesetzt; das Gerät ist über mDNS als `jura-coffee.lan` erreichbar. Bei DHCP-Wechseln empfiehlt sich eine DHCP-Reservierung im Router.

## WebUI

Nach dem Flashen: `http://jura-coffee.lan` (oder die aktuelle IP).

Die Seite zeigt:

- Power-Schalter und Status (Bereit / Ausgeschaltet / Schale / Tank / Spülen)
- Bezugs-Buttons und „Spülen“
- Zähler (Espresso, Kaffee, Reinigungen, Kaffeesatz usw.)
- Diagnose (`IC`-Bits, `HZ`, `CS`, Firmware/Modell, Ereignisse)
- Live-Log und freies Kommandofeld

## Verifizierte Statusbits

Per Trigger-Test an einer Z2 bestätigt:

| Bit | Bedeutung |
| --- | --- |
| `A`-Bit4 = 1 | Auffangschale fehlt/voll |
| `B`-Bit5 = 1 | Wassertank fehlt/leer |
| `B`-Bit4 = 1 | Spül-/Pflegeanforderung |
| `RT`-Wort 14 | Kaffeesatz-Zähler (sprang beim Entnehmen der Schale von 8 auf 0) |

Nicht ausgewertet, weil nicht bestätigt: `A`-Bit2, `B`-Bit3, `B`-Bit6, `C`. `B`-Bit3 ist **kein** Standby-Bit.

## Grenzen

- Bohnen- und Filtermeldungen sind nicht lesbar; es werden keine geratenen Werte angezeigt.
- Es gibt keinen echten Power-Rückkanal. Der Schalter ist **optimistisch**: `AN:01`/`AN:02` werden gesendet, der Zustand wird lokal geführt.
- Standby bzw. „Display aus“ ist nicht erkennbar.
- Die `FA`-Codes für Bezüge können je nach Z2-Firmware/Modell abweichen.
- Kein automatisches Spülen (siehe oben).

## Legacy: Jura Impressa J6

Die ursprüngliche J6-Implementierung bleibt erhalten:

- `jura_coffee.h`, `jura_coffee.yaml`
- `generate_esphome_jura_yaml.py` (erzeugt UART-Sequenzen, benötigt `bitarray`)

Bekannte J6-Befehle (Software `TY: PIM V01.01`, `TL: LOADER V3.0`):

| Befehl | Aktion |
| --- | --- |
| `AN:01` | Einschalten |
| `AN:02` | Ausschalten |
| `FA:01` | Ausschalten inkl. Spülen |
| `FA:02` | Wasser aufheizen |
| `FA:03` | Wasser aufheizen & Dampf |
| `FA:06` | Heißwasser |
| `FA:07` | 1 Espresso |
| `FA:08` | 2 Espressi |
| `FA:09` | 1 Kaffee |
| `FA:0A` | 2 Kaffees |
| `FA:0C` | Menü (RINSE zuerst) |
| `FA:0D` / `FA:0E` | Menü-Zyklus |

Die J6-Version nutzt einen „Optimistic / Assumed State“-Template-Switch, da der echte Power-Zustand nicht auslesbar ist.

## Credits

- Ursprüngliches Projekt: [ryanalden/esphome-jura-component](https://github.com/ryanalden/esphome-jura-component)
- Dieses Fork-Repository: [Henrik-Aalto/esphome-jura-component](https://github.com/Henrik-Aalto/esphome-jura-component)
- Protokoll-Infos: [hn/jura-coffee-machine](https://github.com/hn/jura-coffee-machine) sowie die Diskussionen im Home-Assistant-Forum.
- Modellabhängige `FA`-Codes: eigene Tests an der Z2 sowie Berichte zu F9/J5.

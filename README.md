# Jura Z2 – ESPHome-Basisversion mit WebUI

Diese Version baut auf [Henrik-Aalto/esphome-jura-component](https://github.com/Henrik-Aalto/esphome-jura-component) auf (ursprünglich von [ryanalden/esphome-jura-component](https://github.com/ryanalden/esphome-jura-component)) und ist auf die **Jura Z2** mit ESP8266/D1 mini zugeschnitten.

Die Basisversion konzentriert sich auf Ein/Aus, getrennte Pflegeprogramme und ein zuschaltbares Log. Bezugs-Buttons und Zähler sind bewusst **nicht** enthalten.

## Funktionen

- Ein/Aus über Home Assistant (`AN:01` / `AN:02`)
- **Echter Powerstatus** über `RR:1B` (Polling alle 2 s) statt optimistischem Schalter
- Fünf getrennte Pflegeprogramme: Spülen, Milchsystem spülen, Ausschaltspülung, Reinigen, Entkalken
- Status: Auffangschale, Wassertank, Spül-/Pflegeanforderung, Bereit
- WebUI mit Power, Pflege-Buttons, Diagnose, Live-Log (zuschaltbar) und freier Kommandozeile
- **Kein automatisches Spülen**
- Keine Zähler und keine Bezugs-Buttons in dieser Basisversion

## Warum kein automatisches Spülen?

Die Maschine schaltet sich selbst ab. Steht dann ein Glas unter dem linken Auslauf, sorgt ein automatisch ausgelöstes Spülen für eine Sauerei. Deshalb wird **nur manuell** gespült. Das Ausschalten nutzt `AN:02` (ohne Spülen); `FA:01` würde beim Ausschalten spülen.

## Powerstatus über `RR:1B`

Die Komponente fragt alle 2 Sekunden `RR:1B` ab. Bekannte Zustände aus dem F90-Code:

| Antwort | Bedeutung |
| --- | --- |
| `rr:0000` | aus |
| `rr:1000` | an / bereit |
| `rr:1001` | spült |
| `rr:1002` | reinigt |
| `rr:1008` | brüht |

Nach einem Power-Befehl gibt es eine kurze Schonfrist: Nach `AN:01` wird ein noch gemeldetes `0000` bis zu 15 s ignoriert, nach `AN:02` ein noch gemeldeter An-Zustand bis zu 5 s. Das verhindert ein kurzes Zurückspringen des Schalters.

Falls die Z2 `RR:1B` nicht beantwortet, zeigt der Rohzustand `keine Antwort`; der Schalter bleibt beim zuletzt gesendeten Zustand. Es wird nicht alle 2 s eine Warnung geloggt.

## Befehle

Die `FA`-Codes sind **modellabhängig**. Die Defaults unten stammen aus der bisherigen Z2-Konfiguration und aus dem F90-Code. Beim ersten Test bitte prüfen und bei Bedarf die Substitutions in `jura_z2.yaml` anpassen.

| Funktion | Substitution | Default | Hinweis |
| --- | --- | --- | --- |
| Einschalten | `cmd_power_on` | `AN:01` | Weckwirkung bei ausgeschalteter Z2 nicht bestätigt |
| Ausschalten | `cmd_power_off` | `AN:02` | ohne Spülen; `FA:01` = aus inkl. Spülen |
| Spülen | `cmd_rinse` | `FA:02` | manuell; auf manchen Modellen heizt `FA:02` nur Wasser auf |
| Milchsystem spülen | `cmd_milk_rinse` | `FA:0C` | modellabhängig |
| Ausschaltspülung | `cmd_off_rinse` | `FA:0E` | konfliktbehaftet: alte Z2-YAML = Ausschaltspülung, F90 = Kaffee |
| Reinigen | `cmd_clean` | `FA:0D` | modellabhängig |
| Entkalken | `cmd_descale` | `FA:1F` | modellabhängig |

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
- Fünf Pflege-Buttons
- Diagnose (`IC`-Bits, Firmware, Rohzustand, Ereignisse)
- Live-Log, das per Schalter wirklich gestartet/gestoppt wird
- Freies Kommandofeld

## Verifizierte Statusbits

Per Trigger-Test an einer Z2 bestätigt:

| Bit | Bedeutung |
| --- | --- |
| `A`-Bit4 = 1 | Auffangschale fehlt/voll |
| `B`-Bit5 = 1 | Wassertank fehlt/leer |
| `B`-Bit4 = 1 | Spül-/Pflegeanforderung |

Nicht ausgewertet, weil nicht bestätigt: `A`-Bit2, `B`-Bit3, `B`-Bit6, `C`. `B`-Bit3 ist **kein** Standby-Bit. `B`-Bit7 = 1 bedeutet nur, dass die Maschine antwortet.

## Grenzen

- Bohnen- und Filtermeldungen sind nicht lesbar; es werden keine geratenen Werte angezeigt.
- `RR:1B` ist auf der Z2 noch nicht live verifiziert. Ohne Antwort bleibt der Schalter beim zuletzt gesendeten Zustand.
- Standby bzw. „Display aus“ ist nicht erkennbar.
- Die `FA`-Codes für Pflegeprogramme können je nach Z2-Firmware/Modell abweichen.
- `FA:0E` ist nicht eindeutig belegt.
- Kein automatisches Spülen (siehe oben).
- Keine Zähler und keine Bezugs-Buttons in dieser Basisversion.

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
- `RR:1B`-Zustände: F90-Code aus der openHAB-Community.
- Modellabhängige `FA`-Codes: eigene Tests an der Z2 sowie Berichte zu F9/J5.

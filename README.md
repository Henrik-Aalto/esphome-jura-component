# Jura Z2 – ESPHome-Basisversion mit WebUI

Diese Version baut auf [Henrik-Aalto/esphome-jura-component](https://github.com/Henrik-Aalto/esphome-jura-component) auf (ursprünglich von [ryanalden/esphome-jura-component](https://github.com/ryanalden/esphome-jura-component)) und ist auf die **Jura Z2** mit ESP8266/D1 mini zugeschnitten.

Die Basisversion konzentriert sich auf Ein/Aus, getrennte Pflegeprogramme und ein zuschaltbares Log. Bezugs-Buttons und Zähler sind bewusst **nicht** enthalten.

## Funktionen

- Ein/Aus über Home Assistant (`AN:01` / `AN:02`)
- **Echter Powerstatus** über `HZ:` (Polling alle 2 s) statt optimistischem Schalter
- Fünf getrennte Pflegeprogramme: Spülen, Milchsystem spülen, Ausschaltspülung, Reinigen, Entkalken
- Status: Auffangschale, Wassertank, Spül-/Pflegeanforderung, Bereit
- WebUI mit Power, Pflege-Buttons, Autospülen-Schaltern, Diagnose, Live-Log (zuschaltbar) und freier Kommandozeile
- Autospülen beim Ausschalten getrennt für links (Kaffeeauslauf) und Mitte (Milchsystem) schaltbar
- Keine Zähler und keine Bezugs-Buttons in dieser Basisversion

## Autospülen beim Ausschalten

Vor `AN:02` werden die ausgewählten Spülungen in die Befehlswarteschlange gelegt:

| Schalter | Default | Befehl | Auslauf |
| --- | --- | --- | --- |
| Autospülen links | **aus** | `FA:02` | linker Kaffeeauslauf |
| Autospülen Mitte | **an** | `FA:0C` | Milchsystem / Mitte |

Links ist absichtlich standardmäßig aus: Steht ein Glas unter dem linken Auslauf, kann die Spülung spritzen. Wenn links gespült werden soll, vorher ein Glas unterstellen und den Schalter im WebUI einschalten. Die Mitte ist standardmäßig an, weil dort nichts im Weg steht.

## Powerstatus über `HZ:`

Die Komponente fragt alle 2 Sekunden `HZ:` ab. Auf der Z2 bleibt `RR:1B` auch bei ausgeschalteter Maschine unverändert (`B7EB`); `HZ:` ändert sich dagegen:

| Zustand | `HZ`-Erstfeld | Bit 6 |
| --- | --- | --- |
| an | `110000100000` | 0 |
| aus | `110001100000` | 1 |

Die Firmware wertet Bit 6 des ersten HZ-Feldes aus: gesetzt = aus/Standby, nicht gesetzt = an. Nach einem Power-Befehl gibt es eine Schonfrist: Nach `AN:01` wird ein noch gemeldetes „aus“ bis zu 20 s ignoriert, nach `AN:02` ein noch gemeldetes „an“ bis zu 30 s. Das verhindert ein kurzes Zurückspringen des Schalters, besonders beim Ausschalten mit vorheriger Spülung.

Falls die Z2 `HZ:` nicht beantwortet, zeigt der Rohzustand `keine Antwort`; der Schalter bleibt beim zuletzt gesendeten Zustand. Es wird nicht alle 2 s eine Warnung geloggt.

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

- Power-Schalter und Status (Bereit / Aus / Schale / Tank / Spülen)
- Autospülen-Schalter für links und Mitte
- Fünf Pflege-Buttons
- Diagnose (`IC`-Bits, Firmware, HZ-Rohzustand, Ereignisse)
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
- `HZ:`-Bit 6 wurde an der Z2 bei aus/on beobachtet; die Firmware mit dieser Auswertung ist noch nicht geflasht.
- Ohne HZ-Antwort bleibt der Schalter beim zuletzt gesendeten Zustand.
- Die `FA`-Codes für Pflegeprogramme können je nach Z2-Firmware/Modell abweichen.
- `FA:0E` ist nicht eindeutig belegt.
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
- `HZ:`-Power-Bit: eigene Messung an der Z2 bei aus- und eingeschalteter Maschine.
- Modellabhängige `FA`-Codes: eigene Tests an der Z2 sowie Berichte zu F9/J5.

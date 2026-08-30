# SXServo TODO

Stand: in Arbeit (Servo-Bildansicht V2 + Telemetrie-Sync)

## Statusübersicht
- [x] SX-Bit-Verarbeitung auf sequenzielles Schalten umgestellt (ein Servo pro Schritt)
- [x] Serial-Setup-Grundfunktion eingebaut (s, +/- , Schrittweiten, L/R, Save/Abort)
- [x] Serial-Setup mit gemeinsamer Wizard-Logik gehärtet (Validierung + ACK)
- [x] SX-Wizard-Protokoll in Firmware ergänzt (K10..K15)
- [x] SX-Monitor-Qt auf Wizard-Protokoll K10..K15 umgebaut
- [x] End-to-End Testplan (Serial + SX-Monitor) dokumentiert und praktisch verifiziert
- [x] SX-Wizard robust gemacht (Session/K15/K1-Guards, Servo-Lock, Store-Plausibilisierung)
- [x] Qt-ACK/Telemetrie stabilisiert (gedrosselte Move-Verifikation, CFG-Import-Schutz, vollständige CFG-Prüfung)
- [x] QT-Rückmeldung für lokalen Prog-Tastendruck ergänzt (`PROG_STATUS active=...`, V2-Statuslabel)
- [x] Qt-V2-Protokoll wird zusätzlich in Dateien geschrieben (`~/SXServo-Logs/sxservo_qt_v2_latest.log` + Zeitstempel-Log)
- [x] V2 Gleisindikator gehärtet (SX0/SX1 Umschaltung zeigt sofort bekannten Trackstatus; unbekannt wird als `?` dargestellt)
- [x] `Progmodus anfordern` blockiert bei `Track=1` jetzt hart (Warnung + kein K10-Start)
- [x] Serial-Setup `s` kann SX-Adressen setzen: `p <0..111>` = Adresse A, `o <0..111>` = Adresse B, `0` deaktiviert, beide 0 verboten
- [ ] Optional: Servo-Relax (PWM nach Stellvorgang aus) gegen Haltestrom

## Implementiert
Firmware (SX30-ServoDecoder/src/main.cpp)
- K10 CMD: 1 Start, 2 Abort, 3 Save+Ende
- K11 Servoindex 0..15
- K12 Schrittweite 1/2/5/10/20
- K13 Move: 1=-, 2=+, 3=Mitte
- K14 Store: 1=L speichern, 2=R speichern
- K15 ACK: 0 idle, 1 ok, 2 error, 3 busy
- Serial- und SX-Setup greifen auf denselben Setup-Zustand zu.
- SX-Setup und Serial-Setup gegeneinander entkoppelt (robuster Betrieb)
- Versionsausgabe in Firmware aktiv (derzeit: 2026-06-26-SerialSxAddr)

GUI (sx-monitor-qt/sx_monitor_qt.cpp)
- Servo-Programmer auf Wizard-Bedienung umgestellt.
- Statusanzeige von K8 auf K15 umgestellt (busunabhängig sichtbar).
- K10/K13/K14 werden als Impuls (Wert -> 0) gesendet für saubere Flankenerkennung.

## Verifizierter Bedienablauf (Hardware)
1) Qt starten und mit SX verbinden.
2) Richtigen Bus wählen (im aktuellen Aufbau meist SX1).
3) Setup START auslösen.
4) Servo wählen (K11), Schrittweite setzen (K12).
5) Mit + / - Endlagen anfahren.
6) Linke Endlage speichern (K14=1), rechte Endlage speichern (K14=2).
7) Setup SAVE+ENDE (K10=3).
8) Im Betrieb über Fahrdaten 0/1 die gespeicherten Endlagen anfahren.

## Nächste Schritte
1. Hardwaretest: lokalen Prog-Taster drücken und in Qt V2 `PROG_STATUS active=1/0` + Statuslabel prüfen.
2. Optional Servo-Relax als schaltbare Option ergänzen.
3. Optional kurze Inbetriebnahme-Notiz ins README übernehmen.
4. Feldtest: mehrfache Langläufe mit korrekter SX-Adresse (K1=20) zur finalen Abnahme (keine Fremd-Servo-Zuckungen, stabile ACKs).

## QA Testablauf (3 Programmierwege + Telemetrie)
0) Voraussetzungen
- Hardware: SX-Bus/SLX852 aktiv, Arduino mit aktueller FW, Servo an bekanntem Kanal (z.B. S1)
- Software: Qt V2 neu gebaut, SerialMonitor verfügbar
- Ports klar zugeordnet: SLX-Port und Arduino-Telemetrie-Port

1) Firmware-/Telemetrie-Schnellcheck
- Arduino-Serial (115200) öffnen, Reset auslösen
- Erwartet: `HELLO decoder=servodecoder fw=... proto=1`
- `t` senden -> HELLO erneut
- `c` senden -> `CFG_HDR`, 16x `CFG_S`, `CFG_END`

2) Weg A: SerialMonitor-Setup (Taste `s`)
- `s` senden
- Servo wählen (`n`/`v`), Mitte (`0`), bewegen (`+`/`-`), Schrittweite (`1/2/5/a/b`)
- Endlagen speichern (`l`/`r`), Save (`w`)
- Erwartet: bisheriger Ablauf intakt + `ACK_SETUP_*` Telemetriezeilen
- Persistenz: Reset, dann `c` -> gespeicherte relMin/relMax prüfen

3) Weg B: SX-Bus Wizard (K10..K15)
- Start: K10=1 (Impuls)
- Servo: K11, Schritt: K12
- Move: K13=1/2/3 (Impulse), Store: K14=1/2 (Impulse)
- Save: K10=3 (Impuls)
- Erwartet: K15-ACK korrekt, Servo reagiert reproduzierbar, Serial zeigt `src=sx` ACK-Zeilen

4) Weg C: Qt Servo-Bildansicht V2
- Verbinden, V2 öffnen, Limit auf ±40
- Mitte, dann 10x `+`, danach 1x `++`
- Bis Grenze fahren links/rechts
- Erwartet: `+/-` = 1 Schritt, `++/--` = 10 Schritte, Armbewegung konsistent, an Grenze Stop/Log
- Speichern, Reset, `c`-Dump gegenprüfen

5) Versions-/Decoder-Kompatibilität GUI
- Mit passender FW: keine Warnung
- Mit absichtlich alter/falscher FW (falls verfügbar): klare Warnung „Firmwareupdate nötig"

6) Regression Fahrbetrieb
- Nach Setup normale Fahrdaten 0/1 senden
- Erwartet: Servo fährt gespeicherte Endlagen zuverlässig an

7) Abnahmekriterien
- Alle 3 Programmierwege funktionsfähig
- EEPROM-Werte konsistent (`c`-Dump)
- V2-Klick -> Bewegung reproduzierbar
- Versions-/Decodercheck zuverlässig
- Keine Blocker in Logs/Serial

## Laufender Status
- 2026-05-04: Todo.md erstellt
- 2026-05-04: Firmware K10..K15 + Qt-Wizard umgebaut
- 2026-05-05: Wizard-Flanken/Impulslogik stabilisiert, End-to-End erfolgreich getestet
- 2026-05-05: Offener Punkt ergänzt: QT soll lokalen Prog-Tastendruck sichtbar melden
- 2026-05-06: Servo-Bildansicht V2 erweitert: Buttons --/-/Mitte/+/++, feste Schrittlogik (-/+ = 1, --/++ = 10), V2-Limit ±30..45 (Default 40), Armbewegung an effektive Schritte gekoppelt.
- 2026-05-06: Wizard-Move-Impulszeiten in Qt verlängert (0->move->0), um Klick-Aussetzer zu reduzieren.
- 2026-05-06: Firmware-Telemetrie ergänzt (ohne Bruch der bestehenden Wege):
  - HELLO decoder=servodecoder fw=2026-05-07a proto=1
  - ACK_SETUP_MOVE / ACK_SETUP_STATE / ACK_SETUP_STORE
  - CFG-Dump via c/C (CFG_HDR, CFG_S..., CFG_END)
  - HELLO via t/T
  - Build SX30-ServoDecoder erfolgreich.
- 2026-05-06: Aktueller Zwischenstand: Qt-Telemetrie-Port + Parser + FW-Kompatibilitätswarnung in Arbeit. Ziel: GUI auf Arduino-ACK/CFG synchronisieren, bestehende Programmierwege (SX-Bus und Serial 's') unverändert nutzbar halten.
- 2026-05-07: Qt V2 ACK-Pending/Timeout pro Servo ergänzt: nach V2 MOVE/STORE wird `ACK:pending` am Tile angezeigt; bei `ACK_SETUP_MOVE`/`ACK_SETUP_STATE action=mid`/`ACK_SETUP_STORE` auf `ACK:ok`; ohne passendes ACK nach 900ms auf `ACK:timeout` inkl. Logeintrag.
- 2026-05-08: V2-Klickpfad für besseres Bediengefühl synchronisiert: GUI bewegt Armposition nicht mehr „blind", sondern wartet auf Arduino-ACK; `++/--` senden jetzt echte Mehrfach-Schritte (bis zu 10 Einzelimpulse, limitiert durch V2-Limit), damit sichtbare/akustische Bewegung zum Klick passt.
- 2026-05-08: V2-Queue/BUSY/Timeout verfeinert: pro Servo Move-Queue mit ACK-gesteuerter Schritt-für-Schritt-Abarbeitung ergänzt (auch bei schnellem Klicken), dynamischer ACK-Timeout pro Servo eingebaut, Queue-Status im Tile-Titel sichtbar (`Q:+/-n`).
- 2026-05-12: Lokaler Prog-Taster bekommt eigene Telemetrie `PROG_STATUS active=1/0 source=local_button track=... led=...`; Qt V2 wertet diese Zeile aus und zeigt Progstatus/D13 im V2-Statuslabel an. Nach Fehlerdiagnose ergänzt: Bei `PROG_STATUS active=1 source=local_button` sendet Qt im Serial-Wizard automatisch `s`, damit der Decoder wirklich in den Servo-Setup-Wizard wechselt; Firmware `2026-05-12b` beendet dabei den klassischen Modul-Programmierstatus (`programming=false`). Qt-Build erfolgreich; Firmware-Build lokal nicht möglich, weil PlatformIO/pio in dieser Shell fehlt.
- 2026-05-12: Qt V2 schreibt das Änderungsprotokoll zusätzlich dauerhaft nach `~/SXServo-Logs/sxservo_qt_v2_latest.log` (immer aktueller Lauf, überschrieben) und in ein Zeitstempel-Log `~/SXServo-Logs/sxservo_qt_v2_YYYYMMDD_HHMMSS.log`. Headless-Test hat Datei erzeugt und Start-/Logpfad-Zeilen geschrieben.
- 2026-06-26: Firmware `2026-06-26-SerialSxAddr`: Serial-Setup `s` um SX-Adressprogrammierung erweitert. `p <0..111>` setzt Adresse A, `o <0..111>` setzt Adresse B; `0` deaktiviert eine Hälfte, beide 0 werden abgelehnt. PlatformIO-Build erfolgreich.


1) Ergebnis:  *  Executing task: platformio device monitor 

--- Terminal on /dev/ttyUSB0 | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H

SX30 ServoDecoder start
FW-Version: SX30-ServoDecoder 2026-05-07a
HELLO decoder=servodecoder fw=2026-05-07a proto=1
CFG: aus EEPROM geladen
Setup starten: 's' senden

SX30 ServoDecoder start
FW-Version: SX30-ServoDecoder 2026-05-07a
HELLO decoder=servodecoder fw=2026-05-07a proto=1
CFG: aus EEPROM geladen
Setup starten: 's' senden
RX:t
HELLO decoder=servodecoder fw=2026-05-07a proto=1
RX:c
CFG_HDR decoder=servodecoder fw=2026-05-07a sxA=20 sxB=0
CFG_S servo=1 zero=90 relMin=-90 relMax=25 divLeft=0
CFG_S servo=2 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=3 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=4 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=5 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=6 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=7 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=8 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=9 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=10 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=11 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=12 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=13 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=14 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=15 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_S servo=16 zero=90 relMin=-40 relMax=40 divLeft=0
CFG_END


2) Ergebnis: 
RX:s

=== SERVO SETUP ===
n = naechster Servo
v = voriger Servo
0 = Mitte (rel 0)
l = linken Anschlag aus aktueller Position speichern
r = rechten Anschlag aus aktueller Position speichern
1/2/5/a/b = Schrittweite 1/2/5/10/20
+ = nach rechts, - = nach links (um Schrittweite)
w = alles speichern und Setup beenden
x = Setup ohne Speichern beenden

Servo S1 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=1 rel=0

Servo S2 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=2 rel=0

Servo S3 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=3 rel=0

Servo S4 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=4 rel=0

Servo S5 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=5 rel=0

Servo S6 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=6 rel=0

Servo S7 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=7 rel=0

Servo S8 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=8 rel=0

Servo S9 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=9 rel=0

Servo S16 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=16 rel=0

Servo S1 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=1 rel=0

Servo S2 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=2 rel=0

Servo S3 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=3 rel=0

Servo S4 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=4 rel=0

Servo S5 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=5 rel=0

Servo S6 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=6 rel=0

Servo S7 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=7 rel=0

Servo S8 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=8 rel=0

Servo S9 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=9 rel=0

Servo S10 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=10 rel=0

Servo S11 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=11 rel=0

Servo S12 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=12 rel=0

Servo S13 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=13 rel=0

Servo S14 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=14 rel=0

Servo S16 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=16 rel=0

Servo S1 aktiv, Mitte angefahren (rel 0).
ACK_SETUP_STATE src=core action=select servo=1 rel=0

Aufgefallen: bei n tippen spring t er von S9 zu s16

Fazit: Lass uns da nochmal einen sepataten Test und QA machen

3) machen wir später. hier brauche ich eine Anleitung nach Selectrix stiel
4) Ergebnis: Auffällig: ist der Servo überhaupt mit qt über SerialBus verbunden?
++ geht, aber servo springt zurück
mache mehr ++ servo immer zurück nach 0... nach mehrmaligem ++ keine ++ mehr möglich
dann solange -- bis ich links angekommen bin. Servo in echt ist korrekt mitgelaufen, wieder ++ geht bis 0 erreicht. Servo läuft mit, danach ++ , servo immer zurück nach 0

5) - 7 
Später


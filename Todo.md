# SXServo TODO

Stand: abgeschlossen (Wizard stabil)

## Statusübersicht
- [x] SX-Bit-Verarbeitung auf sequenzielles Schalten umgestellt (ein Servo pro Schritt)
- [x] Serial-Setup-Grundfunktion eingebaut (s, +/- , Schrittweiten, L/R, Save/Abort)
- [x] Serial-Setup mit gemeinsamer Wizard-Logik gehärtet (Validierung + ACK)
- [x] SX-Wizard-Protokoll in Firmware ergänzt (K10..K15)
- [x] SX-Monitor-Qt auf Wizard-Protokoll K10..K15 umgebaut
- [x] End-to-End Testplan (Serial + SX-Monitor) dokumentiert und praktisch verifiziert
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
- Versionsausgabe in Firmware aktiv (derzeit: 2026-05-04f)

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
1. Optional Servo-Relax als schaltbare Option ergänzen.
2. Optional kurze Inbetriebnahme-Notiz ins README übernehmen.

## Laufender Status
- 2026-05-04: Todo.md erstellt
- 2026-05-04: Firmware K10..K15 + Qt-Wizard umgebaut
- 2026-05-05: Wizard-Flanken/Impulslogik stabilisiert, End-to-End erfolgreich getestet

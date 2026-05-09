# Serial-Wizard Schnelltest (SXServo Qt V2)

## Ziel
Servo-Programmierung stabil über den Arduino-Serialpfad (statt SX-Wizard-Rückkanal).

## Voraussetzungen
- Telemetrie-Port verbunden (bevorzugt `/dev/serial/by-id/...A50285BI...`)
- 115200 Baud
- In Qt: Programmierweg = **Serial-Wizard (empfohlen)**

## Minimalablauf (S1, Adresse 20)
1. `c` senden und Ausgangszustand notieren (`CFG_S servo=1 ...`).
2. Setup Start drücken.
3. 2x `+` oder `-` bewegen.
4. `Store L` oder `Store R` drücken.
5. Setup Ende drücken.
6. `c` senden und Änderung prüfen.

## Erwartete Log-Muster
- `SER TX(start): s`
- `SER TX(move): +|-|0`
- `SER TX(store): l|r`
- `ACK_SETUP_MOVE src=serial ...`
- `ACK_SETUP_STORE src=serial ...`

## PASS-Kriterium
- `CFG_S servo=1 relMin/relMax` hat sich wie erwartet geändert.

## Hinweise
- Wenn keine `ACK_SETUP_* src=serial` erscheinen: Telemetrie-Port prüfen.
- Nicht parallel `platformio device monitor` offen lassen (Port exklusiv).
- Für reproduzierbare Endlagen lieber wenige, gezielte Schrittbewegungen statt lange Queue-Läufe.

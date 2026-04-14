# Verdrahtung (Kurzfassung)

## System
- Basisplatine: `SXV0300_Servo`
- Aufsatz: `Servo` (PCA9685)
- Beide zusammen als Gesamtsystem betrachten.

## Wichtige Punkte
1. I2C am Pro Mini:
   - `A4 = SDA`
   - `A5 = SCL`
2. Auf der Basisplatine muss der Jumper `A5_RX` auf **A5** stehen (nicht RX/D0).
3. PCA9685-Adresse ist im Testsketch auf `0x40` eingestellt.
4. Gemeinsame Masse ist Pflicht:
   - GND Logik und GND Servo-Versorgung müssen verbunden sein.
5. Servo-Versorgung:
   - Lastseite mit eigener +5V-Versorgung betreiben.
   - SX-Bus/Logikpfad nicht mit hoher Servolast belasten.

## Servo-Ausgänge
- 16 Kanäle über den PCA9685.
- Im Aufsatz sind die PWM-Signale auf den Ausgangsstecker geführt (16 Servokanäle).

## Schnelltest
1. `software/ServoTest/ServoTest.ino` flashen
2. Serielle Konsole auf 115200 öffnen
3. Befehle:
   - `h` Hilfe
   - `s` Scan I2C
   - `a <winkel>` alle Kanäle
   - `c <kanal> <winkel>` einzelner Kanal
   - `d` Demo

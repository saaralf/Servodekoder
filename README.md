# Servodekoder (OpenSX)

Dieses Repository enthält:
- Hardware
  - `hardware/Servo.sch` / `hardware/Servo.brd` (Servo-Aufsatz mit PCA9685)
  - `hardware/SXV0300_Servo.sch` / `hardware/SXV0300_Servo.brd` (Basisplatine v300 Servo)
- Software
  - `software/ServoTest/ServoTest.ino` (Arduino-Testsketch für PCA9685)

## Annahmen / Pinning
- PCA9685 Adresse fest: `0x40` (A0..A5 auf GND)
- I2C vom Pro Mini:
  - A4 = SDA
  - A5 = SCL
- Basisplatine: A5_RX-Jumper muss auf **A5** stehen (nicht RX)

## Versorgungskonzept
- Externe Servo-Versorgung wird über die Basisplatine eingespeist.
- Servo-Aufsatz führt +5V über MOSFET-Schutzpfad.
- GND zwischen Logik und Servo muss gemeinsam sein.

## Test
- Sketch hochladen: `software/ServoTest/ServoTest.ino`
- Serielle Konsole 115200 öffnen
- Befehle im Sketch: `h`, `s`, `a <winkel>`, `c <kanal> <winkel>`, `d`

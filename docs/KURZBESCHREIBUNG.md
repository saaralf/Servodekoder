# Servodekoder – kurze Beschreibung

Dieses Projekt kombiniert eine OpenSX-Basisplatine mit einem Servo-Aufsatz (PCA9685), um bis zu 16 Servokanäle anzusteuern.

- Basisplatine: `SXV0300_Servo`
- Aufsatz: `Servo` (PCA9685)
- Testsoftware: `software/ServoTest/ServoTest.ino`

Wichtig: Der Jumper `A5_RX` auf der Basisplatine muss auf **A5** stehen, damit I2C-SCL korrekt verbunden ist.

## Bild Aufsatz
![Servo-Aufsatz (PCA9685)](images/servo-aufsatz.jpg)

## Bild Basisplatine
![Basisplatine v2.0](images/basisplatine-v2.0.jpg)

## Bildquellen
- Aufsatzbild (lokale Quelle): `/home/michael/Dokumente/Selectrix/ServoAufsatz/assets/image.jpg`
- Basisplatine (lokale Quelle): `/home/michael/opensx_hardware/Basisplatine-2.0/basisplatine-v2.0.jpg`

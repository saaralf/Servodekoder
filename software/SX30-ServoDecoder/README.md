# SX30-ServoDecoder (Basisplatine 3.0 + ServoAufsatz)

Dieses Arduino/PlatformIO-Projekt steuert 16 Servos über den SX-Bus mit der SX30-Library.

Code-Datei:
- `src/main.cpp`

## Funktionen
- 16 Servos über PCA9685 (Adresse 0x40)
- 2 SX-Adressen für Fahrbetrieb:
  - `sxAddrA` steuert Servo 0..7 (Bit 0..7)
  - `sxAddrB` steuert Servo 8..15 (Bit 0..7)
- Bit-Bedeutung im Fahrbetrieb:
  - `0 = Gerade`
  - `1 = Abzweig`
- Pro Servo in EEPROM gespeichert:
  - Nullpunkt (`zeroPhys`)
  - Limits (`relMin`, `relMax`)
  - Abzweig links/rechts (`divergingIsLeft`)

## Programmiermodus (wie OpenSX-Muster)
Start: Track AUS + Programmiertaste
Ende: Track EIN oder Taste erneut

Während Programmiermodus nutzt der Sketch diese SX-Kanäle:
- Kanal 1: neue SX-Adresse A
- Kanal 2: neue SX-Adresse B
- Kanal 3: Abzweigseite Servo 0..7 (Bit=1 links, Bit=0 rechts)
- Kanal 4: Abzweigseite Servo 8..15 (Bit=1 links, Bit=0 rechts)

Beim Beenden werden diese Werte übernommen und im EEPROM gespeichert.

## Wichtige Defaults
- Standardadressen: 72 und 73
- Standardlimits pro Servo: -40 .. +40
- Nullpunkt pro Servo: physisch 90

Hinweis: Für die individuellen Limits/Nullpunkte pro Servo ist üblicherweise ein separates Kalibrier-Tool sinnvoll (z. B. dein Testprogramm). Dieses Fahrprogramm übernimmt die gespeicherten Werte und läuft dann autark.

# Dual-Backend Migration (SX + RMX) für Daemon/Qt

## Ziel
Ein gemeinsamer Socket-Daemon mit Backend-Modus:
- SX: SX0/SX1 lesen/schreiben
- RMX: RMX0/RMX1 lesen/schreiben

Qt v2 soll pro Verbindung den Modus auswählen und passend senden/anzeigen.

## Einheitliche Socket-API
- READADR <bus> <adr>
- WRITE <bus> <adr> <val>
- GET_TRACK
- SNAPSHOT

## Backend-spezifische Umsetzung
### SX
- 19200 8N1
- FE A0 Init
- Read/Write klassisch 2-Byte
- Polling 77ms, change-only Broadcast
- Track über ADR127/Power-Kanal

### RMX
- 57600 8N2
- Frameformat 0x7D len opcode ... xor
- READSX opcode 0x06 (bus 0/1)
- WRITESX opcode 0x05 (bus 0/1)
- Track über RMX-Status/Powerrückmeldung (Fallback via ADR127-Read in SX1 falls nötig)

## Qt v2 Änderungen
- Neues Dropdown: Protokoll [SX, RMX]
- Sendebus dynamisch:
  - SX -> SX0/SX1
  - RMX -> RMX0/RMX1
- Gleisanzeige berücksichtigt aktives Backend
- bestehende Wizard-Funktionen bleiben, senden über generische sendBusValue()

## Testmatrix
1. SX backend + SX0/SX1 read/write adr20/126
2. RMX backend + RMX0/RMX1 read/write adr20/126
3. Track an/aus Anzeige für beide Backends
4. Snapshot + Liveframes im Monitor

## Akzeptanz
- Monitor kann beide Backends ohne Neustart des Qt-Binaries bedienen
- WRITE/READ roundtrip für bus0 und bus1 in beiden Backends
- Gleisstatus erkennbar und im UI sichtbar

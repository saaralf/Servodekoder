# RMX vs SX (ADR126/SX1) Vergleichstest

## Ziel
Nachweis, wie sich Adresse 126 (Bit7) auf verschiedenen Interfaces verhält:
- RMX-Protokoll (SX1 explizit im Frame)
- SX/SLX-2-Byte-Pfad (Rautenhaus-Modus/FE A0)

## Testprogramm
- Quelle: `software/sx-bus-core/tests/selectrix_compare_modes.c`
- Binary lokal: `selectrix_compare_modes`

Modi:
- `rmx-sx1`  -> RMX-Frame-Protokoll, Bus=1 (SX1), 57600 8N2
- `slx-bus1` -> FE A0 + aktiver Bus über `FE 01`, dann 2-Byte-SX, 19200 8N1
- `sx-raw`   -> FE A0 + 2-Byte-SX ohne explizite Busumschaltung, 19200 8N1

## Build
```bash
gcc -O2 -Wall -Wextra -o /home/michael/selectrix_compare_modes software/sx-bus-core/tests/selectrix_compare_modes.c
```

## Beispiele
```bash
/home/michael/selectrix_compare_modes /dev/serial/by-id/usb-rautenhaus_digital_RMX950USB-if00-port0 rmx-sx1
/home/michael/selectrix_compare_modes /dev/serial/by-id/usb-FTDI_USB_Serial_Converter_FTF8NBF0-if00-port0 slx-bus1
/home/michael/selectrix_compare_modes /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG02SG7M-if00-port0 sx-raw
```

## Beobachtung (reproduziert)
1) RMX950 (rmx-sx1)
- `READ adr126 BEFORE`: je nach Zustand 0x00 oder 0x80
- `WRITE adr126=0x80`: ACK/OK
- `READ adr126 AFTER`: stabil 0x80

2) FTDI FTF8NBF0 (SX-Pfad)
- Readback auf adr126 bleibt stabil 0x00 oder liefert TIMEOUT (laufabhängig)
- Kein RMX-typisches 0x80-Readback-Verhalten

3) FTDI BG02SG7M (SX-Pfad)
- je nach Lauf TIMEOUT oder 0x00
- ebenfalls kein RMX-typisches 0x80-Readback-Verhalten

## Schlussfolgerung für Qt/Daemon
- **SX-Pfad:** aktiv pollen (13 Hz, alle relevanten Adressen), nicht auf automatische Übertragung verlassen.
- **RMX-Pfad:** ereignisgetriebene Datenlieferung nutzen (Frames vom Interface).
- **ADR126:** Semantik im RMX-Pfad entspricht Erwartung (Read/Write Bit7), im SX-2-Byte-Pfad nicht gleich.

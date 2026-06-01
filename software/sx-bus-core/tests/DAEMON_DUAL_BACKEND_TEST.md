# Daemon Dual-Backend Test (SX + RMX)

## Artefakte
- Daemon Quelle: `software/sx-bus-core/sx_bus_daemon_dual.c`
- Daemon Binary: `software/sx-bus-core/sx_bus_daemon_dual`

## Build
```bash
gcc -O2 -Wall -Wextra -o software/sx-bus-core/sx_bus_daemon_dual \
  software/sx-bus-core/sx_bus_daemon_dual.c \
  software/sx-bus-core/sx_bus_core.c
```

## Start
SX:
```bash
software/sx-bus-core/sx_bus_daemon_dual \
  /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG02SG7M-if00-port0 \
  19200 /tmp/sxbusd_sx.sock sx
```

RMX:
```bash
software/sx-bus-core/sx_bus_daemon_dual \
  /dev/serial/by-id/usb-rautenhaus_digital_RMX950USB-if00-port0 \
  57600 /tmp/sxbusd_rmx.sock rmx
```

## Socket-Kommandos
- `READADR <bus> <adr>`
- `WRITE <bus> <adr> <val>`
- `GET_TRACK`
- `SNAPSHOT`

## Live-Test (durchgeführt)
### RMX backend
- READADR 0 20 -> `FRAME 0 20 0` + `OK`
- READADR 0 126 -> `FRAME 0 126 0` + `OK`
- GET_TRACK -> `TRACK 0`
- WRITE 0 126 128 -> `FRAME 0 126 128` + `OK`
- READADR 0 126 -> `FRAME 0 126 128` + `OK`

### SX backend
- READADR 0 20 -> `FRAME 0 20 0` + `OK`
- READADR 0 126 -> `FRAME 0 126 0` + `OK`
- GET_TRACK -> `TRACK 0`
- WRITE 0 126 128 -> `FRAME 0 126 128` + `OK`
- READADR 0 126 -> `FRAME 0 126 0` + `OK`

## Ergebnis
- Beide Interfaces werden gefunden und über denselben Daemon-Befehlssatz bedient.
- RMX zeigt erwartetes ADR126-Readback (0x80 nach Write).
- SX zeigt bekanntes ADR126-Verhalten (Readback bleibt 0x00 trotz Write-OK).

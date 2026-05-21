# SX Bus Core (ohne Qt)

Ziel
- Selectrix/SLX852 Kommunikation in eine eigenständige C-Komponente auslagern.
- Qt nutzt später nur noch diese Komponente (Adapter), keine eigene Protokolllogik.

Inhalt
- libsxbus.a: Protokollkern (open/configure/read frame/track state)
- sx_bus_probe: CLI-Testtool ohne Qt

Status
- initiale Struktur angelegt

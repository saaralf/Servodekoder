# SX-Monitor-Qt: Voraussetzungen und Build

Diese Datei beschreibt die minimalen Abhängigkeiten und den Build-Ablauf für `sx_monitor_qt`.

## 1) Voraussetzungen

## Betriebssystem
- Linux (getestet unter Debian/Ubuntu-ähnlichen Systemen)

## Compiler / Build-Tools
- `g++`
- `cmake` (>= 3.16 empfohlen)
- `make` oder `ninja`
- `pkg-config`

## Qt
- Qt6 Core
- Qt6 Widgets
- Qt6 SerialPort

Unter Debian/Ubuntu typischerweise:
- `qt6-base-dev`
- `qt6-serialport-dev`

## XKB-Hinweis
Beim CMake-Konfigurieren kann erscheinen:
- `Could NOT find XKB ...`

Das ist in vielen Fällen nur eine Warnung. Falls es auf deinem System zum Fehler wird, zusätzlich installieren:
- `libxkbcommon-dev`
- ggf. `libxkbcommon-x11-dev`

## 2) Build

In den Projektordner wechseln:

```bash
cd /opt/programme/selectrix/Servodekoder/software/sx-monitor-qt
```

Build-Verzeichnis erzeugen und konfigurieren:

```bash
cmake -S . -B build
```

Kompilieren:

```bash
cmake --build build -j
```

Programm starten:

```bash
./build/sx_monitor_qt
```

## 3) Kurzer Funktionstest

1. SX-Interface verbinden (korrekter Port/Baud im Tool wählen).
2. Auf `Connect` klicken.
3. In der Programmiereinheit Bus passend wählen (meist SX1).
4. `Setup Start` drücken.
5. `+` / `-` testen, ob im Log K13-Impulse sichtbar sind.

## 4) Typische Fehler

## CMake läuft, Build aber fehlt
- Nach `cmake -S . -B build` immer auch `cmake --build build -j` ausführen.

## Qt-Module nicht gefunden
- Prüfen, ob `qt6-base-dev` und `qt6-serialport-dev` installiert sind.

## Keine Reaktion im Betrieb
- Bus (SX0/SX1) prüfen.
- Richtige Firmware-Version auf Decoder prüfen.
- Prüfen, ob der richtige SX-Adapter verbunden ist.

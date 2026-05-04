# SX Monitor QT V2 – Zielbild

Ziel für V2 (bildliche Bedienung):
- 16 Servos gleichzeitig sichtbar (2 Reihen à 8)
- Pro Servo eine visuelle Darstellung mit Servoarm
- Pro Servo direkte Bedienelemente:
  - `-` / `+`
  - `Mitte`
  - `Links speichern`
  - `Rechts speichern`
- Einfache Klick-Bedienung ohne Tabellenfokus

## Stand
- V2-Projektordner angelegt (`sx-monitor-qt-v2`)
- Eigenes Binary/Target: `sx_monitor_qt_v2`
- Build-Konfiguration getrennt von V1

## Build
```bash
cd /opt/programme/selectrix/Servodekoder/software/sx-monitor-qt-v2
cmake -S . -B build
cmake --build build -j
./build/sx_monitor_qt_v2
```

## Nächster Implementierungsschritt
- Neue V2-Tabseite mit 16 Servo-Kacheln (2x8) implementieren
- Jede Kachel schickt K11/K12/K13/K14-Impulse analog bestehendem Wizard
- Visuelle Armstellung lokal in der Kachel anzeigen und bei Klick aktualisieren

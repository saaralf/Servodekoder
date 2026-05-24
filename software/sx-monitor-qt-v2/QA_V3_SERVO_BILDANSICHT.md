# QA-Anleitung: SX-Monitor Qt V3 – Servo-Bildansicht V2

Ziel: prüfen, ob der V3-Reiter „Servo-Bildansicht V2“ den originalen V2-Servo-Bildreiter korrekt sichtbar macht und die grundlegende Bedienung sicher funktioniert.

Stand: V3-Port, Fokus auf Bild-Reiter mit 16 Servos, Original-Servoarm-Assets und V2-ähnlicher Bedienstruktur.

## 1. Vorbereitung

Arbeitsverzeichnis:

```bash
cd /opt/programme/selectrix/Servodekoder/software/sx-monitor-qt-v2
```

Build prüfen:

```bash
cmake --build build -j4 --target sx_monitor_qt_v3
```

Erwartung:
- Build endet ohne Fehler.
- Ziel `sx_monitor_qt_v3` wird gebaut.

Programm starten:

```bash
./build/sx_monitor_qt_v3
```

Hinweis:
- Für reine Sichtprüfung ist keine angeschlossene Hardware zwingend nötig.
- Für RX-/Servo-Liveprüfung ist der SX-Daemon bzw. das reale Interface nötig.

## 2. Sichtprüfung Hauptfenster

Prüfen:
- Fenster startet ohne Absturz.
- Tabs sind sichtbar.
- Kein Zwang zum Scrollen im Hauptbereich, soweit Bildschirmhöhe ausreicht.
- Tab „Servo-Bildansicht V2“ ist vorhanden.

Bewertung:
- PASS: Tab sichtbar, UI stabil, keine Fehlermeldung.
- FAIL: Tab fehlt, Fenster startet nicht, Layout unbenutzbar.
- N/A: Bildschirm zu klein; dann Beobachtung notieren.

## 3. Servo-Bildansicht öffnen

Aktion:
- Tab „Servo-Bildansicht V2“ öffnen.

Erwartung:
- 16 Servo-Kacheln sichtbar.
- Obere Reihe: Servo 1 bis Servo 8.
- Untere Reihe: Servo 9 bis Servo 16.
- Zwischen den Reihen ist die Adresszeile für Adresse 2 sichtbar.
- Servo-Grafik mit blauem Body und Servoarm wird angezeigt.

Bewertung:
- PASS: 16 Kacheln und Servo-Grafiken sichtbar.
- FAIL: Grafiken fehlen, Kacheln fehlen oder Programm stürzt ab.
- N/A: Wenn Assets nicht gefunden werden, Pfad notieren.

## 4. Asset-Prüfung

Prüfen, ob diese Dateien vorhanden sind:

```bash
ls -l /opt/programme/selectrix/Servodekoder/software/sx-monitor-qt-v2/assets/servo_body_blue.png
ls -l /opt/programme/selectrix/Servodekoder/software/sx-monitor-qt-v2/assets/servo_arm_new.png
```

Erwartung:
- Beide Dateien existieren.

Bewertung:
- PASS: beide PNG-Dateien vorhanden.
- FAIL: eine oder beide Dateien fehlen.

## 5. Layout-Vergleich gegen V2

V2 starten, falls benötigt:

```bash
./build/sx_monitor_qt_v2
```

Vergleichen:
- Tab-Aufbau Servo-Bildansicht
- 2 Reihen mit je 8 Servos
- Adresse 1 oben
- Adresse 2 zwischen den Servo-Reihen
- Limit-Zeile
- Buttons pro Servo:
  - `--`
  - `-`
  - `Mitte`
  - `+`
  - `++`
  - `Links speichern`
  - `Rechts speichern`

Bewertung:
- PASS: V3 entspricht optisch und strukturell weitgehend V2.
- FAIL: zentrale V2-Bedienelemente fehlen.
- N/A: Wenn V2 nicht gestartet werden kann, Screenshot/Beobachtung von V3 notieren.

## 6. Adressfelder prüfen

Aktionen:
1. Im Servo-Bildtab Adresse 1 ändern.
2. Adresse 2 ändern.
3. Checkbox „Bits links->rechts (Bit1 links)“ umschalten.

Erwartung:
- Header-/Adress-/Bit-Anzeigen ändern sich passend.
- Programm bleibt stabil.
- Keine ungewollte Busübertragung nur durch Ändern der Anzeige.

Bewertung:
- PASS: Anzeige aktualisiert sich korrekt.
- FAIL: Anzeige bleibt falsch oder UI hängt.

## 7. Button-Bedienung ohne Hardware

Aktionen im Servo-Bildtab, z. B. Servo 1:
- `--`
- `-`
- `Mitte`
- `+`
- `++`
- `Links speichern`
- `Rechts speichern`

Erwartung ohne Hardware:
- Programm stürzt nicht ab.
- Log zeigt passende V2-Sx-Aktion, z. B. `V2 S1 Mitte`.
- Bei nicht verbundenem Backend darf Senden fehlschlagen, aber UI muss stabil bleiben.

Bewertung:
- PASS: Klicks erzeugen Logeinträge, keine Abstürze.
- FAIL: Absturz, Freeze oder falscher Servo-Index.

## 8. Button-Bedienung mit SX-Daemon/Hardware

Voraussetzung:
- SX-Daemon oder reales Interface verbunden.
- In V3 Backend/Bus korrekt wählen.
- Für aktuelles Zielsetup bevorzugt SX/SX0-only verwenden.

Aktionen:
1. Verbindung herstellen.
2. Servo-Bildtab öffnen.
3. Bei Servo 1 `Mitte` klicken.
4. Danach `-`, `+`, `Links speichern`, `Rechts speichern` testen.

Erwartung:
- V3 sendet über den gewählten Backend/Bus-Pfad.
- Keine Mehrfachsendungen auf falschen Bus.
- Servo-Index passt: Servo 1 => K11=0, Servo 16 => K11=15.

Bewertung:
- PASS: Decoder reagiert plausibel, Log passt.
- FAIL: falscher Bus, falscher Servo, keine Reaktion trotz korrekter Verbindung.
- N/A: Wenn Hardware nicht angeschlossen ist.

## 9. Live-RX/Servoarm prüfen

Voraussetzung:
- RX-Daten kommen im Monitor an.
- Adresse 1/2 im Servo-Bildtab entsprechen den überwachten Servo-Adressen.

Aktionen:
1. Adresse 1 auf aktive SX-Adresse stellen.
2. Adresse 2 auf zweite aktive SX-Adresse stellen oder 0, falls deaktiviert.
3. SX-Bits ändern bzw. Decoder schalten.

Erwartung:
- Servoarm-Grafiken bewegen sich passend zum empfangenen Bitzustand.
- Bitreihenfolge-Checkbox beeinflusst die Zuordnung wie erwartet.

Bewertung:
- PASS: Servoarme reagieren auf RX-Bits plausibel.
- FAIL: keine Bewegung trotz RX-Daten oder falsche Bitzuordnung.
- N/A: keine RX-Daten verfügbar.

## 10. Regressionsprüfung Monitor-Tabs

Nach Änderungen am Servo-Bildtab prüfen:
- SX-Monitor-Tab öffnet weiterhin.
- RMX-Monitor-Tab öffnet weiterhin.
- Senden im Monitorbereich funktioniert weiterhin.
- Frame-RX-Anzeige bleibt stabil.

Bewertung:
- PASS: bestehende Monitorfunktionen unverändert nutzbar.
- FAIL: Regression in Monitor-/Sendefunktion.

## 11. Fehlerprotokoll-Vorlage

Bei jedem Fehler notieren:

```text
Datum/Uhrzeit:
Branch/Commit:
Programm: sx_monitor_qt_v3
Testfall:
Erwartung:
Beobachtung:
Backend/Bus:
Port/Daemon:
Hardware angeschlossen: ja/nein
Screenshot/Log:
Bewertung: FAIL oder N/A
```

Commit ermitteln:

```bash
git -C /opt/programme/selectrix/Servodekoder rev-parse --short HEAD
```

## 12. Abnahmekriterien

Der Stand ist QA-ok, wenn:
- Build erfolgreich ist.
- V3 startet ohne Absturz.
- Tab „Servo-Bildansicht V2“ sichtbar ist.
- 16 Servo-Kacheln mit Original-Servo-Grafik sichtbar sind.
- V2-ähnliche Buttonstruktur pro Servo vorhanden ist.
- Buttons senden/loggen den richtigen Servo-Index.
- RX-Bits bewegen die Servoarm-Grafik plausibel.
- SX/RMX-Monitor-Tabs nicht regressiv beschädigt wurden.

Offene Punkte ausdrücklich als N/A dokumentieren, nicht als PASS werten.

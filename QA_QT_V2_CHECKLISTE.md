# QA Checkliste SXServo Qt V2

Projekt: /opt/programme/selectrix/Servodekoder  
Datum: __________  
Tester: __________  
Branch+Commit (Qt): __________  
Firmware-Version: __________

Legende Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A

## 0) Vorbereitung

- [ ] Hardware vollständig verbunden (SLX852, Decoder, Servo, Telemetrie)
- [ ] Stabile Ports über /dev/serial/by-id verwendet
- [ ] Kein Parallelzugriff auf Telemetrie-Port (z. B. PlatformIO-Monitor geschlossen)
- [ ] Logdatei bekannt: ~/SXServo-Logs/sxservo_qt_v2_latest.log

Notizen:

---

## A) App-Start / Grundzustand

### A1 Startzustand GUI
Aktion: Qt V2 starten

Erwartet:
- Status: offline
- Connect aktiv, Disconnect deaktiviert
- Telemetrie: offline
- Tabs sichtbar
- Gleisindikator initial ? / unbekannt (falls noch kein Busstatus)

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### A2 Tab-Umschaltung
Aktion: Zwischen Tabs wechseln

Erwartet:
- Kein Hänger/Crash
- Sichtbarkeit konsistent

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## B) SX Connect / Disconnect

### B1 SX Connect erfolgreich
Aktion: Korrektes SX-Interface wählen, Connect klicken

Erwartet:
- GUI: online, Connect grün/deaktiviert, Disconnect aktiv
- Log: Connect ok, ADR126 gesetzt

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### B2 SX Connect Fehlerfall
Aktion: Falschen Port eintragen, Connect klicken

Erwartet:
- Offline/open failed
- Popup mit Fehler
- Log mit Port+Grund

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### B3 Disconnect
Aktion: Disconnect klicken

Erwartet:
- GUI: offline
- Connect wieder aktiv

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## C) Telemetrie Connect / Parser

### C1 Telemetrie verbinden
Aktion: Arduino-Telemetrieport verbinden

Erwartet:
- telemetry online
- Log: Telemetrie-Port verbunden

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### C2 HELLO abrufen
Aktion: HELLO/t senden

Erwartet:
- Log: TEL TX: t
- Log: HELLO decoder=servodecoder fw=... proto=1
- FW-Statuslabel aktualisiert

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### C3 CFG-Dump abrufen
Aktion: c senden

Erwartet:
- CFG_HDR
- 16x CFG_S
- CFG_END

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### C4 Falschport-Schutz (optional)
Aktion: Bewusst falschen binären Port testen

Erwartet:
- Warnung Binärdaten
- Auto-Disconnect Telemetrie

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## D) Gleisindikator / Track-State

### D1 Track live
Aktion: Track AUS/AN schalten

Erwartet:
- Track=0: rot, "0", "Gleis: AUS"
- Track=1: grün, "1", "Gleis: AN"

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### D2 Buswechsel (SX0/SX1)
Aktion: Zwischen SX0/SX1 umschalten

Erwartet:
- Sofortige Anzeige des bekannten Trackstatus pro Bus
- Unbekannt -> "?" + "Gleis: unbekannt"

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### D3 Progmodus-Schutz bei Track=1
Aktion: Track=1 setzen, "Progmodus anfordern" klicken

Erwartet:
- Popup + Warnlog
- Kein K10=1 Start

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## E) SX senden Bereich

### E1 Manuelles Senden
Aktion: Bus/Adresse/Wert setzen, Senden

Erwartet:
- Log TX SXx ADR ... DATA=...

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### E2 Quick-Buttons 0/1/255
Aktion: Nacheinander klicken

Erwartet:
- Werte korrekt gesetzt + gesendet

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### E3 Senden bestätigen
Aktion: Confirm an, einmal Nein, einmal Ja

Erwartet:
- Nein -> kein Send
- Ja -> Send erfolgt

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### E4 Blockade bei aktivem Serial-Wizard
Aktion: Serial-Wizard aktiv, dann SX senden

Erwartet:
- Warnung/Blockade

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## F) Servo-Programmer (Wizard)

### F1 START/SAVE/ABORT im SX-Wizard
Aktion: Programmerweg SX-Wizard, Start/Move/Store/Save/Abort

Erwartet:
- K10/K13/K14 Impulse sichtbar
- ACK-Verhalten konsistent

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### F2 START/SAVE/ABORT im Serial-Wizard
Aktion: Programmerweg Serial-Wizard, Buttons testen

Erwartet:
- SER TX(start): s
- SER TX(save-end): w
- SER TX(abort): x

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### F3 Servo/Step/Move/Store
Aktion: Servo wählen, 1/2/5 testen, +/-/Mitte, L/R speichern

Erwartet:
- ACK_SETUP_* oder SER TX korrekt
- Servo reagiert physisch

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## G) Servo-Bildansicht V2

### G1 Basisbedienung
Aktion: S1, Mitte, +, -, ++, --

Erwartet:
- Ack-Pending -> Ack-OK/Timeout sauber
- Armvisualisierung konsistent

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### G2 Queue/Mehrfachklick
Aktion: Schnell mehrfach +/++ klicken

Erwartet:
- Queue sauber, keine Hänger
- Keine fremden Servosprünge

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### G3 Wizard-Lock
Aktion: Lock auf S1 erzeugen, dann S2 bewegen/store versuchen

Erwartet:
- Warnung/Block auf S2

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### G4 Save/Abort in V2
Aktion: Lauf A mit Save, Lauf B mit Abort

Erwartet:
- Statuslabel beendet (Save/Abort)
- Queue geleert

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## H) Lokale Prog-Taste / PROG_STATUS

### H1 Lokaler Start/Ende
Aktion: Lokale Taste drücken (Track=0)

Erwartet:
- PROG_STATUS active=1 ... source=local_button
- später active=0 ...
- GUI-Status inkl. Quelle/Track/D13

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### H2 Auto-s im Serial-Wizard
Aktion: Serial-Wizard aktiv, lokale Taste startet Progmodus

Erwartet:
- auto-start-nach-lokaler-progtaste
- SER TX: s
- danach ACK_SETUP_* möglich

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## I) ACK/Timeout/CFG-Fallback

### I1 STRICT
Aktion: Fallback aus

Erwartet:
- ACK ok nur bei echten ACK_SETUP_*
- sonst Timeout

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### I2 NOTBETRIEB
Aktion: Fallback ein

Erwartet:
- ACK-Modus Label orange
- Fallback klar markiert

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### I3 CFG-Import-Schutz
Aktion: Während Import Interaktionen auslösen

Erwartet:
- Kein Konflikt/Spam
- CFG komplett

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## J) Persistenz / Regression / Langlauf

### J1 EEPROM Persistenz
Aktion: Endlagen speichern, Neustart, c-Dump

Erwartet:
- Werte bleiben erhalten

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### J2 Fahrbetrieb nach Setup
Aktion: Fahrdaten 0/1 senden

Erwartet:
- Gespeicherte Endlagen werden korrekt angefahren

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

### J3 Langlauf 15-30 min
Aktion: Wiederholt bedienen (Move/Store/Buswechsel/Track)

Erwartet:
- Stabil, keine Timeout-Kaskaden
- Keine unerwünschten Servosprünge

Ergebnis: [ ] PASS  [ ] FAIL  [ ] N/A  
Beobachtung:

---

## Gesamt-Abnahme

- [ ] GO
- [x] NO-GO

Kritische Fehler (Blocker):

1) Track-State in Qt V2 wird nicht korrekt/live aktualisiert
- Reproduzierbar:
  - D1 FAIL: Bei Track 0/1 keine korrekte Anzeige (Farbe/0-1/Text) in Qt.
  - Auch mit „SX-Monitor RX pausieren“ = OFF keine Besserung.
- Auswirkung:
  - GUI-Zustand weicht vom realen Anlagenzustand ab.

2) Progmodus-Schutz in Qt arbeitet mit falschem Trackzustand
- Reproduzierbar:
  - D3 FAIL: „Progmodus anfordern“ in Qt verhält sich falsch bzgl. Trackstatus.
  - E4 FAIL: Serial-Wizard/Progmodus scheitert mit „Gleis an“, obwohl real Gleis AUS.
- Gegenprobe Firmware:
  - Arduino verhält sich korrekt:
    - Gleis AN -> lokale Prog-Taste startet NICHT in Progmodus.
    - Gleis AUS -> lokale Prog-Taste startet in Progmodus.
- Schluss:
  - Fehler liegt primär in Qt-Trackstatus-Übernahme/Interpretation, nicht in der Arduino-Logik.

3) SX-Senden loggt TX, aber keine physische Servoreaktion
- Reproduzierbar:
  - E1 PASS (TX-Log vorhanden), aber Servo bewegt sich nicht.
  - E2 PASS (Quick 0/1/255 gesendet), aber Servo bewegt sich nicht.
  - E3 PASS (Confirm-Dialoglogik korrekt), aber weiterhin ohne Servowirkung.
- Hinweis:
  - Wegen fehlerhaftem Track/Progmodus-Status ist die End-to-End-Wirkung aktuell nicht belastbar testbar.

Nicht-kritische Fehler:
- Port-Transparenz: GUI zeigt aktuell /dev/ttyUSB0; by-id-Zuordnung ist in der UI nicht transparent sichtbar.

Empfohlene nächste Maßnahmen:
1) Track-State-Pipeline in Qt debuggen und vollständig instrumentieren
- Eingangsdaten je Bus (SX0/SX1), zuletzt bekannter Trackstatus, Zeitstempel, Quelle.
- Jede Statusänderung mit Alt/Neu-Wert und Auslöser ins Log schreiben.

2) Klare Bus-Semantik im UI erzwingen
- Für aktuellen Aufbau dokumentieren/abbilden:
  - SX0 = Fahrbus (Track relevant)
  - SX1 = Schalt-/Meldebus (kein Fahr-Track für diese Prüfung)
- D2.1 daher in diesem Testlauf als N/A führen.

3) Progmodus-Guard an realen/aktuellen Trackstatus koppeln
- „Progmodus anfordern“ darf nur auf verifiziertem SX0-Trackzustand entscheiden.
- Fehlermeldung muss den tatsächlich gelesenen Bus/Trackwert anzeigen.

4) Nach Fix: Re-QA ab Abschnitt D erneut starten
- Reihenfolge: D -> E -> F/G/H (Wizard/Servo) -> I/J.

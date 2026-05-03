# SX-Servo-Aufsatz: Programmierkanal-Plan (V1)

Ziel: Einfacher Selectrix-Programmiermodus mit 2 Decoder-Adressen und pro-Servo-Parametern.

## Aktivierung Programmiermodus
- Eintritt: Prog-Taster kurz
  - Decoder setzt dabei aktiv `TrackBit=0` (Gleis aus)
  - D13 (Board-LED neben Schalter) wird EIN als klare Anzeige "Programmiermodus aktiv"
- Austritt: Prog-Taster lang ODER expliziter Ende-Befehl
  - Decoder setzt `TrackBit=1` (Gleis ein)
  - D13 wird AUS
- Beim Austritt: optional Auto-Save EEPROM

## Kanalbelegung (Programmieradresse)
Empfohlene dedizierte Programmieradresse: `ADR_PROG = 1` (nur im Programmiermodus ausgewertet).

- Kanal 1: `ADDR_A` (1..111)
- Kanal 2: `ADDR_B` (0..111, 0 = deaktiviert)
- Kanal 3: `SERVO_INDEX` (0..15)
- Kanal 4: `PARAM_ID`
  - 1 = `zeroPhys` (0..180)
  - 2 = `relMin` (-90..0)
  - 3 = `relMax` (0..90)
  - 4 = `divergingIsLeft` (0/1)
- Kanal 5: `PARAM_VALUE` (0..255, je Param umgerechnet)
- Kanal 6: `COMMIT`
  - 1 = Parameter übernehmen (Index/ID/Wert)
  - 2 = Testfahrt Mitte
  - 3 = Testfahrt Gerade
  - 4 = Testfahrt Abzweig
- Kanal 7: `SAVE`
  - 1 = EEPROM speichern
  - 2 = Defaults laden (alle Servos)
- Kanal 8: `STATUS/ACK` (vom Decoder geschrieben, optional)
  - 0 = idle
  - 1 = ok
  - 2 = range-error
  - 3 = index-error
  - 4 = busy

## Werte-Mapping
- `zeroPhys`: direkt 0..180
- `relMin`: `mapped = value - 90` -> gültig -90..0
- `relMax`: `mapped = value - 90` -> gültig 0..90
- `divergingIsLeft`: 0 oder 1

## Schutzregeln
- `ADDR_A` darf nie 0 sein
- Niemals beide Adressen deaktivieren
- Bei nur einer aktiven Adresse (B=0): nur Servos 0..7 über A
- `relMin < relMax` muss immer gelten
- Nach Konfig-Änderung Servo sofort soft-clamped fahren

## Laufzeitbetrieb (außerhalb Programmiermodus)
- Adresse A steuert Servo 0..7 per Bit0..7
- Adresse B steuert Servo 8..15 per Bit0..7 (falls B!=0)
- Bit=0 => Gerade, Bit=1 => Abzweig

## Minimal-UX vor Ort
- Prog-Taster + Status-LED:
  - langsam blinken = Programmiermodus
  - 2x kurz = Commit OK
  - 3x kurz = Fehler

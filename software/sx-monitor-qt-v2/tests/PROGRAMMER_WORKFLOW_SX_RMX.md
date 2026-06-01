# Programmer-Workflow SX/RMX (Qt V2)

## Ziel
Ein identischer Bedienablauf für Selectrix (SX) und RMX mit gleicher K10..K15-Semantik.

## Bus-/Backend-Regeln
- Backend wird oben gewählt: SX oder RMX.
- Buswahl folgt dem Backend:
  - SX: SX0 / SX1 / SX0+SX1
  - RMX: RMX0 / RMX1 / RMX0+RMX1
- Für aktive Programmierkommandos (K10..K14) gilt: Zielbus = `effectiveWizardBus()`.
- Gleiszustand (Prog EIN/AUS) läuft über Bus0 (SX0 bzw. RMX0).

## Ablauf
1. Setup START (K10=1 Impuls)
2. Servo wählen (K11), Schritt (K12)
3. MOVE (K13: -, +, Mitte)
4. STORE (K14: L/R)
5. SAVE+ENDE (K10=3) oder ABBRUCH (K10=2)

## Guards
- ACK-pending sperrt Move/Store bis ACK oder Timeout.
- Wizard-Lock auf einen Servo während Setup aktiv.
- Ohne aktive Session-ID (SX-Wizard-Modus) werden Move/Store geblockt.
- Serial-Wizard blockiert paralleles manuelles SX/RMX-Senden.

## Logging
- Logs müssen Backend-Busse semantisch korrekt ausgeben (SX0/SX1 oder RMX0/RMX1).
- Keine SX-only Texte bei RMX-Betrieb.

# SX Monitor Qt V3 – Rebuild Plan

Ziel: Wartbare Klassenstruktur statt monolithischem `sx_monitor_qt_v2.cpp`, inkl. Dual-Connect (SX+RMX parallel) und getrennte Gleisanzeige.

## Architektur

1. `main_v3.cpp`
- App-Start, `MainWindowV3` erzeugen.

2. `MainWindowV3` (UI-Orchestrator)
- Tabs/Layouts, Logging-Fenster, gemeinsame Bus-Ansicht.
- Hält Instanzen von:
  - `ConnectionPanel` (SX)
  - `ConnectionPanel` (RMX)
  - `DualRuntimeController`
  - `ProgrammerPanel`

3. `ConnectionPanel` (wiederverwendbare Komponente)
- Eine Zeile: Name, Socket, Connect/Disconnect, Track-Status.
- Emits: `connectRequested(endpoint, baud)`, `disconnectRequested()`.
- Slots: `setConnected()`, `setDisconnected()`, `setTrackState()`.

4. `DualRuntimeController`
- Besitzt zwei `SxRuntime` Instanzen (`sx`, `rmx`).
- Pollt beide zyklisch.
- Einheitliche Signale:
  - `frameReceived(backend,bus,adr,val)`
  - `trackUpdated(backend,trackBit)`
  - `status(backend,text)`

5. `ProgrammerPanel`
- K10..K15 Workflow (Start/Move/Store/Ende) als separate Klasse.
- Nutzt abstraktes Send-Interface (`send(backend,bus,adr,val)`).

## Migrationsphasen

Phase 1 (jetzt):
- Neues V3 Gerüst + Dual Connect + Track Anzeige (ohne kompletten Programmer-Funktionsport).

Phase 2:
- Programmierlogik modular aus V2 extrahieren und in `ProgrammerPanel` einhängen.

Phase 3:
- Tabellenmonitor/Bitansicht übernehmen.

## Akzeptanzkriterien Phase 1
- SX und RMX gleichzeitig connectbar.
- Je Backend eigener Connect-Status + Gleis AN/AUS.
- Polling stabil, keine UI-Blockade.
- Build erfolgreich als `sx_monitor_qt_v3`.

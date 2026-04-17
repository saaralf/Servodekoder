# Detaillierte Dokumentation: `src/main.cpp`

Diese Datei beschreibt die aktuelle Implementierung von
`software/SX30-ServoDecoder/src/main.cpp` im Detail.

Ziel des Programms:
- 16 Servos über PCA9685 schalten
- Ansteuerung über SX-Bus (SX30-Library)
- 2 SX-Adressen für 16 Weichenbits
- Programmiermodus (Taste + Track aus) für Adressen + Abzweigseite
- Persistenz in EEPROM


## 1) Gesamtarchitektur

Das Programm besteht aus 5 Blöcken:

1. Hardware-/Library-Initialisierung
   - SX30 für Selectrix-Bus
   - PCA9685 für Servopulse
   - EEPROM für dauerhafte Konfiguration

2. Datenmodell
   - Pro Servo (0..15):
     - Nullpunkt (`zeroPhys`)
     - Relativgrenzen (`relMin`, `relMax`)
     - Richtungsdefinition Abzweig links/rechts (`divergingIsLeft`)
   - Global:
     - Zwei SX-Adressen (`sxAddrA`, `sxAddrB`)

3. Laufzeit-Fahrlogik
   - SX-Datenbyte A steuert Servos 0..7
   - SX-Datenbyte B steuert Servos 8..15
   - Bitwert 0 = Gerade, 1 = Abzweig

4. Programmierlogik
   - Start bei `Track AUS + Taste`
   - Währenddessen werden Konfigurationswerte auf spezielle SX-Kanäle gespiegelt
   - Ende bei `Track EIN` oder Taste erneut
   - Werte vom Bus werden übernommen und gespeichert

5. Persistenz
   - Laden beim Start
   - Defaults, wenn EEPROM ungültig
   - Speichern nach Programmierende


## 2) Includes und ihre Rolle

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_PWMServoDriver.h>
#include <SX30.h>
```

- `Arduino.h`: Basisfunktionen (`pinMode`, `digitalWrite`, `millis`, `delay`, `bitRead`)
- `Wire.h`: I2C für PCA9685
- `EEPROM.h`: Konfiguration persistent speichern
- `Adafruit_PWMServoDriver.h`: PCA9685 API (`begin`, `setPWMFreq`, `setPWM`)
- `SX30.h`: Zugriff auf SX-Bus (`get`, `set`, `getTrackBit`, `isr`)


## 3) Konstanten und globale Instanzen

### 3.1 Hardware-Makros

```cpp
#define PROGLED      13
#define PROGBUTTON   A6
#define KEYPRESS     (analogRead(PROGBUTTON) > 512)
#define DEBOUNCETIME 200
```

- `PROGLED`: LED zeigt Programmiermodus
- `PROGBUTTON`: lokale Programmiertaste
- `KEYPRESS`: Analog-Threshold zur Tastererkennung
- `DEBOUNCETIME`: Entprellzeit

### 3.2 Geräteinstanzen

```cpp
SX30 sx;
Adafruit_PWMServoDriver pwm(0x40);
```

- `sx`: Selectrix-Kommunikation
- `pwm`: PCA9685 mit I2C-Adresse `0x40`

### 3.3 Servo-/Protokoll-Konstanten

```cpp
const uint8_t SERVO_COUNT = 16;
const uint16_t SERVO_MIN_TICK = 110;
const uint16_t SERVO_MAX_TICK = 500;
const uint8_t SX_ADDR_MIN = 1;
const uint8_t SX_ADDR_MAX = 111;
```

- 16 Kanäle über PCA9685
- Tickbereich für 0..180° Mapping
- Gültigkeitsbereich für SX-Adressen

### 3.4 Programmierkanäle (SX)

```cpp
const uint8_t SX_CHAN_ADDR_A    = 1;
const uint8_t SX_CHAN_ADDR_B    = 2;
const uint8_t SX_CHAN_ORIENT_L  = 3;
const uint8_t SX_CHAN_ORIENT_H  = 4;
```

Im Programmiermodus:
- Kanal 1: Adresse A
- Kanal 2: Adresse B
- Kanal 3: Orientierungsmaske Servo 0..7
- Kanal 4: Orientierungsmaske Servo 8..15


## 4) Datenstrukturen

### 4.1 `ServoCfg`

```cpp
struct ServoCfg {
  int16_t zeroPhys;
  int16_t relMin;
  int16_t relMax;
  uint8_t divergingIsLeft;
};
```

Bedeutung:
- `zeroPhys`: physischer Nullpunkt in Grad (meist 90)
- `relMin`: linker relativer Endpunkt (z. B. -40)
- `relMax`: rechter relativer Endpunkt (z. B. +40)
- `divergingIsLeft`: Mapping, wo „Abzweig“ mechanisch liegt

### 4.2 `DecoderCfg`

```cpp
struct DecoderCfg {
  uint16_t magic;
  uint8_t sxAddrA;
  uint8_t sxAddrB;
  ServoCfg servo[SERVO_COUNT];
};
```

- `magic`: EEPROM-Gültigkeitsmarker
- `sxAddrA`, `sxAddrB`: Laufzeitadressen für die 16 Weichenbits
- `servo[]`: vollständige per-Servo-Konfiguration


## 5) Globale Laufzeitvariablen

```cpp
DecoderCfg cfg;
bool programming = false;
uint32_t keyPressTime = 0;
uint8_t oldDataA = 0xFF;
uint8_t oldDataB = 0xFF;
```

- `cfg`: aktiver Konfigurationsstand
- `programming`: Zustand Programmiermodus
- `keyPressTime`: Entprell-/Sperrzeit bei Taste
- `oldDataA/B`: letzte SX-Daten, um Änderungen zu erkennen


## 6) Funktionsdokumentation (vollständig)

### `uint16_t angleToTick(uint8_t angle)`
Konvertiert Servo-Winkel (0..180°) auf PCA9685-Ticks (`SERVO_MIN_TICK..SERVO_MAX_TICK`).

Kontext:
- Zentral für alle Servoausgaben.
- Kapselt die Kalibrierung des Pulsbereichs.

### `bool validSxAddr(uint8_t a)`
Prüft, ob Adresse im erlaubten Bereich liegt.

Kontext:
- Schutz gegen ungültige Busadressen aus EEPROM oder Programmierung.

### `int16_t clampRel(uint8_t ch, int16_t rel)`
Begrenzt relativen Sollwert in zwei Stufen:
1. harte Klemme auf -90..+90
2. servoindividuelle Softlimits `relMin..relMax`

Kontext:
- verhindert mechanisches Überfahren
- zentrale Sicherheitsfunktion

### `void setServoRawPhys(uint8_t ch, uint8_t physAngle)`
Schreibt direkt einen physischen Winkel auf PCA9685-Kanal.

Kontext:
- low-level Ausgabe nach berechneter Zielposition

### `void setServoRel(uint8_t ch, int16_t rel)`
Hauptfunktion zur Positionssetzung:
1. relative Position clampen (`clampRel`)
2. zu physisch umrechnen über `zeroPhys`
3. physisch auf 0..180 begrenzen
4. an PCA9685 ausgeben

Kontext:
- wird von nahezu allen Fahrbefehlen indirekt genutzt

### `void setServoPhysical(uint8_t ch, int16_t phys)`
Alternative Eingabe als physischer Winkel.

Kontext:
- rechnet in relative Position um und verwendet dann `setServoRel`
- aktuell im Fahrpfad nicht zentral, aber nützlich für Erweiterungen

### `int16_t getRelTargetAbzweig(uint8_t ch)`
Liefert je Servo den relativen Zielwert für „Abzweig“:
- links -> `relMin`
- rechts -> `relMax`

### `int16_t getRelTargetGerade(uint8_t ch)`
Liefert je Servo den relativen Zielwert für „Gerade“:
- Gegenrichtung von Abzweig

Kontext beider Funktionen:
- trennt semantische Weichenlage von mechanischer Richtung
- erlaubt pro Servo unterschiedliche Einbaurichtungen

### `void moveGerade(uint8_t ch)` / `void moveAbzweig(uint8_t ch)`
Semantische Fahrbefehle auf Basis der Orientierungslogik.

Kontext:
- wird für SX-Bit-Auswertung verwendet
- sorgt dafür, dass „1=Abzweig“ unabhängig von Einbaulage korrekt ist

### `void applyBitToServo(uint8_t ch, uint8_t bitVal)`
Mappt SX-Bit auf Fahrbefehl:
- `0 => moveGerade`
- `1 => moveAbzweig`

### `void applyAllFromSx(uint8_t dataA, uint8_t dataB)`
Wendet zwei SX-Datenbytes auf alle 16 Servos an:
- Byte A -> Servos 0..7
- Byte B -> Servos 8..15

Kontext:
- zentrale Laufzeitfunktion für Fahrbetrieb

### `void setDefaults()`
Setzt vollständige Defaultkonfiguration:
- Adressen 72/73
- pro Servo zero=90, limits=-40/+40, Abzweig links

Kontext:
- Fallback bei leerem/ungültigem EEPROM

### `bool configValid(const DecoderCfg &c)`
Plausibilitätsprüfung der gesamten Konfiguration.

Prüft:
- Magic
- Adressbereich
- pro Servo: zero/min/max/Orientierung

### `void saveConfig()` / `bool loadConfig()`
Persistenz:
- `saveConfig`: schreibt `cfg` inkl. magic nach EEPROM
- `loadConfig`: liest, validiert, übernimmt bei Erfolg

Kontext:
- garantiert autarken Start ohne manuelles Nachkonfigurieren

### `uint8_t getOrientationMaskLow()/High()`
Packt `divergingIsLeft` von 16 Servos in zwei Bytes.

Kontext:
- Übertragung der Orientierungsdaten im Programmiermodus über SX-Kanäle

### `void setOrientationFromMasks(uint8_t lowMask, uint8_t highMask)`
Entpackt Orientierungsbits zurück in `cfg.servo[].divergingIsLeft`.

### `bool keypressed()`
Entprellte Tastererkennung mit Sperrzeit (`keyPressTime`).

Kontext:
- robustes Triggern des Programmiermodus

### `void startModuleProgramming()`
Aktiviert Programmiermodus:
- `programming = true`
- LED an
- schreibt aktuelle Werte auf Programmierkanäle

Kontext:
- entspricht dem OpenSX-Prinzip „Werte auf Bus legen, dann ändern lassen“

### `void finishModuleProgramming()`
Beendet Programmiermodus:
- liest neue Werte von Programmierkanälen
- validiert Adressen
- übernimmt Orientierungsbits
- speichert EEPROM
- LED aus

### `void sxisr()`
ISR-Weiterleitung:
- ruft `sx.isr()` auf

Kontext:
- notwendig, damit SX30 zeitkritisch korrekt arbeitet


## 7) Setup-Ablauf (Startsequenz)

`setup()` macht in Reihenfolge:

1. I/O vorbereiten (`PROGLED`, `PROGBUTTON`)
2. I2C/PCA9685 initialisieren (50 Hz)
3. Konfiguration laden
   - bei Fehler -> Defaults setzen und speichern
4. alle Servos zunächst auf Mitte (`rel 0`) setzen
5. SX initialisieren + Interrupt binden
6. aktuellen SX-Zustand lesen und sofort anwenden

Wichtiger Effekt:
- definierter, reproduzierbarer Startzustand
- danach direkte Synchronisation auf den aktuellen Buszustand


## 8) Loop-Ablauf (Runtime)

`loop()` arbeitet als zyklischer Zustandsautomat:

1. Aktuelle SX-Daten lesen (`sxAddrA`, `sxAddrB`)
2. Nur bei Änderung neu anwenden (`applyAllFromSx`)
3. Track-Bit auswerten
4. Programmiermodus-Übergänge:
   - Start bei `track==0 && keypressed()`
   - Ende bei `track==1 || keypressed()`


## 9) Bedeutung der SX-Bits im Betrieb

- `sxAddrA`, Bit0..Bit7 => Servo 0..7
- `sxAddrB`, Bit0..Bit7 => Servo 8..15

Pro Servo:
- Bit `0` -> Gerade
- Bit `1` -> Abzweig

Die tatsächliche Bewegungsrichtung wird pro Servo über `divergingIsLeft` auf links/rechts gemappt.


## 10) EEPROM-Kontext und Robustheit

- Magic schützt vor Zufallsdaten
- vollständige Plausibilitätsprüfung verhindert gefährliche Fehlkonfiguration
- bei ungültigen Daten automatische Rückfallebene (Defaults)
- Speichern passiert gezielt nach Programmierende


## 11) Zusammenhang mit Testprogramm

Das Fahrprogramm ist auf stabilen Betrieb ausgelegt.

Für feines Einlernen (mechanische Grenzen, individuelle Nullpunkte) ist das separate Test-/Kalibrierprogramm weiterhin sinnvoll. Danach übernimmt dieses Fahrprogramm die dauerhaft gespeicherten Werte.


## 12) Erweiterungsideen (optional)

- zusätzliche SX-Programmierkanäle für `zeroPhys`, `relMin`, `relMax` je Servo
- Rampe/geschwindigkeitsabhängige Bewegung pro Servo
- Fail-safe bei fehlendem SX-Datenwechsel über Timeout
- Diagnostik über serielle Statusausgabe im Debug-Build

/*
  SX30 Servo-Decoder (Basisplatine 3.0 + ServoAufsatz/PCA9685)

  Ziel:
  - 16 Weichenservos über SX-Bus schalten
  - 2 SX-Adressen (je 8 Bits) steuern 16 Servos
  - Programmiermodus wie OpenSX-Beispiele (Taste + Track aus)
  - EEPROM speichert:
      * SX-Adresse A/B
      * pro Servo: rel-Min/rel-Max/Nullpunkt
      * pro Servo: Abzweig links/rechts

  Laufzeit-Logik:
  - SX-Bit = 0  => GERADE
  - SX-Bit = 1  => ABZWEIG
  - Ob ABZWEIG links oder rechts ist, wird pro Servo konfiguriert (EEPROM)
*/

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_PWMServoDriver.h>
#include <SX30.h>

// ---------------- Hardware ----------------
#define PROGLED      13
#define PROGBUTTON   A6
#define KEYPRESS     (analogRead(PROGBUTTON) > 512)
#define DEBOUNCETIME 200

// SX30 ISR auf INT0
void sxisr();
SX30 sx;
Adafruit_PWMServoDriver pwm(0x40);

// ---------------- Servo/PCA9685 ----------------
const uint8_t SERVO_COUNT = 16;
const uint16_t SERVO_MIN_TICK = 110;   // bei Bedarf kalibrieren
const uint16_t SERVO_MAX_TICK = 500;   // bei Bedarf kalibrieren

// ---------------- SX Kanalgrenzen ----------------
const uint8_t SX_ADDR_DISABLED = 0;
const uint8_t SX_ADDR_MIN = 1;
const uint8_t SX_ADDR_MAX = 111;

// Programmierkanäle am SX-Bus
// Altbestand (Adresse/Orientierung)
const uint8_t SX_CHAN_ADDR_A    = 1;
const uint8_t SX_CHAN_ADDR_B    = 2;
const uint8_t SX_CHAN_ORIENT_L  = 3;   // Servo 0..7:  1=Abzweig links, 0=rechts
const uint8_t SX_CHAN_ORIENT_H  = 4;   // Servo 8..15: 1=Abzweig links, 0=rechts

// Neuer Setup-Wizard (gleiches Verhalten wie Serial-Setup)
const uint8_t SX_CHAN_SETUP_CMD   = 10; // 1=start, 2=abort, 3=save+end
const uint8_t SX_CHAN_SETUP_SERVO = 11; // 0..15
const uint8_t SX_CHAN_SETUP_STEP  = 12; // 1/2/5/10/20
const uint8_t SX_CHAN_SETUP_MOVE  = 13; // 1=-step, 2=+step, 3=mitte
const uint8_t SX_CHAN_SETUP_STORE = 14; // 1=save L, 2=save R
const uint8_t SX_CHAN_SETUP_ACK   = 15; // 0=idle,1=ok,2=err,3=busy

// ---------------- EEPROM ----------------
const uint16_t CFG_MAGIC = 0x5A41;
const int EEPROM_ADDR = 0;

struct ServoCfg {
  int16_t zeroPhys;         // physischer Nullpunkt für rel=0
  int16_t relMin;           // linker Endpunkt relativ zu zeroPhys
  int16_t relMax;           // rechter Endpunkt relativ zu zeroPhys
  uint8_t divergingIsLeft;  // 1=Abzweig links, 0=Abzweig rechts
};

struct DecoderCfg {
  uint16_t magic;
  uint8_t sxAddrA;
  uint8_t sxAddrB;
  ServoCfg servo[SERVO_COUNT];
};

DecoderCfg cfg;

// ---------------- Laufzeitstatus ----------------
bool programming = false;
uint32_t keyPressTime = 0;
uint8_t oldDataA = 0xFF;
uint8_t oldDataB = 0xFF;

// Interaktive Initialroutine (seriell)
bool setupMode = false;
bool setupBySxWizard = false;
uint8_t setupServo = 0;
int16_t setupRelPos = 0;
int16_t setupStep = 5;
uint8_t sxSetupLastCmd = 0;
uint8_t sxSetupLastServo = 0;
uint8_t sxSetupLastMove = 0;
uint8_t sxSetupLastStore = 0;

// Sequenzielles Ansteuern: niemals alle Servos gleichzeitig umschalten
const uint16_t SERVO_SWITCH_INTERVAL_MS = 35;
uint8_t pendingDataA = 0;
uint8_t pendingDataB = 0;
uint8_t pendingMaskA = 0;
uint8_t pendingMaskB = 0;
bool pendingUseA = false;
bool pendingUseB = false;
bool hasPendingApply = false;
uint8_t nextServoToApply = 0;
uint32_t lastServoSwitchMs = 0;

// ---------- Hilfsfunktionen ----------
uint16_t angleToTick(uint8_t angle) {
  if (angle > 180) angle = 180;
  return map(angle, 0, 180, SERVO_MIN_TICK, SERVO_MAX_TICK);
}

bool validSxAddr(uint8_t a) {
  return (a >= SX_ADDR_MIN && a <= SX_ADDR_MAX);
}

bool validOrDisabledSxAddr(uint8_t a) {
  return (a == SX_ADDR_DISABLED) || validSxAddr(a);
}

bool sxAddrEnabled(uint8_t a) {
  return validSxAddr(a);
}

int16_t clampRel(uint8_t ch, int16_t rel) {
  if (rel < -90) rel = -90;
  if (rel > 90) rel = 90;
  if (rel < cfg.servo[ch].relMin) rel = cfg.servo[ch].relMin;
  if (rel > cfg.servo[ch].relMax) rel = cfg.servo[ch].relMax;
  return rel;
}

void setServoRawPhys(uint8_t ch, uint8_t physAngle) {
  if (ch >= SERVO_COUNT) return;
  pwm.setPWM(ch, 0, angleToTick(physAngle));
}

void setServoRel(uint8_t ch, int16_t rel) {
  if (ch >= SERVO_COUNT) return;
  rel = clampRel(ch, rel);

  int16_t phys = cfg.servo[ch].zeroPhys + rel;
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;

  setServoRawPhys(ch, (uint8_t)phys);
}

void moveGerade(uint8_t ch) {
  if (cfg.servo[ch].divergingIsLeft) {
    setServoRel(ch, cfg.servo[ch].relMax);
  } else {
    setServoRel(ch, cfg.servo[ch].relMin);
  }
}

void moveAbzweig(uint8_t ch) {
  if (cfg.servo[ch].divergingIsLeft) {
    setServoRel(ch, cfg.servo[ch].relMin);
  } else {
    setServoRel(ch, cfg.servo[ch].relMax);
  }
}

void applyBitToServo(uint8_t ch, uint8_t bitVal) {
  // 0 = Gerade, 1 = Abzweig
  if (bitVal) moveAbzweig(ch);
  else moveGerade(ch);
}

void applyAllFromSx(uint8_t dataA, uint8_t dataB, bool useA, bool useB) {
  // Nur geaenderte Bits als Pending markieren (wichtig: sonst bewegen sich zu viele Servos)
  pendingDataA = dataA;
  pendingDataB = dataB;
  pendingUseA = useA;
  pendingUseB = useB;

  pendingMaskA = useA ? (uint8_t)(dataA ^ oldDataA) : 0;
  pendingMaskB = useB ? (uint8_t)(dataB ^ oldDataB) : 0;

  hasPendingApply = (pendingMaskA != 0) || (pendingMaskB != 0);
}

void processPendingServoStep() {
  if (!hasPendingApply) return;
  if ((millis() - lastServoSwitchMs) < SERVO_SWITCH_INTERVAL_MS) return;

  for (uint8_t n = 0; n < SERVO_COUNT; n++) {
    uint8_t ch = (nextServoToApply + n) % SERVO_COUNT;
    bool enabled = (ch < 8) ? pendingUseA : pendingUseB;
    if (!enabled) continue;

    uint8_t bitVal = (ch < 8) ? bitRead(pendingDataA, ch)
                              : bitRead(pendingDataB, ch - 8);
    bool isPending = (ch < 8) ? bitRead(pendingMaskA, ch)
                              : bitRead(pendingMaskB, ch - 8);

    if (isPending) {
      applyBitToServo(ch, bitVal);
      if (ch < 8) {
        bitWrite(oldDataA, ch, bitVal);
        bitWrite(pendingMaskA, ch, 0);
      } else {
        bitWrite(oldDataB, ch - 8, bitVal);
        bitWrite(pendingMaskB, ch - 8, 0);
      }

      nextServoToApply = (ch + 1) % SERVO_COUNT;
      lastServoSwitchMs = millis();
      return;
    }
  }

  // nichts mehr zu tun
  hasPendingApply = false;
  pendingMaskA = 0;
  pendingMaskB = 0;
}

void setDefaults() {
  cfg.sxAddrA = 72;
  cfg.sxAddrB = 73;

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    cfg.servo[i].zeroPhys = 90;
    cfg.servo[i].relMin = -40;
    cfg.servo[i].relMax = 40;
    cfg.servo[i].divergingIsLeft = 1;
  }
}

bool configValid(const DecoderCfg &c) {
  if (c.magic != CFG_MAGIC) return false;
  if (!validOrDisabledSxAddr(c.sxAddrA) || !validOrDisabledSxAddr(c.sxAddrB)) return false;
  // Mindestens eine SX-Adresse muss aktiv sein
  if (!sxAddrEnabled(c.sxAddrA) && !sxAddrEnabled(c.sxAddrB)) return false;

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    const ServoCfg &s = c.servo[i];
    if (s.zeroPhys < 0 || s.zeroPhys > 180) return false;
    if (s.relMin < -90 || s.relMin > 90) return false;
    if (s.relMax < -90 || s.relMax > 90) return false;
    if (s.relMin >= s.relMax) return false;
    if (!(s.divergingIsLeft == 0 || s.divergingIsLeft == 1)) return false;
  }
  return true;
}

void saveConfig() {
  cfg.magic = CFG_MAGIC;
  EEPROM.put(EEPROM_ADDR, cfg);
  delay(10);
}

bool loadConfig() {
  DecoderCfg tmp;
  EEPROM.get(EEPROM_ADDR, tmp);
  if (!configValid(tmp)) return false;
  cfg = tmp;
  return true;
}

uint8_t getOrientationMaskLow() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (cfg.servo[i].divergingIsLeft) bitSet(m, i);
  }
  return m;
}

uint8_t getOrientationMaskHigh() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (cfg.servo[i + 8].divergingIsLeft) bitSet(m, i);
  }
  return m;
}

void setOrientationFromMasks(uint8_t lowMask, uint8_t highMask) {
  for (uint8_t i = 0; i < 8; i++) {
    cfg.servo[i].divergingIsLeft = bitRead(lowMask, i) ? 1 : 0;
  }
  for (uint8_t i = 0; i < 8; i++) {
    cfg.servo[i + 8].divergingIsLeft = bitRead(highMask, i) ? 1 : 0;
  }
}

void printSetupHelp() {
  Serial.println(F("\n=== SERVO SETUP ==="));
  Serial.println(F("n = naechster Servo"));
  Serial.println(F("v = voriger Servo"));
  Serial.println(F("0 = Mitte (rel 0)"));
  Serial.println(F("l = linken Anschlag aus aktueller Position speichern"));
  Serial.println(F("r = rechten Anschlag aus aktueller Position speichern"));
  Serial.println(F("1/2/5/a/b = Schrittweite 1/2/5/10/20"));
  Serial.println(F("+ = nach rechts, - = nach links (um Schrittweite)"));
  Serial.println(F("w = alles speichern und Setup beenden"));
  Serial.println(F("x = Setup ohne Speichern beenden"));
}

void setupSelectServo(uint8_t ch) {
  if (ch >= SERVO_COUNT) ch = SERVO_COUNT - 1;
  setupServo = ch;
  setupRelPos = 0;
  setServoRel(setupServo, setupRelPos);
  Serial.print(F("\nServo S")); Serial.print(setupServo + 1);
  Serial.println(F(" aktiv, Mitte angefahren (rel 0)."));
}

void setupMoveRel(int16_t delta) {
  setupRelPos += delta;
  if (setupRelPos < -90) setupRelPos = -90;
  if (setupRelPos > 90) setupRelPos = 90;

  int16_t phys = cfg.servo[setupServo].zeroPhys + setupRelPos;
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;
  setServoRawPhys(setupServo, (uint8_t)phys);

  Serial.print(F("S")); Serial.print(setupServo + 1);
  Serial.print(F(" rel=")); Serial.println(setupRelPos);
}

void setupAck(uint8_t v) {
  while (sx.set(SX_CHAN_SETUP_ACK, v) != 0) delay(2);
}

bool setupValidateAll() {
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    if (cfg.servo[i].relMin >= cfg.servo[i].relMax) return false;
  }
  return true;
}

void startInitialSetup(bool fromSxWizard = false) {
  setupMode = true;
  setupBySxWizard = fromSxWizard;
  digitalWrite(PROGLED, HIGH); // D13: Einstellmodus aktiv
  setupStep = 5;
  sxSetupLastCmd = fromSxWizard ? sx.get(SX_CHAN_SETUP_CMD) : 0;
  sxSetupLastServo = fromSxWizard ? sx.get(SX_CHAN_SETUP_SERVO) : 0;
  sxSetupLastMove = fromSxWizard ? sx.get(SX_CHAN_SETUP_MOVE) : 0;
  sxSetupLastStore = fromSxWizard ? sx.get(SX_CHAN_SETUP_STORE) : 0;
  if (!fromSxWizard) printSetupHelp();
  setupSelectServo(0);
  setupAck(1);
}

void processSetupSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;

    switch (c) {
      case 'n': setupSelectServo((setupServo + 1) % SERVO_COUNT); break;
      case 'v': setupSelectServo((setupServo == 0) ? (SERVO_COUNT - 1) : (setupServo - 1)); break;
      case '0': setupRelPos = 0; setServoRel(setupServo, setupRelPos); Serial.println(F("Mitte (rel 0).")); break;
      case 'l':
        cfg.servo[setupServo].relMin = setupRelPos;
        Serial.print(F("S")); Serial.print(setupServo + 1);
        Serial.print(F(" relMin gespeichert: ")); Serial.println(cfg.servo[setupServo].relMin);
        break;
      case 'r':
        cfg.servo[setupServo].relMax = setupRelPos;
        Serial.print(F("S")); Serial.print(setupServo + 1);
        Serial.print(F(" relMax gespeichert: ")); Serial.println(cfg.servo[setupServo].relMax);
        break;
      case '1': setupStep = 1; Serial.println(F("Schrittweite=1")); break;
      case '2': setupStep = 2; Serial.println(F("Schrittweite=2")); break;
      case '5': setupStep = 5; Serial.println(F("Schrittweite=5")); break;
      case 'a': setupStep = 10; Serial.println(F("Schrittweite=10")); break;
      case 'b': setupStep = 20; Serial.println(F("Schrittweite=20")); break;
      case '+': setupMoveRel(setupStep); break;
      case '-': setupMoveRel(-setupStep); break;
      case 'w': {
        bool ok = setupValidateAll();
        if (!ok) {
          setupAck(2);
          Serial.println(F("FEHLER: mindestens ein Servo hat relMin >= relMax. Nicht gespeichert."));
        } else {
          saveConfig();
          setupMode = false;
          digitalWrite(PROGLED, LOW); // D13 aus: Einstellmodus beendet
          setupAck(1);
          Serial.println(F("Setup gespeichert, beendet."));
        }
      } break;
      case 'x':
        setupMode = false;
        digitalWrite(PROGLED, LOW); // D13 aus: Einstellmodus beendet
        setupAck(0);
        Serial.println(F("Setup beendet ohne Speichern."));
        break;
      case 'h':
      case '?':
        printSetupHelp();
        break;
      default:
        // Unbekannte/Steuerzeichen ignorieren (z.B. Bus-/Terminal-Noise)
        break;
    }
  }
}

bool keypressed() {
  if ((millis() - keyPressTime) < (5UL * DEBOUNCETIME)) return false;

  if (KEYPRESS) {
    delay(DEBOUNCETIME);
    if (KEYPRESS) {
      keyPressTime = millis();
      return true;
    }
  }
  return false;
}

void startModuleProgramming() {
  programming = true;
  keyPressTime = millis();
  digitalWrite(PROGLED, HIGH);

  // aktuelle Werte auf Programmierkanäle legen
  while (sx.set(SX_CHAN_ADDR_A, cfg.sxAddrA) != 0) delay(10);
  while (sx.set(SX_CHAN_ADDR_B, cfg.sxAddrB) != 0) delay(10);
  while (sx.set(SX_CHAN_ORIENT_L, getOrientationMaskLow()) != 0) delay(10);
  while (sx.set(SX_CHAN_ORIENT_H, getOrientationMaskHigh()) != 0) delay(10);
}

void finishModuleProgramming() {
  programming = false;

  uint8_t newA = sx.get(SX_CHAN_ADDR_A);
  uint8_t newB = sx.get(SX_CHAN_ADDR_B);
  uint8_t lowMask = sx.get(SX_CHAN_ORIENT_L);
  uint8_t highMask = sx.get(SX_CHAN_ORIENT_H);

  uint8_t candA = cfg.sxAddrA;
  uint8_t candB = cfg.sxAddrB;

  if (validOrDisabledSxAddr(newA)) candA = newA;
  if (validOrDisabledSxAddr(newB)) candB = newB;

  // Sicherheit: nie beide Adressen deaktivieren
  if (!sxAddrEnabled(candA) && !sxAddrEnabled(candB)) {
    // falls beide 0 wurden, Konfiguration unverändert lassen
  } else {
    cfg.sxAddrA = candA;
    cfg.sxAddrB = candB;
  }

  setOrientationFromMasks(lowMask, highMask);

  saveConfig();
  digitalWrite(PROGLED, LOW);
}

// ---------- SX ISR ----------
void sxisr() {
  sx.isr();
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);

  pinMode(PROGLED, OUTPUT);
  digitalWrite(PROGLED, LOW);
  pinMode(PROGBUTTON, INPUT);
  digitalWrite(PROGBUTTON, HIGH); // Pullup

  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);

  bool loadedFromEeprom = loadConfig();
  if (!loadedFromEeprom) {
    setDefaults();
    saveConfig();
  }

  Serial.println();
  Serial.println(F("SX30 ServoDecoder start"));
  Serial.println(F("FW-Version: SX30-ServoDecoder 2026-05-04g"));
  Serial.println(loadedFromEeprom ? F("CFG: aus EEPROM geladen") : F("CFG: Defaults genutzt"));
  Serial.println(F("Setup starten: 's' senden"));

  // Beim Start zuerst alle auf Mitte (rel 0), bewusst nacheinander
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    setServoRel(i, 0);
    delay(SERVO_SWITCH_INTERVAL_MS);
  }

  sx.init();
  attachInterrupt(0, sxisr, CHANGE);

  // aktuellen SX-Zustand direkt übernehmen
  bool useA = sxAddrEnabled(cfg.sxAddrA);
  bool useB = sxAddrEnabled(cfg.sxAddrB);
  oldDataA = useA ? sx.get(cfg.sxAddrA) : 0;
  oldDataB = useB ? sx.get(cfg.sxAddrB) : 0;
  applyAllFromSx(oldDataA, oldDataB, useA, useB);
}

void processSetupSxWizard() {
  uint8_t cmd = sx.get(SX_CHAN_SETUP_CMD);
  uint8_t move = sx.get(SX_CHAN_SETUP_MOVE);
  uint8_t store = sx.get(SX_CHAN_SETUP_STORE);

  if (cmd != sxSetupLastCmd) {
    sxSetupLastCmd = cmd;
    if (cmd == 1) {
      startInitialSetup(true);
      setupAck(1);
    } else if (cmd == 2) {
      setupMode = false;
      digitalWrite(PROGLED, LOW); // D13 aus: Einstellmodus beendet
      setupAck(0);
    } else if (cmd == 3) {
      if (setupValidateAll()) {
        saveConfig();
        setupMode = false;
        digitalWrite(PROGLED, LOW); // D13 aus: Einstellmodus beendet
        setupAck(1);
      } else {
        setupAck(2);
      }
    }
  }

  if (!setupMode) return;

  uint8_t sxServo = sx.get(SX_CHAN_SETUP_SERVO);
  if (sxServo >= SERVO_COUNT) sxServo = SERVO_COUNT - 1;
  if (sxServo != sxSetupLastServo) {
    sxSetupLastServo = sxServo;
    if (sxServo != setupServo) {
      setupSelectServo(sxServo);
      setupAck(1);
    }
  }

  uint8_t sxStep = sx.get(SX_CHAN_SETUP_STEP);
  if (sxStep == 1 || sxStep == 2 || sxStep == 5 || sxStep == 10 || sxStep == 20) setupStep = sxStep;

  // Nur 0->Befehl Flanken akzeptieren (robust gegen Bus-Jitter/Mehrfachtelegramme)
  if (move != sxSetupLastMove) {
    if (sxSetupLastMove == 0) {
      if (move == 1) { setupMoveRel(-setupStep); setupAck(1); }
      else if (move == 2) { setupMoveRel(setupStep); setupAck(1); }
      else if (move == 3) { setupRelPos = 0; setServoRel(setupServo, 0); setupAck(1); }
    }
    sxSetupLastMove = move;
  }

  if (store != sxSetupLastStore) {
    if (sxSetupLastStore == 0) {
      if (store == 1) {
        cfg.servo[setupServo].relMin = setupRelPos;
        setupAck(1);
      } else if (store == 2) {
        cfg.servo[setupServo].relMax = setupRelPos;
        setupAck(1);
      }
    }
    sxSetupLastStore = store;
  }
}

void loop() {
  processSetupSxWizard();

  if (setupMode) {
    // Serial-Befehle immer erlauben (auch wenn Setup per SX-Wizard gestartet wurde),
    // damit 'x'/'w'/+/- lokal zuverlässig funktionieren.
    processSetupSerial();
    return;
  }

  // Setup-Trigger jederzeit per seriell
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    Serial.print(F("RX:")); Serial.println(c);
    if (c == 's' || c == 'S') {
      startInitialSetup(false);
      return;
    }
  }

  bool useA = sxAddrEnabled(cfg.sxAddrA);
  bool useB = sxAddrEnabled(cfg.sxAddrB);

  uint8_t dA = useA ? sx.get(cfg.sxAddrA) : oldDataA;
  uint8_t dB = useB ? sx.get(cfg.sxAddrB) : oldDataB;

  bool changed = false;
  if (useA && dA != oldDataA) changed = true;
  if (useB && dB != oldDataB) changed = true;

  if (changed) {
    applyAllFromSx(dA, dB, useA, useB);
  }

  // Pro Loop maximal ein Servo-Schritt
  processPendingServoStep();

  uint8_t track = sx.getTrackBit();

  if (programming) {
    if (track || keypressed()) {
      // Ende Programmiermodus bei Track EIN oder erneutem Tastendruck
      finishModuleProgramming();
    }
  } else {
    // Start Programmiermodus nur bei Track AUS und Tastendruck
    if ((track == 0) && keypressed()) {
      startModuleProgramming();
    }
  }
}

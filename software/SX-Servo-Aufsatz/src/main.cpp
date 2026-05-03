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
const bool SERVO_RELAX_AFTER_MOVE = true;
const uint16_t SERVO_RELAX_MS = 700;
const bool DEBUG_RELAX_LOG = false;
const bool DEBUG_PROG_RX = true;

// ---------------- SX Kanalgrenzen ----------------
const uint8_t SX_ADDR_DISABLED = 0;
const uint8_t SX_ADDR_MIN = 1;
const uint8_t SX_ADDR_MAX = 111;

// Programmierkanäle am SX-Bus (frei definierbar)
// Wie bei OpenSX: bei aktivem Programmiermodus schreibt der Decoder
// seine aktuellen Werte hinein; beim Beenden werden sie wieder eingelesen.
const uint8_t SX_CHAN_ADDR_A    = 1;
const uint8_t SX_CHAN_ADDR_B    = 2;
const uint8_t SX_CHAN_ORIENT_L  = 3;   // Servo 0..7:  1=Abzweig links, 0=rechts
const uint8_t SX_CHAN_ORIENT_H  = 4;   // Servo 8..15: 1=Abzweig links, 0=rechts
const uint8_t SX_CHAN_TEST_SERVO = 100; // 0..15 (dedizierter Testkanal)
const uint8_t SX_CHAN_TEST_CMD   = 101; // 2=Mitte,3=Gerade,4=Abzweig

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
uint8_t lastTestCmd = 0;
uint32_t lastProgDebugMs = 0;
uint32_t relaxUntil[SERVO_COUNT] = {0};
bool servoPwmEnabled[SERVO_COUNT] = {false};

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
  servoPwmEnabled[ch] = true;
}

void setServoRel(uint8_t ch, int16_t rel) {
  if (ch >= SERVO_COUNT) return;
  rel = clampRel(ch, rel);

  int16_t phys = cfg.servo[ch].zeroPhys + rel;
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;

  setServoRawPhys(ch, (uint8_t)phys);
  if (SERVO_RELAX_AFTER_MOVE) {
    relaxUntil[ch] = millis() + SERVO_RELAX_MS;
  }
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
  if (useA) {
    for (uint8_t i = 0; i < 8; i++) {
      applyBitToServo(i, bitRead(dataA, i));
    }
  }
  if (useB) {
    for (uint8_t i = 0; i < 8; i++) {
      applyBitToServo(i + 8, bitRead(dataB, i));
    }
  }
}

void setDefaults() {
  cfg.sxAddrA = 20;
  cfg.sxAddrB = 21;

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
  while (sx.set(SX_CHAN_TEST_SERVO, 0) != 0) delay(10);
  while (sx.set(SX_CHAN_TEST_CMD, 0) != 0) delay(10);
  lastTestCmd = 0;
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

void serviceServoRelax() {
  if (!SERVO_RELAX_AFTER_MOVE) return;
  uint32_t now = millis();
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    if (servoPwmEnabled[ch] && relaxUntil[ch] != 0) {
      if ((int32_t)(now - relaxUntil[ch]) >= 0) {
        pwm.setPWM(ch, 0, 0); // PWM aus -> Servo stromlos/relaxed
        if (DEBUG_RELAX_LOG) {
          Serial.print(F("Servo "));
          Serial.print(ch);
          Serial.println(F(" RELAX: PWM AUS (stromlos)"));
        }
        servoPwmEnabled[ch] = false;
        relaxUntil[ch] = 0;
      }
    }
  }
}

// ---------- SX ISR ----------
void sxisr() {
  sx.isr();
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println(F("SX30-ServoDecoder Start"));

  pinMode(PROGLED, OUTPUT);
  digitalWrite(PROGLED, LOW);
  pinMode(PROGBUTTON, INPUT);
  digitalWrite(PROGBUTTON, HIGH); // Pullup

  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);

  if (!loadConfig()) {
    setDefaults();
    saveConfig();
  }

  // Beim Start zuerst alle auf Mitte (rel 0)
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    setServoRel(i, 0);
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

void loop() {
  serviceServoRelax();

  bool useA = sxAddrEnabled(cfg.sxAddrA);
  bool useB = sxAddrEnabled(cfg.sxAddrB);

  uint8_t dA = useA ? sx.get(cfg.sxAddrA) : oldDataA;
  uint8_t dB = useB ? sx.get(cfg.sxAddrB) : oldDataB;

  bool changed = false;
  if (useA && dA != oldDataA) changed = true;
  if (useB && dB != oldDataB) changed = true;

  if (changed) {
    applyAllFromSx(dA, dB, useA, useB);
    if (useA) oldDataA = dA;
    if (useB) oldDataB = dB;
  }

  uint8_t track = sx.getTrackBit();

  if (programming) {
    uint8_t tcmd = sx.get(SX_CHAN_TEST_CMD);
    uint8_t sNow = sx.get(SX_CHAN_TEST_SERVO);

    if (DEBUG_PROG_RX && (millis() - lastProgDebugMs) > 500) {
      lastProgDebugMs = millis();
      Serial.print(F("PROG RX: Track=")); Serial.print(track);
      Serial.print(F(" K3=")); Serial.print(sNow);
      Serial.print(F(" K6=")); Serial.println(tcmd);
    }

    if (tcmd != 0 && tcmd != lastTestCmd) {
      uint8_t s = sNow;
      Serial.print(F("RX TEST CMD=")); Serial.print(tcmd);
      Serial.print(F(" SERVO=")); Serial.println(s);
      if (s < SERVO_COUNT) {
        if (tcmd == 2) { setServoRel(s, 0); Serial.print(F("TEST S")); Serial.print(s); Serial.println(F(" MITTE")); }
        else if (tcmd == 3) { applyBitToServo(s, 0); Serial.print(F("TEST S")); Serial.print(s); Serial.println(F(" GERADE")); }
        else if (tcmd == 4) { applyBitToServo(s, 1); Serial.print(F("TEST S")); Serial.print(s); Serial.println(F(" ABZWEIG")); }
      }
      lastTestCmd = tcmd;
      while (sx.set(SX_CHAN_TEST_CMD, 0) != 0) delay(2);
      lastTestCmd = 0;
    }

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

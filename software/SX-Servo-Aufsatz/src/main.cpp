#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const uint16_t SERVO_MIN_TICK = 110;  // physisch ~0°
const uint16_t SERVO_MAX_TICK = 500;  // physisch ~180°

const uint8_t SERVO_COUNT = 16;

struct ServoConfig {
  int16_t zeroPhys;         // physischer Nullpunkt fuer rel=0
  int16_t relMin;           // linker Endpunkt (rel)
  int16_t relMax;           // rechter Endpunkt (rel)
  int8_t divergingIsLeft;   // 1=Abzweig links, 0=Abzweig rechts
};

struct EepromBlob {
  uint16_t magic;
  ServoConfig cfg[SERVO_COUNT];
};

const uint16_t CFG_MAGIC = 0x5A32;
const int CFG_ADDR = 0;

ServoConfig g_cfg[SERVO_COUNT];
int16_t g_relAngle[SERVO_COUNT];
int16_t g_physAngle[SERVO_COUNT];

uint8_t g_activeServo = 0;

uint16_t angleToTick(uint8_t angle) {
  if (angle > 180) angle = 180;
  return map(angle, 0, 180, SERVO_MIN_TICK, SERVO_MAX_TICK);
}

void setServoAngleRaw(uint8_t channel, uint8_t angle) {
  if (channel >= SERVO_COUNT) return;
  pwm.setPWM(channel, 0, angleToTick(angle));
}

int16_t clampRel(uint8_t ch, int16_t rel) {
  if (rel < -90) rel = -90;
  if (rel > 90) rel = 90;

  if (rel < g_cfg[ch].relMin) rel = g_cfg[ch].relMin;
  if (rel > g_cfg[ch].relMax) rel = g_cfg[ch].relMax;
  return rel;
}

int16_t getRelTargetAbzweig(uint8_t ch) {
  return (g_cfg[ch].divergingIsLeft == 1) ? g_cfg[ch].relMin : g_cfg[ch].relMax;
}

int16_t getRelTargetGerade(uint8_t ch) {
  return (g_cfg[ch].divergingIsLeft == 1) ? g_cfg[ch].relMax : g_cfg[ch].relMin;
}

const __FlashStringHelper* getTurnoutStateText(uint8_t ch) {
  if (g_relAngle[ch] == getRelTargetAbzweig(ch)) return F("ABZWEIG");
  if (g_relAngle[ch] == getRelTargetGerade(ch)) return F("GERADE");
  if (g_relAngle[ch] == 0) return F("MITTE");
  return F("ZWISCHEN");
}

void printServoState(uint8_t ch) {
  Serial.print(F("Servo "));
  Serial.print(ch);
  Serial.print(F(": rel="));
  Serial.print(g_relAngle[ch]);
  Serial.print(F("°, phys="));
  Serial.print(g_physAngle[ch]);
  Serial.print(F("°, zeroPhys="));
  Serial.print(g_cfg[ch].zeroPhys);
  Serial.print(F("°, limits=["));
  Serial.print(g_cfg[ch].relMin);
  Serial.print(F(".."));
  Serial.print(g_cfg[ch].relMax);
  Serial.print(F("], abzweig="));
  Serial.print(g_cfg[ch].divergingIsLeft ? F("links") : F("rechts"));
  Serial.print(F(", stellung="));
  Serial.println(getTurnoutStateText(ch));
}

void setServoRelative(uint8_t ch, int16_t rel) {
  if (ch >= SERVO_COUNT) return;

  g_relAngle[ch] = clampRel(ch, rel);
  int16_t phys = g_cfg[ch].zeroPhys + g_relAngle[ch];
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;
  g_physAngle[ch] = phys;

  setServoAngleRaw(ch, (uint8_t)g_physAngle[ch]);
  printServoState(ch);
}

void setServoPhysical(uint8_t ch, int16_t phys) {
  if (ch >= SERVO_COUNT) return;
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;

  g_physAngle[ch] = phys;
  int16_t rel = g_physAngle[ch] - g_cfg[ch].zeroPhys;
  setServoRelative(ch, rel);
}

void setDefaultsForServo(uint8_t ch) {
  g_cfg[ch].zeroPhys = 90;
  g_cfg[ch].relMin = -40;
  g_cfg[ch].relMax = 40;
  g_cfg[ch].divergingIsLeft = 1;
}

void setDefaultsForAllServos() {
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    setDefaultsForServo(ch);
  }
}

bool configIsValid(const EepromBlob &blob) {
  if (blob.magic != CFG_MAGIC) return false;
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    const ServoConfig &c = blob.cfg[ch];
    if (c.zeroPhys < 0 || c.zeroPhys > 180) return false;
    if (c.relMin < -90 || c.relMin > 90) return false;
    if (c.relMax < -90 || c.relMax > 90) return false;
    if (c.relMin >= c.relMax) return false;
    if (!(c.divergingIsLeft == 0 || c.divergingIsLeft == 1)) return false;
  }
  return true;
}

void saveConfig() {
  EepromBlob blob;
  blob.magic = CFG_MAGIC;
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    blob.cfg[ch] = g_cfg[ch];
  }
  EEPROM.put(CFG_ADDR, blob);
  Serial.println(F("Konfiguration (alle 16 Servos) in EEPROM gespeichert."));
}

bool loadConfig() {
  EepromBlob blob;
  EEPROM.get(CFG_ADDR, blob);
  if (!configIsValid(blob)) return false;

  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    g_cfg[ch] = blob.cfg[ch];
  }
  return true;
}

char readNextTokenChar(uint16_t timeoutMs = 500) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
      if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
      return c;
    }
  }
  return 0;
}

void scanI2C() {
  Serial.println(F("I2C-Scan startet..."));
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("Gefunden: 0x"));
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }
  Serial.println(F("I2C-Scan fertig."));
  Serial.println(F("Aktiver Servo Status:"));
  printServoState(g_activeServo);
}

void moveToAbzweig(uint8_t ch) {
  Serial.print(F("Servo "));
  Serial.print(ch);
  Serial.println(F(" -> ABZWEIG"));
  setServoRelative(ch, getRelTargetAbzweig(ch));
}

void moveToGerade(uint8_t ch) {
  Serial.print(F("Servo "));
  Serial.print(ch);
  Serial.println(F(" -> GERADE"));
  setServoRelative(ch, getRelTargetGerade(ch));
}

void testServo(uint8_t ch) {
  Serial.print(F("Servo-"));
  Serial.print(ch);
  Serial.println(F("-Test startet (-30 <-> +30 rel)..."));
  for (uint8_t i = 0; i < 6; i++) {
    setServoRelative(ch, -30);
    delay(700);
    setServoRelative(ch, 30);
    delay(700);
  }
  setServoRelative(ch, 0);
  Serial.println(F("Servo-Test fertig."));
}

void printHelp() {
  Serial.println();
  Serial.println(F("Befehle (reduziert):"));
  Serial.println(F("  h              -> Hilfe"));
  Serial.println(F("  s              -> I2C-Scan + Status aktiver Servo"));
  Serial.println(F("  u <0..15>      -> aktiven Servo waehlen"));
  Serial.println(F("  0              -> aktiver Servo auf Mitte (rel 0)"));
  Serial.println(F("  p              -> Alias zu 0 (Mitte)"));
  Serial.println(F("  1              -> aktiver Servo auf linken Softlimit-Punkt"));
  Serial.println(F("  2              -> aktiver Servo auf rechten Softlimit-Punkt"));
  Serial.println(F("  g              -> aktiver Servo auf GERADE"));
  Serial.println(F("  b              -> aktiver Servo auf ABZWEIG"));
  Serial.println(F("  o l|r          -> Abzweigseite fuer aktiven Servo setzen"));
  Serial.println(F("  f              -> Standardwerte fuer ALLE Servos (zero=90, -40/+40)"));
  Serial.println(F("  t              -> Testfahrt fuer aktiven Servo"));
  Serial.println(F("  v              -> alle Servo-Konfigurationen speichern"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);

  Serial.println(F("SX-Servo-Aufsatz Test gestartet."));
  Serial.println(F("PCA9685 Adresse: 0x40"));
  Serial.println(F("Hinweis: Externe 5V fuer Servo-V+ verwenden, GND gemeinsam."));

  if (loadConfig()) {
    Serial.println(F("EEPROM-Konfiguration (alle Servos) gefunden und geladen."));
  } else {
    Serial.println(F("Keine gueltige EEPROM-Konfiguration. Defaults fuer alle Servos aktiv."));
    setDefaultsForAllServos();
  }

  // Beim Start IMMER zuerst Mitte fuer alle Servos
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    setServoRelative(ch, 0);
  }
  Serial.println(F("Startposition: alle Servos auf Mitte (rel 0) gesetzt."));

  scanI2C();
  printHelp();
}

void loop() {
  if (!Serial.available()) return;
  char cmd = Serial.read();

  if (cmd == '\r' || cmd == '\n' || cmd == ' ' || cmd == '\t') return;
  if (cmd >= 'A' && cmd <= 'Z') cmd = cmd - 'A' + 'a';

  if (cmd == 'h') {
    printHelp();
  } else if (cmd == 's') {
    scanI2C();
  } else if (cmd == 'u') {
    int ch = Serial.parseInt();
    if (ch < 0 || ch >= SERVO_COUNT) {
      Serial.println(F("Ungueltiger Servo. 0..15"));
    } else {
      g_activeServo = (uint8_t)ch;
      Serial.print(F("Aktiver Servo gesetzt: "));
      Serial.println(g_activeServo);
      printServoState(g_activeServo);
    }
  } else if (cmd == '0' || cmd == 'p') {
    setServoRelative(g_activeServo, 0);
  } else if (cmd == '1') {
    setServoRelative(g_activeServo, g_cfg[g_activeServo].relMin);
  } else if (cmd == '2') {
    setServoRelative(g_activeServo, g_cfg[g_activeServo].relMax);
  } else if (cmd == 'f') {
    setDefaultsForAllServos();
    Serial.println(F("Defaults fuer alle Servos gesetzt: zero=90, limits=-40/+40, abzweig=links"));
    for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) setServoRelative(ch, 0);
  } else if (cmd == 'g') {
    moveToGerade(g_activeServo);
  } else if (cmd == 'b') {
    moveToAbzweig(g_activeServo);
  } else if (cmd == 'o') {
    char side = readNextTokenChar();
    if (side == 'l') {
      g_cfg[g_activeServo].divergingIsLeft = 1;
      Serial.print(F("Servo "));
      Serial.print(g_activeServo);
      Serial.println(F(": Abzweigseite gesetzt: links"));
      printServoState(g_activeServo);
    } else if (side == 'r') {
      g_cfg[g_activeServo].divergingIsLeft = 0;
      Serial.print(F("Servo "));
      Serial.print(g_activeServo);
      Serial.println(F(": Abzweigseite gesetzt: rechts"));
      printServoState(g_activeServo);
    } else {
      Serial.println(F("Ungueltig. Nutzung: o l   oder   o r"));
    }
  } else if (cmd == 'v') {
    saveConfig();
  } else if (cmd == 't') {
    testServo(g_activeServo);
  } else {
    Serial.print(F("Unbekannter Befehl: '"));
    Serial.print(cmd);
    Serial.println(F("' (h fuer Hilfe)"));
  }
}

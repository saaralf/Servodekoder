#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const uint16_t SERVO_MIN_TICK = 110;  // physisch ~0°
const uint16_t SERVO_MAX_TICK = 500;  // physisch ~180°

const uint8_t SERVO_COUNT = 16;
const uint8_t SERVO0_STEP = 1;  // feine Justierung mit +/-

// Referenzpunkt für RELATIV-Winkel (0 = Mittelstellung)
int16_t servo0ZeroPhys = 90;
int16_t servo0RelAngle = 0;   // -90..+90
int16_t servo0PhysAngle = 90; // 0..180

// Gelernte Softlimits (ohne Endschalter-Rückmeldung absolut wichtig)
int16_t servo0RelMin = -40;
int16_t servo0RelMax = 40;
bool divergingIsLeft = true;   // true: Abzweig liegt auf linker Seite
bool calibrationMode = false;  // true: volle -90..+90 zum Einlernen

struct Servo0Config {
  uint16_t magic;
  int16_t zeroPhys;
  int16_t relMin;
  int16_t relMax;
  int8_t divergingIsLeft;
};

const uint16_t CFG_MAGIC = 0x5A31;
const int CFG_ADDR = 0;

uint16_t angleToTick(uint8_t angle) {
  if (angle > 180) angle = 180;
  return map(angle, 0, 180, SERVO_MIN_TICK, SERVO_MAX_TICK);
}

void setServoAngle(uint8_t channel, uint8_t angle) {
  if (channel >= SERVO_COUNT) return;
  pwm.setPWM(channel, 0, angleToTick(angle));
}

void allServos(uint8_t angle) {
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) setServoAngle(ch, angle);
}

int16_t clampRel(int16_t rel) {
  if (rel < -90) rel = -90;
  if (rel > 90) rel = 90;

  if (!calibrationMode) {
    if (rel < servo0RelMin) rel = servo0RelMin;
    if (rel > servo0RelMax) rel = servo0RelMax;
  }
  return rel;
}

void printServo0State() {
  Serial.print(F("Servo0: rel="));
  Serial.print(servo0RelAngle);
  Serial.print(F("°, phys="));
  Serial.print(servo0PhysAngle);
  Serial.print(F("°, zeroPhys="));
  Serial.print(servo0ZeroPhys);
  Serial.print(F("°, limits=["));
  Serial.print(servo0RelMin);
  Serial.print(F(".."));
  Serial.print(servo0RelMax);
  Serial.print(F("], abzweig="));
  Serial.print(divergingIsLeft ? F("links") : F("rechts"));
  Serial.print(F(", mode="));
  Serial.println(calibrationMode ? F("CAL") : F("RUN"));
}

void setServo0Physical(int16_t phys) {
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;

  servo0PhysAngle = phys;
  servo0RelAngle = servo0PhysAngle - servo0ZeroPhys;
  servo0RelAngle = clampRel(servo0RelAngle);

  // rel wieder konsistent in phys zurückrechnen (falls geclamped)
  servo0PhysAngle = servo0ZeroPhys + servo0RelAngle;
  if (servo0PhysAngle < 0) servo0PhysAngle = 0;
  if (servo0PhysAngle > 180) servo0PhysAngle = 180;

  setServoAngle(0, (uint8_t)servo0PhysAngle);
  printServo0State();
}

void setServo0Relative(int16_t rel) {
  servo0RelAngle = clampRel(rel);

  int16_t phys = servo0ZeroPhys + servo0RelAngle;
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;
  servo0PhysAngle = phys;

  setServoAngle(0, (uint8_t)servo0PhysAngle);
  printServo0State();
}

void moveServo0Relative(int16_t delta) {
  setServo0Relative(servo0RelAngle + delta);
}

int16_t getRelTargetAbzweig() {
  return divergingIsLeft ? servo0RelMin : servo0RelMax;
}

int16_t getRelTargetGerade() {
  return divergingIsLeft ? servo0RelMax : servo0RelMin;
}

void moveToAbzweig() {
  Serial.println(F("Weiche -> ABZWEIG"));
  setServo0Relative(getRelTargetAbzweig());
}

void moveToGerade() {
  Serial.println(F("Weiche -> GERADE"));
  setServo0Relative(getRelTargetGerade());
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

bool configIsValid(const Servo0Config &cfg) {
  if (cfg.magic != CFG_MAGIC) return false;
  if (cfg.zeroPhys < 0 || cfg.zeroPhys > 180) return false;
  if (cfg.relMin < -90 || cfg.relMin > 90) return false;
  if (cfg.relMax < -90 || cfg.relMax > 90) return false;
  if (cfg.relMin >= cfg.relMax) return false;
  if (!(cfg.divergingIsLeft == 0 || cfg.divergingIsLeft == 1)) return false;
  return true;
}

void saveConfig() {
  Servo0Config cfg;
  cfg.magic = CFG_MAGIC;
  cfg.zeroPhys = servo0ZeroPhys;
  cfg.relMin = servo0RelMin;
  cfg.relMax = servo0RelMax;
  cfg.divergingIsLeft = divergingIsLeft ? 1 : 0;
  EEPROM.put(CFG_ADDR, cfg);
  Serial.println(F("Konfiguration in EEPROM gespeichert."));
}

void loadConfig() {
  Servo0Config cfg;
  EEPROM.get(CFG_ADDR, cfg);
  if (configIsValid(cfg)) {
    servo0ZeroPhys = cfg.zeroPhys;
    servo0RelMin = cfg.relMin;
    servo0RelMax = cfg.relMax;
    divergingIsLeft = (cfg.divergingIsLeft == 1);
    Serial.println(F("EEPROM-Konfiguration geladen."));
  } else {
    Serial.println(F("Keine gueltige EEPROM-Konfiguration, Defaults aktiv."));
  }
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
}

void printHelp() {
  Serial.println();
  Serial.println(F("Befehle:"));
  Serial.println(F("  h              -> Hilfe"));
  Serial.println(F("  s              -> I2C-Scan"));
  Serial.println(F("  p              -> Status Servo 0"));
  Serial.println(F("  0              -> Servo 0 auf rel 0"));
  Serial.println(F("  +              -> Servo 0 +1 Grad (rel)"));
  Serial.println(F("  -              -> Servo 0 -1 Grad (rel)"));
  Serial.println(F("  + <n>          -> Servo 0 +n Grad (rel)"));
  Serial.println(F("  - <n>          -> Servo 0 -n Grad (rel)"));
  Serial.println(F("  x <n>          -> Servo 0 absolut rel -90..+90"));
  Serial.println(F("  c 0 <w>        -> Servo 0 absolut phys 0..180"));
  Serial.println(F("  z <phys>       -> Nullpunkt setzen (0..180), z.B. z 90"));
  Serial.println(F("  f              -> empfohlene Defaults: zero=90, limits=-40/+40"));
  Serial.println(F("  g              -> Weiche GERADE (ziel = gegenueber Abzweigseite)"));
  Serial.println(F("  b              -> Weiche ABZWEIG (ziel = Schalterseite)"));
  Serial.println(F("  o l|r          -> Abzweigseite setzen: links oder rechts"));
  Serial.println(F("  1              -> Servo 0 auf linken Softlimit-Punkt"));
  Serial.println(F("  2              -> Servo 0 auf rechten Softlimit-Punkt"));
  Serial.println(F("  k              -> Kalibriermodus EIN (volle -90..+90)"));
  Serial.println(F("  n              -> Kalibriermodus AUS (Softlimits aktiv)"));
  Serial.println(F("  l              -> aktueller rel-Wert als LINKS-Limit speichern"));
  Serial.println(F("  r              -> aktueller rel-Wert als RECHTS-Limit speichern"));
  Serial.println(F("  v              -> Limits/Nullpunkt in EEPROM speichern"));
  Serial.println(F("  t              -> Servo-0-Test (-30 <-> +30 rel)"));
  Serial.println(F("  a <winkel>     -> alle 16 Servos phys 0..180"));
  Serial.println();
}

void testServo0() {
  Serial.println(F("Servo-0-Test startet (-30 <-> +30 rel)..."));
  for (uint8_t i = 0; i < 6; i++) {
    setServo0Relative(-30);
    delay(700);
    setServo0Relative(30);
    delay(700);
  }
  setServo0Relative(0);
  Serial.println(F("Servo-0-Test fertig."));
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

  loadConfig();
  scanI2C();

  calibrationMode = false;
  setServo0Relative(0);  // sicher in die Mitte relativ zum Nullpunkt
  printHelp();
}

void loop() {
  if (!Serial.available()) return;
  char cmd = Serial.read();

  // Leerzeichen / CR / LF ignorieren (wichtig fuer Monitor mit CRLF)
  if (cmd == '\r' || cmd == '\n' || cmd == ' ' || cmd == '\t') return;

  // Grossbuchstaben auf Kleinbuchstaben abbilden
  if (cmd >= 'A' && cmd <= 'Z') cmd = cmd - 'A' + 'a';

  if (cmd == 'h') {
    printHelp();
  } else if (cmd == 's') {
    scanI2C();
  } else if (cmd == 'p') {
    printServo0State();
  } else if (cmd == '0') {
    setServo0Relative(0);
  } else if (cmd == '1') {
    setServo0Relative(servo0RelMin);
  } else if (cmd == '2') {
    setServo0Relative(servo0RelMax);
  } else if (cmd == '+') {
    int step = Serial.parseInt();
    if (step <= 0) step = SERVO0_STEP;
    if (step > 90) step = 90;
    int16_t before = servo0RelAngle;
    moveServo0Relative(step);
    if (!calibrationMode && servo0RelAngle == before) {
      Serial.println(F("Softlimit erreicht. Fuer weitere Justage zuerst 'k' (Kalibriermodus)."));
    }
  } else if (cmd == '-') {
    int step = Serial.parseInt();
    if (step <= 0) step = SERVO0_STEP;
    if (step > 90) step = 90;
    int16_t before = servo0RelAngle;
    moveServo0Relative(-step);
    if (!calibrationMode && servo0RelAngle == before) {
      Serial.println(F("Softlimit erreicht. Fuer weitere Justage zuerst 'k' (Kalibriermodus)."));
    }
  } else if (cmd == 'x') {
    int rel = Serial.parseInt();
    setServo0Relative(rel);
  } else if (cmd == 'z') {
    int z = Serial.parseInt();
    if (z < 0) z = 0;
    if (z > 180) z = 180;
    servo0ZeroPhys = z;
    Serial.print(F("Neuer Zero-Phys gesetzt: "));
    Serial.println(servo0ZeroPhys);
    setServo0Relative(0);
  } else if (cmd == 'f') {
    servo0ZeroPhys = 90;
    servo0RelMin = -40;
    servo0RelMax = 40;
    divergingIsLeft = true;
    Serial.println(F("Defaults gesetzt: zero=90, limits=-40/+40, abzweig=links"));
    setServo0Relative(0);
  } else if (cmd == 'g') {
    moveToGerade();
  } else if (cmd == 'b') {
    moveToAbzweig();
  } else if (cmd == 'o') {
    char side = readNextTokenChar();
    if (side == 'l') {
      divergingIsLeft = true;
      Serial.println(F("Abzweigseite gesetzt: links"));
      printServo0State();
    } else if (side == 'r') {
      divergingIsLeft = false;
      Serial.println(F("Abzweigseite gesetzt: rechts"));
      printServo0State();
    } else {
      Serial.println(F("Ungueltig. Nutzung: o l   oder   o r"));
    }
  } else if (cmd == 'k') {
    calibrationMode = true;
    Serial.println(F("Kalibriermodus EIN (volle -90..+90 ohne Softlimit-Klemme)."));
    printServo0State();
  } else if (cmd == 'n') {
    calibrationMode = false;
    Serial.println(F("Kalibriermodus AUS (Softlimits aktiv)."));
    setServo0Relative(servo0RelAngle);
  } else if (cmd == 'l') {
    servo0RelMin = servo0RelAngle;
    if (servo0RelMin >= servo0RelMax) {
      servo0RelMin = servo0RelMax - 1;
      if (servo0RelMin < -90) servo0RelMin = -90;
    }
    Serial.print(F("Neues LINKS-Limit (RAM): "));
    Serial.println(servo0RelMin);
    Serial.println(F("Hinweis: mit 'v' dauerhaft in EEPROM speichern."));
    printServo0State();
  } else if (cmd == 'r') {
    servo0RelMax = servo0RelAngle;
    if (servo0RelMax <= servo0RelMin) {
      servo0RelMax = servo0RelMin + 1;
      if (servo0RelMax > 90) servo0RelMax = 90;
    }
    Serial.print(F("Neues RECHTS-Limit (RAM): "));
    Serial.println(servo0RelMax);
    Serial.println(F("Hinweis: mit 'v' dauerhaft in EEPROM speichern."));
    printServo0State();
  } else if (cmd == 'v') {
    saveConfig();
  } else if (cmd == 't') {
    testServo0();
  } else if (cmd == 'a') {
    int w = Serial.parseInt();
    if (w < 0) w = 0;
    if (w > 180) w = 180;
    allServos((uint8_t)w);
    Serial.print(F("Alle Servos -> phys "));
    Serial.println(w);
  } else if (cmd == 'c') {
    int ch = Serial.parseInt();
    int w = Serial.parseInt();
    if (ch < 0 || ch >= SERVO_COUNT) {
      Serial.println(F("Ungueltiger Kanal. 0..15"));
      return;
    }
    if (w < 0) w = 0;
    if (w > 180) w = 180;

    if (ch == 0) {
      setServo0Physical(w);
    } else {
      setServoAngle((uint8_t)ch, (uint8_t)w);
      Serial.print(F("Kanal "));
      Serial.print(ch);
      Serial.print(F(" -> phys "));
      Serial.println(w);
    }
  } else {
    Serial.print(F("Unbekannter Befehl: '"));
    Serial.print(cmd);
    Serial.println(F("' (h fuer Hilfe)"));
  }
}

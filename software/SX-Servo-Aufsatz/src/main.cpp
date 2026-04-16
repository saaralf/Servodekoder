#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const uint16_t SERVO_MIN_TICK = 110;  // physisch ~0°
const uint16_t SERVO_MAX_TICK = 500;  // physisch ~180°

const uint8_t SERVO_COUNT = 16;
const uint8_t SERVO0_STEP = 5;

// Referenzpunkt fuer RELATIV-Winkel (0 = Mittelstellung)
// Standard: 90 => damit sind -90..+90 voll erreichbar.
int16_t servo0ZeroPhys = 90;
int16_t servo0RelAngle = 0;   // -90..+90
int16_t servo0PhysAngle = 90; // 0..180

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

void printServo0State() {
  Serial.print(F("Servo0: rel="));
  Serial.print(servo0RelAngle);
  Serial.print(F("°, phys="));
  Serial.print(servo0PhysAngle);
  Serial.print(F("°, zeroPhys="));
  Serial.print(servo0ZeroPhys);
  Serial.println(F("°"));
}

void setServo0Physical(int16_t phys) {
  if (phys < 0) phys = 0;
  if (phys > 180) phys = 180;

  servo0PhysAngle = phys;
  servo0RelAngle = servo0PhysAngle - servo0ZeroPhys;
  if (servo0RelAngle < -90) servo0RelAngle = -90;
  if (servo0RelAngle > 90) servo0RelAngle = 90;

  setServoAngle(0, (uint8_t)servo0PhysAngle);
  printServo0State();
}

void setServo0Relative(int16_t rel) {
  if (rel < -90) rel = -90;
  if (rel > 90) rel = 90;

  servo0RelAngle = rel;
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
  Serial.println(F("  +              -> Servo 0 +5 Grad (rel)"));
  Serial.println(F("  -              -> Servo 0 -5 Grad (rel)"));
  Serial.println(F("  + <n>          -> Servo 0 +n Grad (rel)"));
  Serial.println(F("  - <n>          -> Servo 0 -n Grad (rel)"));
  Serial.println(F("  x <n>          -> Servo 0 absolut rel -90..+90"));
  Serial.println(F("  c 0 <w>        -> Servo 0 absolut phys 0..180"));
  Serial.println(F("  z <phys>       -> Nullpunkt setzen (0..180), z.B. z 90"));
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

  scanI2C();
  setServo0Relative(0);
  printHelp();
}

void loop() {
  if (!Serial.available()) return;
  char cmd = Serial.read();

  if (cmd == 'h') {
    printHelp();
  } else if (cmd == 's') {
    scanI2C();
  } else if (cmd == 'p') {
    printServo0State();
  } else if (cmd == '0') {
    setServo0Relative(0);
  } else if (cmd == '+') {
    int step = Serial.parseInt();
    if (step <= 0) step = SERVO0_STEP;
    if (step > 90) step = 90;
    moveServo0Relative(step);
  } else if (cmd == '-') {
    int step = Serial.parseInt();
    if (step <= 0) step = SERVO0_STEP;
    if (step > 90) step = 90;
    moveServo0Relative(-step);
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
  }
}

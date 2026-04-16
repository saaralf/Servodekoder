#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// PCA9685 auf dem ServoAufsatz
// A0..A5 sind fest auf GND -> I2C-Adresse 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// ---------- Servo-Kalibrierung ----------
// 50Hz => 20ms Periode, 12-bit -> 4096 Ticks
// Typisch: 0.5ms..2.5ms => ca. 102..512 Ticks
const uint16_t SERVO_MIN_TICK = 110;  // physisch ~0°
const uint16_t SERVO_MAX_TICK = 500;  // physisch ~180°

const uint8_t SERVO_COUNT = 16;
const uint8_t SERVO0_STEP = 5;        // Schrittweite für +/-
int8_t servo0SignedAngle = 0;         // -90..+90, 0 = Mittelstellung

uint16_t angleToTick(uint8_t angle) {
  if (angle > 180) angle = 180;
  return map(angle, 0, 180, SERVO_MIN_TICK, SERVO_MAX_TICK);
}

void setServoAngle(uint8_t channel, uint8_t angle) {
  if (channel >= SERVO_COUNT) return;
  uint16_t tick = angleToTick(angle);
  pwm.setPWM(channel, 0, tick);
}

void allServos(uint8_t angle) {
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    setServoAngle(ch, angle);
  }
}

void setServo0Signed(int16_t signedAngle) {
  if (signedAngle < -90) signedAngle = -90;
  if (signedAngle > 90) signedAngle = 90;

  servo0SignedAngle = (int8_t)signedAngle;
  uint8_t physicalAngle = (uint8_t)(servo0SignedAngle + 90);  // -90..+90 -> 0..180
  setServoAngle(0, physicalAngle);

  Serial.print(F("Servo 0 signed -> "));
  Serial.print(servo0SignedAngle);
  Serial.print(F(" (phys="));
  Serial.print(physicalAngle);
  Serial.println(F("°)"));
}

void setServo0Physical(uint8_t physicalAngle) {
  if (physicalAngle > 180) physicalAngle = 180;
  setServoAngle(0, physicalAngle);
  servo0SignedAngle = (int8_t)physicalAngle - 90;

  Serial.print(F("Servo 0 phys -> "));
  Serial.print(physicalAngle);
  Serial.print(F(" (signed="));
  Serial.print(servo0SignedAngle);
  Serial.println(F(")"));
}

void moveServo0Relative(int8_t delta) {
  int16_t next = (int16_t)servo0SignedAngle + delta;
  setServo0Signed(next);
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
  Serial.println(F("  a <winkel>     -> alle 16 Servos auf Winkel (0..180 physisch)"));
  Serial.println(F("  c <ch> <w>     -> Kanal ch (0..15) auf Winkel w (0..180 physisch)"));
  Serial.println(F("  d              -> Demofahrt"));
  Serial.println(F("  t              -> Servo-0-Test (-30/+30)"));
  Serial.println(F("  0              -> Servo 0 auf signed 0 (Mitte)"));
  Serial.println(F("  +              -> Servo 0 +5 Grad (signed)"));
  Serial.println(F("  -              -> Servo 0 -5 Grad (signed)"));
  Serial.println(F("  + <n>          -> Servo 0 +n Grad (signed, z.B. + 30)"));
  Serial.println(F("  - <n>          -> Servo 0 -n Grad (signed, z.B. - 30)"));
  Serial.println(F("  x <n>          -> Servo 0 absolut signed -90..+90"));
  Serial.println();
}

void demoSweep() {
  Serial.println(F("Demo startet..."));
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++) {
    setServoAngle(ch, 20);
    delay(100);
    setServoAngle(ch, 160);
    delay(100);
    setServoAngle(ch, 90);
  }
  Serial.println(F("Demo fertig."));
}

void testServo0() {
  Serial.println(F("Servo-0-Test startet (-30 <-> +30)..."));
  for (uint8_t i = 0; i < 6; i++) {
    setServo0Signed(-30);
    delay(700);
    setServo0Signed(30);
    delay(700);
  }
  setServo0Signed(0);
  Serial.println(F("Servo-0-Test fertig. Servo 0 steht auf signed 0."));
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin();  // A4=SDA, A5=SCL am Pro Mini
  pwm.begin();
  pwm.setPWMFreq(50);  // Servo-typisch 50Hz
  delay(10);

  Serial.println(F("ServoTest fuer PCA9685 gestartet."));
  Serial.println(F("Adresse erwartet: 0x40"));
  Serial.println(F("Wichtig: Externe 5V fuer Servo-V+ verwenden, GND gemeinsam."));

  scanI2C();

  // Grundstellung: signed 0 (Mitte)
  setServo0Signed(0);
  printHelp();
}

void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (cmd == 'h') {
    printHelp();
  } else if (cmd == 's') {
    scanI2C();
  } else if (cmd == 'd') {
    demoSweep();
  } else if (cmd == 't') {
    testServo0();
  } else if (cmd == '0') {
    setServo0Signed(0);
  } else if (cmd == '+') {
    int step = Serial.parseInt();
    if (step <= 0) step = SERVO0_STEP;
    if (step > 90) step = 90;
    moveServo0Relative((int8_t)step);
  } else if (cmd == '-') {
    int step = Serial.parseInt();
    if (step <= 0) step = SERVO0_STEP;
    if (step > 90) step = 90;
    moveServo0Relative((int8_t)(-step));
  } else if (cmd == 'x') {
    int signedAngle = Serial.parseInt();
    setServo0Signed(signedAngle);
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
      setServo0Physical((uint8_t)w);
    } else {
      setServoAngle((uint8_t)ch, (uint8_t)w);
      Serial.print(F("Kanal "));
      Serial.print(ch);
      Serial.print(F(" -> phys "));
      Serial.println(w);
    }
  }
}

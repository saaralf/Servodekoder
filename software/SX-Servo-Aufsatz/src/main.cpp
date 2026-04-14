#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

const uint8_t LED_PIN = LED_BUILTIN;

// PCA9685 Standardadresse
Adafruit_PWMServoDriver pwm(0x40);

// Servokalibrierung (bei Bedarf pro Servo anpassen)
const uint16_t SERVO_MIN_TICK = 110;
const uint16_t SERVO_MAX_TICK = 500;
const uint8_t SERVO_CHANNEL = 0; // Servo 0

unsigned long lastHeartbeat = 0;
unsigned long lastServoMove = 0;
bool ledState = false;
bool servoAtMax = false;

void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  delay(300);
  Serial.println("[SX-Servo-Aufsatz] Start");

  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50); // 50 Hz fuer Servos
  delay(10);

  // Startposition: MIN
  pwm.setPWM(SERVO_CHANNEL, 0, SERVO_MIN_TICK);
  Serial.println("PCA9685 initialisiert, Servo 0 -> MIN");
}

void loop() {
  const unsigned long now = millis();

  // Heartbeat + LED jede Sekunde
  if (now - lastHeartbeat >= 1000) {
    lastHeartbeat = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    Serial.print("Heartbeat, uptime(ms)=");
    Serial.println(now);
  }

  // Servo 0 alle 2 Sekunden zwischen MIN und MAX umschalten
  if (now - lastServoMove >= 2000) {
    lastServoMove = now;
    servoAtMax = !servoAtMax;

    const uint16_t target = servoAtMax ? SERVO_MAX_TICK : SERVO_MIN_TICK;
    pwm.setPWM(SERVO_CHANNEL, 0, target);

    Serial.print("Servo 0 -> ");
    Serial.println(servoAtMax ? "MAX" : "MIN");
  }
}

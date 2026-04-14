#include <Arduino.h>

const uint8_t LED_PIN = LED_BUILTIN;
unsigned long lastMsg = 0;
bool ledState = false;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(200);
  Serial.println("[SX-Servo-Aufsatz] Serial Monitor OK");
}

void loop() {
  const unsigned long now = millis();
  if (now - lastMsg >= 1000) {
    lastMsg = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    Serial.print("Heartbeat, uptime(ms)=");
    Serial.println(now);
  }
}

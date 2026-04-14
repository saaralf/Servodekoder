// Schneller LED-Blink-Test
// Board: Arduino Uno (PlatformIO env:uno)

const uint8_t LED_PIN = LED_BUILTIN; // beim UNO = D13

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
}

// Task 0.4: first sketch. Blinks an LED and lets a button keep it on.
#include <Arduino.h>

constexpr uint8_t LED_PIN = 2;
constexpr uint8_t BUTTON_PIN = 4;  // Button A in the pin plan
constexpr uint32_t BLINK_INTERVAL_MS = 500;

bool ledOn = false;
bool wasPressed = false;
uint32_t lastToggleMs = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  // The internal pull-up keeps the pin HIGH; pressing the button connects it to GND (LOW).
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Merhaba PixelCharm!");
}

void loop() {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;

  if (pressed != wasPressed) {
    Serial.println(pressed ? "Butona basildi: LED surekli yanik" : "Buton birakildi: LED yanip sonuyor");
    wasPressed = pressed;
  }

  if (pressed) {
    ledOn = true;
  } else if (millis() - lastToggleMs >= BLINK_INTERVAL_MS) {
    // millis() instead of delay() so the button is still read while we wait
    ledOn = !ledOn;
    lastToggleMs = millis();
  }

  digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
}

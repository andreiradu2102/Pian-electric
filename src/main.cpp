#include <Arduino.h>

const byte BUZZER_PIN = 9;

const byte keyPins[8] = {2, 3, 4, 5, 6, 7, A0, A1};

const int  freqs[8]  = {262, 294, 330, 349, 392, 440, 494, 523};

int currentKey = -1;

void setup() {
  for (byte i = 0; i < 8; i++) {
    pinMode(keyPins[i], INPUT_PULLUP);
  }
}

void loop() {
  int pressed = -1;

  for (byte i = 0; i < 8; i++) {
    if (digitalRead(keyPins[i]) == LOW) {
      pressed = i;
      break;
    }
  }

  if (pressed != currentKey) {
    if (pressed == -1) {
      noTone(BUZZER_PIN);
    } else {
      tone(BUZZER_PIN, freqs[pressed]);
    }
    currentKey = pressed;
  }

  delay(5);
}

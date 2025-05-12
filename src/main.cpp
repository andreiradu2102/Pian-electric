#include <Arduino.h>
#include <Wire.h>

const int BUZZ = 9;

const int notes[] = {262, 294, 330, 349, 392, 440, 494, 523};

void setup() {
  for (byte i = 0; i < 8; i++) {
    tone(BUZZ, notes[i]);
    delay(300);
    noTone(BUZZ);
    delay(50);
  }
}

void loop() {
}

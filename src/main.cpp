#include <Arduino.h>

const byte BUZZ = 9;
const byte KEY_PINS[8] = {2,3,4,5,6,7, A0, A1};
const int  FREQS[8]   = {262,294,330,349,392,440,494,523};

int lastFreq = 0;

void setup() {
  for (byte i = 0; i < 8; i++) {
    pinMode(KEY_PINS[i], INPUT_PULLUP);
  }
}

void loop() {
  long sum = 0;
  int  count = 0;

  for (byte i = 0; i < 8; i++) {
    if (digitalRead(KEY_PINS[i]) == LOW) {
      sum   += FREQS[i];
      count += 1;
    }
  }

  int newFreq = (count > 0) ? (sum / count) : 0;

  if (newFreq != lastFreq) {
    if (newFreq == 0) {
      noTone(BUZZ);
    } else {
      tone(BUZZ, newFreq);
    }
    lastFreq = newFreq;
  }

  delay(5); 
}

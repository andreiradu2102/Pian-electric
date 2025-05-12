#include <Arduino.h>

const byte BUZZ = 9;
const byte KEY_PINS[8] = {2,3,4,5,6,7, A0, A1};
const int  FREQS[8]   = {262,294,330,349,392,440,494,523};

int lastFreq = 0;

float freqToMidi(float f) {
  return 12.0f * (log(f/440.0f)/log(2.0f)) + 69.0f;
}

float midiToFreq(float m) {
  return 440.0f * pow(2.0f, (m-69.0f)/12.0f);
}

void setup() {
  for (byte i = 0; i < 8; i++) {
    pinMode(KEY_PINS[i], INPUT_PULLUP);
  }
}

void loop() {
  float sumMidi = 0.0;
  int   count   = 0;

  for (byte i = 0; i < 8; i++) {
    if (digitalRead(KEY_PINS[i]) == LOW) {
      sumMidi += freqToMidi(FREQS[i]);
      count++;
    }
  }

  int newFreq;
  if (count > 0) {
    float midiAvg = sumMidi / count;
    newFreq = int(midiToFreq(midiAvg) + 0.5);
  } else {
    newFreq = 0;                 
  }

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

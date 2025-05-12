#include <Arduino.h>
#include <math.h>

const byte DATA_PIN  = 8;
const byte CLOCK_PIN = 12;
const byte LATCH_PIN = 13;

const byte BUZZ = 9;
const byte KEY_PINS[8] = {2,3,4,5,6,7, A0, A1};
const int  FREQS[8]   = {262,294,330,349,392,440,494,523};

byte lastMask   = 0;
int  lastFreq = 0; 

void sendByte(byte val) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, val);
  digitalWrite(LATCH_PIN, HIGH);
}

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

  pinMode(DATA_PIN,  OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  sendByte(0);
}

void loop() {
  byte  mask     = 0;
  float sumMidi = 0.0;
  int   count   = 0;

  for (byte i = 0; i < 8; i++) {
    if (digitalRead(KEY_PINS[i]) == LOW) {
      mask |= (1 << i);
      sumMidi += freqToMidi(FREQS[i]);
      count++;
    }
  }

  if (mask != lastMask) {
    sendByte(mask);
    lastMask = mask;
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

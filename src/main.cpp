#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <math.h>

// ─────────── LCD I2C ───────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─────────── Pin-mapping ───────────
#define SHIFT_SER    8    
#define SHIFT_CLOCK  A2   
#define SHIFT_LATCH  A3   

#define SD_CS        4   
#define BUZZ_PIN     9

#define BTN_NEXT     0    
#define BTN_OK       1    

const byte KEY_PINS[8] = {2, 3, 10, 5, 6, 7, A0, A1};
const int  FREQS[8]    = {262,294,330,349,392,440,494,523};

enum State { ST_MENU, ST_FREE, ST_SONG_SELECT, ST_SONG_PLAY };
static State state;
static byte  lastMask   = 0;
static int   lastFreqHz = 0;

#define MAX_SONGS 10
char songNames[MAX_SONGS][13];
byte songCount = 0, songSel = 0;

// ─────────── Helpers ───────────
inline void sendMask(byte m) {
  digitalWrite(SHIFT_LATCH, LOW);
  shiftOut(SHIFT_SER, SHIFT_CLOCK, MSBFIRST, m);
  digitalWrite(SHIFT_LATCH, HIGH);
}

inline float freqToMidi(float f) {
  return 12.0f * (log(f/440.0f)/log(2.0f)) + 69.0f;
}
inline float midiToFreq(float m) {
  return 440.0f * pow(2.0f, (m-69.0f)/12.0f);
}

byte readKeysMask() {
  byte m = 0;
  for (byte i = 0; i < 8; i++)
    if (digitalRead(KEY_PINS[i]) == LOW) m |= (1<<i);
  return m;
}

// ─────────── Setup ───────────
void setup() {
  Wire.begin();
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin();

  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_OK,   INPUT_PULLUP);
  for (byte i = 0; i < 8; i++) pinMode(KEY_PINS[i], INPUT_PULLUP);

  pinMode(SHIFT_SER,   OUTPUT);
  pinMode(SHIFT_CLOCK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  sendMask(0);

  pinMode(BUZZ_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Init Pian...");
  lcd.setCursor(0,1); lcd.print("Verificare...");
  delay(1000);

  if (!SD.begin(SD_CS)) {
    lcd.clear(); lcd.setCursor(0,0);
    lcd.print("SD init FAIL");
    delay(1500);
  } else {
    lcd.clear(); lcd.setCursor(0,0);
    lcd.print("SD OK");
    delay(800);
  }

  state = ST_MENU;
}

// ─────────── Loop ───────────
void loop() {
  switch (state) {
    case ST_MENU:        menuTask();        break;
    case ST_FREE:        freePlayTask();    break;
    case ST_SONG_SELECT: songSelectTask();  break;
    case ST_SONG_PLAY:   songPlayTask();    break;
    default:             state = ST_MENU;   break;
  }
}

// ───────────── MENIU ─────────────
void menuTask() {
  byte sel = 0;
  while (true) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(sel ? " Free-Play" : ">Free-Play");
    lcd.setCursor(0,1); lcd.print(sel ? ">Song Mode" : " Song Mode");
    if (!digitalRead(BTN_NEXT)) { sel ^= 1; delay(200); }
    if (!digitalRead(BTN_OK)) {
      delay(200);
      state = sel ? ST_SONG_SELECT : ST_FREE;
      return;
    }
  }
}

// ───────────── FREE-PLAY ─────────────
void freePlayTask() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Free-Play (OK)");
  while (true) {
    if (!digitalRead(BTN_OK)) { delay(200); state = ST_MENU; sendMask(0); noTone(BUZZ_PIN); return; }

    byte mask = 0; float sum=0; int cnt=0;
    for (byte i=0; i<8; i++) {
      if (digitalRead(KEY_PINS[i]) == LOW) {
        mask |= (1<<i);
        sum += freqToMidi(FREQS[i]);
        cnt++;
      }
    }
    if (mask != lastMask) { sendMask(mask); lastMask = mask; }
    int f = cnt ? int(midiToFreq(sum/cnt)+0.5f) : 0;
    if (f != lastFreqHz) {
      if (f) tone(BUZZ_PIN,f); else noTone(BUZZ_PIN);
      lastFreqHz = f;
    }
    delay(5);
  }
}

// ────────── SONG SELECT ──────────
void songSelectTask() {
  songCount = 0;
  File root = SD.open("/");
  while (File f = root.openNextFile()) {
    if (!f.isDirectory() && strstr(f.name(), ".TXT") && songCount < MAX_SONGS) {
      strncpy(songNames[songCount], f.name(), 12);
      songNames[songCount][12] = 0;
      songCount++;
    }
    f.close();
  }
  root.close();
  if (songCount == 0) {
    lcd.clear(); lcd.print("No .txt songs!");
    delay(1500);
    state = ST_MENU;
    return;
  }
  byte sel = 0;
  while (true) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Select song:");
    lcd.setCursor(0,1); lcd.print(songNames[sel]);
    if (!digitalRead(BTN_NEXT)) { sel = (sel+1) % songCount; delay(200); }
    if (!digitalRead(BTN_OK))  { songSel = sel; delay(200); state = ST_SONG_PLAY; return; }
  }
}

// ─────────── SONG PLAY ───────────
void songPlayTask() {
  File sf = SD.open(songNames[songSel]);
  if (!sf) {
    lcd.clear(); lcd.print("Open fail");
    delay(1500);
    state = ST_SONG_SELECT;
    return;
  }
  lcd.clear(); lcd.setCursor(0,0); lcd.print(songNames[songSel]);
  lcd.setCursor(0,1); lcd.print("Playing...");
  delay(500);

  while (sf.available()) {
    String line = sf.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.charAt(0)=='#') continue;

    int sp = line.indexOf(' ');
    if (sp < 1) continue;
    String notes = line.substring(0, sp);
    int dur = line.substring(sp+1).toInt();

    byte mask = 0;
    int comma;
    int start = 0;
    while (start < notes.length()) {
      comma = notes.indexOf(',', start);
      String tok = notes.substring(start, comma<0 ? notes.length() : comma);
      int idx = tok.toInt();
      if (idx >= 0 && idx < 8) mask |= (1<<idx);
      if (comma<0) break;
      start = comma+1;
    }
    if (mask == 0) { delay(dur); continue; }

    float sumF=0; int c=0;
    for (byte i=0; i<8; i++) if (mask & (1<<i)) { sumF += FREQS[i]; c++; }
    int freq = int(sumF / c + 0.5f);

    tone(BUZZ_PIN, freq);
    for (byte step=0; step<8; step++) {
      sendMask(step<8 ? mask : 0);
      unsigned long t0 = millis();
      bool ok=false;
      while (millis() - t0 < dur/8) {
        if ((mask & readKeysMask()) == mask) { ok=true; break; }
      }
      if (ok) break;
    }
    sendMask(0);
    noTone(BUZZ_PIN);

    if ((mask & readKeysMask()) != mask) {
      lcd.clear(); lcd.setCursor(0,0); lcd.print("  ERROR!");
      tone(BUZZ_PIN, 200, 500);
      delay(700);
      sf.close();
      state = ST_SONG_SELECT;
      return;
    }
  }

  sf.close();
  lcd.clear(); lcd.setCursor(0,0); lcd.print("  Finished!");
  tone(BUZZ_PIN, 523, 500);
  delay(800);
  state = ST_MENU;
}

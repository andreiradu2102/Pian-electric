// Include Wire library for I²C communication
#include <Wire.h>

// Include SPI library for hardware SPI support
#include <SPI.h>

// Include I²C LCD wrapper library
#include <LiquidCrystal_I2C.h>

// Include SD library for filesystem access on MicroSD
#include <SD.h>

// Include math library for log()/pow() conversions
#include <math.h>

// ───────────── Configuration Constants ─────────────

// I²C address and dimensions of the LCD
#define LCD_I2C_ADDRESS   0x27
#define LCD_COLUMNS       16
#define LCD_ROWS          2

// Pin assignments for 74HC595 shift register
#define PIN_SHIFT_DATA    8    // DS pin of 74HC595
#define PIN_SHIFT_CLOCK   A2   // SH_CP pin of 74HC595
#define PIN_SHIFT_LATCH   A3   // ST_CP pin of 74HC595

// Chip-select pin for the MicroSD module
#define PIN_SD_CS         4

// Pin driving the piezo buzzer
#define PIN_BUZZER        9

// Menu navigation buttons
#define PIN_BUTTON_NEXT   0    // D0
#define PIN_BUTTON_OK     1    // D1

// Number of keys (and LEDs) in the keyboard
#define NUM_KEYS          8

// Maximum number of songs the device can list
#define MAX_SONGS         20

// Debounce and long-press thresholds (milliseconds)
#define DEBOUNCE_DELAY    200
#define LONG_PRESS_TIME   5000
#define SHORT_PRESS_TIME  300

// Array of digital pins connected to the 8 tactile keys
const byte KEY_PINS[NUM_KEYS] = { 2, 3, 10, 5, 6, 7, A0, A1 };

// Frequencies (Hz) corresponding to each key index 0…7
const int KEY_FREQUENCIES[NUM_KEYS] = { 262, 294, 330, 349, 392, 440, 494, 523 };

// ───────────── Global State ─────────────

// LCD object using the I²C backpack
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// Finite-state machine states
enum State {
    STATE_MENU,
    STATE_FREE_PLAY,
    STATE_SONG_SELECT,
    STATE_SONG_PLAY
};
static State currentState;

// Last output bitmask sent to the shift register
static byte lastOutputMask = 0;

// Last frequency generated on the buzzer
static int lastFrequencyHz = 0;

// Storage for discovered song filenames
char songNames[MAX_SONGS][13];
byte songCount = 0;
byte selectedSong = 0;

// ───────────── Helper Functions ─────────────

/*
 * reverseBits(byte m)
 * Reverse the bit order of m (bit 0↔7, 1↔6, …)
 * This aligns our logical key indices with the physical wiring of the shift register.
 */
static inline byte reverseBits(byte m)
{
    byte result = 0;
    for (uint8_t i = 0; i < NUM_KEYS; i++) {
        if (m & (1 << i)) {
            result |= 1 << (NUM_KEYS - 1 - i);
        }
    }
    return result;
}

/*
 * sendMask(byte mask)
 * Shift out 8 bits to the 74HC595 and latch the outputs.
 * Uses reverseBits() to map logic order to physical order.
 */
static inline void sendMask(byte mask)
{
    byte dataToSend = reverseBits(mask);
    digitalWrite(PIN_SHIFT_LATCH, LOW);
    shiftOut(PIN_SHIFT_DATA, PIN_SHIFT_CLOCK, MSBFIRST, dataToSend);
    digitalWrite(PIN_SHIFT_LATCH, HIGH);
}

/*
 * freqToMidi(float freq)
 * Convert a frequency in Hz to the corresponding MIDI note number (floating point).
 * Formula: MIDI = 12 * log2(freq/440) + 69
 */
static inline float freqToMidi(float freq)
{
    return 12.0f * (log(freq / 440.0f) / log(2.0f)) + 69.0f;
}

/*
 * midiToFreq(float midi)
 * Convert a MIDI note number (floating point) back to its frequency in Hz.
 * Formula: freq = 440 * 2^((midi - 69) / 12)
 */
static inline float midiToFreq(float midi)
{
    return 440.0f * pow(2.0f, (midi - 69.0f) / 12.0f);
}

/*
 * readKeysMask()
 * Read the state of the 8 key input pins (active LOW, pulled up internally).
 * Return a bitmask with bit i set if key i is pressed.
 */
static byte readKeysMask()
{
    byte mask = 0;
    for (byte i = 0; i < NUM_KEYS; i++) {
        if (digitalRead(KEY_PINS[i]) == LOW) {
            mask |= 1 << i;
        }
    }
    return mask;
}

// ───────────── Setup and Main Loop ─────────────

/*
 * setup()
 * Initialize all hardware interfaces: I²C for LCD, SPI for SD,
 * GPIO modes, shift register, buzzer pin, and start in the MENU state.
 */
void setup()
{
    // Start I²C bus for the LCD
    Wire.begin();

    // Configure SD chip-select as output and deselect initially
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    // Start the hardware SPI interface
    SPI.begin();

    // Configure user buttons with internal pull-ups
    pinMode(PIN_BUTTON_NEXT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_OK,   INPUT_PULLUP);

    // Configure each key pin with internal pull-up
    for (byte i = 0; i < NUM_KEYS; i++) {
        pinMode(KEY_PINS[i], INPUT_PULLUP);
    }

    // Configure shift register pins as outputs
    pinMode(PIN_SHIFT_DATA,  OUTPUT);
    pinMode(PIN_SHIFT_CLOCK, OUTPUT);
    pinMode(PIN_SHIFT_LATCH, OUTPUT);

    // Clear any outputs on the shift register
    sendMask(0);

    // Configure the buzzer pin as output
    pinMode(PIN_BUZZER, OUTPUT);

    // Initialize the LCD and turn on its backlight
    lcd.init();
    lcd.backlight();

    // Display a startup message on the LCD
    lcd.setCursor(0, 0);
    lcd.print("Init Piano...");
    lcd.setCursor(0, 1);
    lcd.print("Checking SD...");
    delay(1000);

    // Attempt to initialize the SD card at half SPI speed
    if (!SD.begin(SPI_HALF_SPEED, PIN_SD_CS)) {
        // If initialization fails, show error and halt for a moment
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SD init FAIL");
        delay(1500);
    } else {
        // On success, indicate OK and proceed
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SD OK");
        delay(800);
    }

    // Enter the main menu state
    currentState = STATE_MENU;
}

/*
 * loop()
 * Dispatch control to the function matching the current FSM state.
 */
void loop()
{
    switch (currentState) {
        case STATE_MENU:
            menuTask();
            break;
        case STATE_FREE_PLAY:
            freePlayTask();
            break;
        case STATE_SONG_SELECT:
            songSelectTask();
            break;
        case STATE_SONG_PLAY:
            songPlayTask();
            break;
        default:
            // Fallback to menu if state is invalid
            currentState = STATE_MENU;
            break;
    }
}

// ───────────── State: MENU ─────────────

/*
 * drawMenu(byte selection)
 * Render the two-option menu on the LCD, highlighting the current selection.
 */
static void drawMenu(byte selection)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(selection ? " Free-Play" : ">Free-Play");
    lcd.setCursor(0, 1);
    lcd.print(selection ? ">Song Mode" : " Song Mode");
}

/*
 * menuTask()
 * Handle user input in the MENU state: NEXT toggles selection,
 * OK (short press) confirms and changes state, long-press does nothing.
 */
static void menuTask()
{
    byte sel = 0;
    drawMenu(sel);

    while (true) {
        // Handle NEXT button press (toggle between options)
        if (digitalRead(PIN_BUTTON_NEXT) == LOW) {
            sel ^= 1;
            drawMenu(sel);
            delay(DEBOUNCE_DELAY);
        }

        // Handle OK button press (short press to confirm)
        if (digitalRead(PIN_BUTTON_OK) == LOW) {
            unsigned long pressStart = millis();

            // Wait until OK is released or long-press threshold reached
            while (digitalRead(PIN_BUTTON_OK) == LOW) {
                if (millis() - pressStart > LONG_PRESS_TIME) {
                    // If held for a long time, simply redraw menu
                    drawMenu(sel);
                    while (digitalRead(PIN_BUTTON_OK) == LOW) { /* wait */ }
                    delay(DEBOUNCE_DELAY);
                    pressStart = millis();
                }
            }

            // If it was a short press, switch to the selected state
            if (millis() - pressStart < SHORT_PRESS_TIME) {
                delay(DEBOUNCE_DELAY);
                currentState = (sel ? STATE_SONG_SELECT : STATE_FREE_PLAY);
                return;
            }
        }
    }
}

// ───────────── State: FREE-PLAY ─────────────

/*
 * freePlayTask()
 * In Free-Play, any pressed key lights its LED and generates the
 * corresponding tone. OK button returns to the menu (short or long press).
 */
static void freePlayTask()
{
    // Show mode indicator
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Free-Play (OK)");

    while (true) {
        // If OK is pressed, handle short vs long to return to menu
        if (digitalRead(PIN_BUTTON_OK) == LOW) {
            unsigned long t0 = millis();
            while (digitalRead(PIN_BUTTON_OK) == LOW) {
                if (millis() - t0 > LONG_PRESS_TIME) {
                    // On long press, back to menu immediately
                    sendMask(0);
                    noTone(PIN_BUZZER);
                    currentState = STATE_MENU;
                    return;
                }
            }
            if (millis() - t0 < SHORT_PRESS_TIME) {
                // On short press, also return to menu
                delay(DEBOUNCE_DELAY);
                sendMask(0);
                noTone(PIN_BUZZER);
                currentState = STATE_MENU;
                return;
            }
        }

        // Read current key mask and update LEDs if changed
        byte mask = readKeysMask();
        if (mask != lastOutputMask) {
            sendMask(mask);
            lastOutputMask = mask;
        }

        // Compute average frequency if multiple keys are pressed
        float sumMidi = 0;
        int count = 0;
        for (byte i = 0; i < NUM_KEYS; i++) {
            if (mask & (1 << i)) {
                sumMidi += freqToMidi(KEY_FREQUENCIES[i]);
                count++;
            }
        }
        int frequency = (count ? int(midiToFreq(sumMidi / count) + 0.5f) : 0);

        // Only change the buzzer output when frequency actually changes
        if (frequency != lastFrequencyHz) {
            if (frequency) {
                tone(PIN_BUZZER, frequency);
            } else {
                noTone(PIN_BUZZER);
            }
            lastFrequencyHz = frequency;
        }

        // Small delay to reduce CPU load and for debounce
        delay(5);
    }
}

// ───────────── State: SONG-SELECT ─────────────

/*
 * drawSongSelect(byte selection)
 * Show the prompt and current song filename on the LCD.
 */
static void drawSongSelect(byte selection)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Select song:");
    lcd.setCursor(0, 1);
    lcd.print(songNames[selection]);
}

/*
 * songSelectTask()
 * Populate songNames[] from SD, allow NEXT to cycle, OK to confirm,
 * and KEY_PINS[0] (first key) to go back to menu.
 */
static void songSelectTask()
{
    // Scan root directory for up to MAX_SONGS .TXT files
    songCount = 0;
    File root = SD.open("/");
    while (File f = root.openNextFile()) {
        if (!f.isDirectory() && strstr(f.name(), ".TXT") && songCount < MAX_SONGS) {
            strncpy(songNames[songCount], f.name(), 12);
            songNames[songCount][12] = '\0';
            songCount++;
        }
        f.close();
    }
    root.close();

    // If no songs found, show message and return to menu
    if (songCount == 0) {
        lcd.clear();
        lcd.print("No .txt songs!");
        delay(1500);
        currentState = STATE_MENU;
        return;
    }

    // Display first song
    byte sel = 0;
    drawSongSelect(sel);

    // Navigate list until OK or BACK
    while (true) {
        // BACK on first key press
        if (digitalRead(KEY_PINS[0]) == LOW) {
            delay(DEBOUNCE_DELAY);
            currentState = STATE_MENU;
            return;
        }

        // NEXT advances selection
        if (digitalRead(PIN_BUTTON_NEXT) == LOW) {
            sel = (sel + 1) % songCount;
            drawSongSelect(sel);
            delay(DEBOUNCE_DELAY);
        }

        // OK confirms selection
        if (digitalRead(PIN_BUTTON_OK) == LOW) {
            selectedSong = sel;
            delay(DEBOUNCE_DELAY);
            currentState = STATE_SONG_PLAY;
            return;
        }
    }
}

// ───────────── State: SONG-PLAY ─────────────

/*
 * songPlayTask()
 * Read the selected .TXT file line by line. Each line is
 * “mask duration”. LEDs fade out, and user must press correct keys
 * in time or an ERROR is shown.
 */
static void songPlayTask()
{
    // Open the selected song file
    File sf = SD.open(songNames[selectedSong]);
    if (!sf) {
        // If it fails, notify and return to selection
        lcd.clear();
        lcd.print("Open fail");
        delay(1500);
        currentState = STATE_SONG_SELECT;
        return;
    }

    // Show “Play by keys…” on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(songNames[selectedSong]);
    lcd.setCursor(0, 1);
    lcd.print("Play by keys...");
    delay(500);

    // Process each line of the song file
    while (sf.available()) {
        String line = sf.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.charAt(0) == '#') {
            // Skip empty lines or comments
            continue;
        }

        // Split at space: notes mask vs duration
        int sp = line.indexOf(' ');
        if (sp < 1) {
            continue;
        }
        String notesPart = line.substring(0, sp);
        int durationMs   = line.substring(sp + 1).toInt();

        // Build bitmask of required keys
        byte requiredMask = 0;
        int pos = 0;
        while (true) {
            int comma = notesPart.indexOf(',', pos);
            String tok = notesPart.substring(pos, (comma < 0 ? notesPart.length() : comma));
            int idx = tok.toInt();
            if (idx >= 0 && idx < NUM_KEYS) {
                requiredMask |= 1 << idx;
            }
            if (comma < 0) {
                break;
            }
            pos = comma + 1;
        }
        if (requiredMask == 0) {
            // If no keys, just wait the duration
            delay(durationMs);
            continue;
        }

        // Light up the LEDs immediately
        sendMask(requiredMask);

        // Perform visible fade-out in 8 steps
        for (byte step = 0; step < NUM_KEYS; step++) {
            for (byte k = 0; k < 10; k++) {
                sendMask(requiredMask);
                delay((NUM_KEYS - step) * 2);
                sendMask(0);
                delay(step * 2);
            }
        }

        // Now allow user up to twice the duration to press keys
        unsigned long startTime = millis();
        unsigned long window    = 2UL * durationMs;
        while (millis() - startTime < window) {
            byte pressed = readKeysMask() & requiredMask;
            if (pressed) {
                // Compute and play averaged tone for pressed keys
                float sumM = 0;
                int cnt    = 0;
                for (byte i = 0; i < NUM_KEYS; i++) {
                    if (pressed & (1 << i)) {
                        sumM += freqToMidi(KEY_FREQUENCIES[i]);
                        cnt++;
                    }
                }
                int f = int(midiToFreq(sumM / cnt) + 0.5f);
                tone(PIN_BUZZER, f);
            } else {
                // No keys pressed → mute buzzer
                noTone(PIN_BUZZER);
            }
        }

        // Turn off LEDs and buzzer
        sendMask(0);
        noTone(PIN_BUZZER);

        // If not all required keys were pressed, show ERROR
        if ((readKeysMask() & requiredMask) != requiredMask) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  ERROR!");
            tone(PIN_BUZZER, 200, 500);
            delay(700);
            sf.close();
            currentState = STATE_SONG_SELECT;
            return;
        }
    }

    // Close file and indicate completion
    sf.close();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Finished!");
    tone(PIN_BUZZER, 523, 500);
    delay(800);
    currentState = STATE_MENU;
}

[![Pian electric](https://img.youtube.com/vi/p7ay5NSMkBY/0.jpg)](https://www.youtube.com/watch?v=p7ay5NSMkBY "Pian electric")

# Portable Electric Piano

## Overview
A compact, Arduino-based electric piano that supports two modes:
- **Free-Play**: press any of the 8 keys to play a tone and light its LED  
- **Song-Mode**: load simple “mask duration” .TXT files from a FAT32 microSD card and follow along; LEDs fade out and you must press the correct keys in time

Designed for easy hardware prototyping (I2C LCD, SPI shift-register, SD module, piezo buzzer, tactile buttons, LEDs) and fully documented with schematics, wiring, and academic-style code comments.

---

## Features
- **8-note keyboard** on digital pins, with averaged-pitch chord support  
- **74HC595 shift register** drives 8 LEDs via only 3 Arduino pins  
- **16×2 I2C LCD** displays mode, song names, error messages  
- **microSD playback** of `.txt` files in format `mask duration`  
- **Free-Play mode** with instant tone + LED feedback (<5 ms latency)  
- **Song-Mode** with fade-out LED effect, real-time press validation, error recovery  
- **Two-button UI** (`NEXT` / `OK`) for menu navigation and selection  
- **Long/short-press** on `OK` to return to menu from any mode  
- **Modular C++ firmware** (FSM states, inline frequency conversions, well-commented)

---

## Hardware Requirements
- Arduino UNO (ATmega328P)  
- 16×2 LCD with I2C backpack  
- 74HC595 shift register (DIP-16)  
- microSD card adapter (SPI, 5 V tolerant) + FAT32 microSD card  
- Piezo buzzer (passive, 5 V)  
- 8× tactile keys (6×6 mm) for notes  
- 2× tactile keys (6×6 mm) for `NEXT` & `OK`  
- 8× 5 mm LEDs + 220 Ω resistors  
- Breadboard & jumper wires  
- 5 V supply (USB or power-bank)

Refer to `docs/schematic.pdf` for the full wiring diagram.

---

## Software Requirements
- **Arduino IDE** (1.8.x or later)  
- **Libraries**:  
  - `Wire.h` for I2C  
  - `SPI.h` for SPI  
  - `LiquidCrystal_I2C.h` for the LCD  
  - `SD.h` for microSD access  
- **FAT32-formatted** microSD card containing song files (`/01.txt`, `/02.txt`, …)

---

## Quick Start

1. **Hardware hookup**  
   - Wire LCD to A4 (SDA) & A5 (SCL), SD module to D11–D13+CS (D4), 74HC595 to D8/D10/A2/A3, buzzer to D9, keys & LEDs as per `docs/schematic.pdf`.

2. **Prepare microSD**  
   - Format as FAT32, copy your `.txt` songs into the root.

3. **Upload firmware**  
   - Open `Pian-electric.ino` in Arduino IDE  
   - Select “Arduino UNO” & your COM port  
   - Click **Upload**

4. **Operate**  
   - On power-up, the LCD shows “Init Piano… Checking SD…”  
   - Use **NEXT** to toggle Free-Play vs Song-Mode, **OK** to select  
   - In Song-Mode, use **NEXT** to cycle through songs, **OK** to play  
   - In either play mode, **OK** short-press returns to menu; long-press also works  
   - In Song-Mode, follow LEDs and press the correct keys in time

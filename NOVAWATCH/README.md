# NOVAWATCH — HT16K33 (REAL HARDWARE)

Version NOVAWATCH adapted from the original MAX7219 design to the HT16K33A I2C LED driver.

This sketch is intended for the real hardware below, not a simulator.

## 1. Hardware
- Arduino Nano
- Adafruit-style HT16K33 16x8 breakout (default I2C address 0x70)
- 4-digit 7-segment display, COMMON CATHODE
- DS3231 RTC
- 7 × 74HC595
- 7 × ULN2803A
- Buzzer
- POWER / MODE / PLUS / MINUS buttons
- External colon LED(s) on D7
- 12 V input with a proper 5 V regulator/buck converter

## 2. Arduino Nano → HT16K33

| Arduino Nano | HT16K33 breakout |
|---|---|
| 5V | VDD |
| GND | GND |
| A4 | SDA |
| A5 | SCL |

The HT16K33 default address is 0x70.

### DS3231 on the same I2C bus

| Arduino Nano | DS3231 |
|---|---|
| A4 | SDA |
| A5 | SCL |
| 5V | VCC |
| GND | GND |

The DS3231 uses 0x68, so it can share the same SDA/SCL lines with the HT16K33 at 0x70.

## 3. HT16K33 → 4-digit common-cathode display

The module outputs are labelled A0...A15 and C0...C7. For this firmware, connect:

| HT16K33 breakout | Display function |
|---|---|
| A0 | Segment A |
| A1 | Segment B |
| A2 | Segment C |
| A3 | Segment D |
| A4 | Segment E |
| A5 | Segment F |
| A6 | Segment G |
| A7 | Decimal point (DP), optional |
| C0 | Digit 1 common cathode |
| C1 | Digit 2 common cathode |
| C2 | Digit 3 common cathode |
| C3 | Digit 4 common cathode |
| A8-A15 | Unused |
| C4-C7 | Unused |

Important: A0/A1/etc. above are the labels printed on the HT16K33 breakout, NOT Arduino analog pins.

The four common-cathode connections must match the four digit commons of the actual display.

### Physical pin numbers of the 4-digit display

Do not guess the physical pin numbers from a generic 12-pin drawing. Different 4-digit common-cathode displays can use different pinouts.

Identify the exact display part number/datasheet, or map its pins with a multimeter/LED test, then connect those physical pins to the logical functions in the table above.

## 4. Buttons / buzzer / colon

| Arduino Nano | Function |
|---|---|
| D2 | POWER button |
| D3 | MODE button |
| D4 | PLUS button |
| D5 | MINUS button |
| D6 | Buzzer |
| D7 | Colon LED(s) |
| D8 | 74HC595 DATA |
| D9 | 74HC595 LATCH |
| D13 | 74HC595 CLOCK |

Buttons use INPUT_PULLUP, therefore each button is wired between its Arduino pin and GND.

## 5. 74HC595 contour

The original NOVAWATCH contour system remains unchanged:
- 7 × 74HC595
- 7 × ULN2803A
- D8 = DATA
- D9 = LATCH
- D13 = CLOCK

The MAX7219 is removed completely.

## 6. HT16K33 RAM mapping used by the firmware

The HT16K33A official mapping is:

| COM | ROW0-ROW7 RAM address |
|---|---:|
| COM0 | 0x00 |
| COM1 | 0x02 |
| COM2 | 0x04 |
| COM3 | 0x06 |

The firmware writes the segment pattern into these locations and leaves ROW8-ROW15 unused.

## 7. NOVAWATCH controls

### Normal mode
- POWER: ON/OFF
- MODE ×1: reset clock to 00:00
- MODE ×2: enter time setting

### Time setting
- MODE ×1: switch HOURS ↔ MINUTES
- PLUS: increase selected value
- MINUS: decrease selected value
- MODE ×3: save the new time

The MODE multi-click window is 1 second.

## 8. Startup

When POWER is pressed:
1. HT16K33 is initialized.
2. Seven-segment startup animation runs.
3. Buzzer plays the startup melody.
4. DS3231 time is read.
5. HH:MM is displayed.
6. The 74HC595 contour animation starts.

## 9. Power

Never connect 12 V directly to the 5 V logic rail.

Use: 12 V DC → regulated 5 V → Arduino Nano + HT16K33 + DS3231 + logic.

All grounds must be common.

## 10. Firmware

Main sketch: NOVAWATCH/NOVAWATCH_HT16K33.ino

The sketch uses only the Arduino Wire library; no external HT16K33 library is required.

## Official HT16K33A reference

https://www.holtek.com/webapi/116711/HT16K33Av110.pdf

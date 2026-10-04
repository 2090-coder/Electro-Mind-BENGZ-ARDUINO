# NOVAWATCH — HT16K33

Version adapted from the original NOVAWATCH MAX7219 design to the HT16K33A I2C LED driver.

## Hardware

- Arduino Nano
- HT16K33 16x8 breakout
- 4-digit 7-segment display, common cathode
- DS3231 RTC
- 7x 74HC595
- 7x ULN2803A
- Buzzer
- 4 buttons
- Colon LED(s)
- 12 V DC input with a suitable 5 V regulator/buck converter

## Arduino Nano → HT16K33

| Nano | HT16K33 |
|---|---|
| 5V | VDD |
| GND | GND |
| A4 | SDA |
| A5 | SCL |

The DS3231 stays on the same I2C bus:

- DS3231 address: 0x68
- HT16K33 default address: 0x70

## HT16K33 → 4-digit common-cathode display

This firmware assumes the following logical mapping:

| HT16K33 | Display |
|---|---|
| A0 / ROW0 | Segment A |
| A1 / ROW1 | Segment B |
| A2 / ROW2 | Segment C |
| A3 / ROW3 | Segment D |
| A4 / ROW4 | Segment E |
| A5 / ROW5 | Segment F |
| A6 / ROW6 | Segment G |
| A7 / ROW7 | Decimal point |
| C0 / COM0 | Digit 1 common cathode |
| C1 / COM1 | Digit 2 common cathode |
| C2 / COM2 | Digit 3 common cathode |
| C3 / COM3 | Digit 4 common cathode |

C4-C7 remain unused.

**Important:** the physical pin numbers of the 4-digit display are NOT assumed here. Use the exact datasheet/pinout of the display before connecting wires.

## Buttons / buzzer / contour

- D2 = POWER
- D3 = MODE
- D4 = PLUS
- D5 = MINUS
- D6 = BUZZER
- D7 = COLON
- D8 = 74HC595 DATA
- D9 = 74HC595 LATCH
- D13 = shared 74HC595 CLOCK

The MAX7219 is removed completely.

## HT16K33 RAM mapping

For the HT16K33 16x8 mode, the official Holtek mapping is:

- COM0: RAM 0x00 / 0x01
- COM1: RAM 0x02 / 0x03
- COM2: RAM 0x04 / 0x05
- COM3: RAM 0x06 / 0x07

The firmware uses the first byte of each pair for ROW0-ROW7.

## Important power note

Do not feed 12 V directly into the 5 V logic rail. Use a proper regulated/buck 5 V supply for the Nano/HT16K33/DS3231/logic, and connect all grounds together.

## Controls

- POWER: ON/OFF
- MODE x1: reset clock to 00:00
- MODE x2: enter time setting
- In setting: MODE x1 switches hours/minutes
- PLUS/MINUS: change selected value
- MODE x3 in setting: save time

## Official reference

Holtek HT16K33A datasheet:
https://www.holtek.com/webapi/116711/HT16K33Av110.pdf

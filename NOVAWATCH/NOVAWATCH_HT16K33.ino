#include <Wire.h>

// ============================================================
// NOVAWATCH
// Arduino Nano + HT16K33 + DS3231 + 7x74HC595 + 7xULN2803A
// 4-digit 7-segment COMMON CATHODE display
// Input supply: 12 V DC (use an appropriate 5 V regulator/buck)
// ============================================================

// -------------------- BUTTONS / BUZZER -----------------------
const byte PIN_BUTTON_POWER = 2;
const byte PIN_BUTTON_MODE  = 3;
const byte PIN_BUTTON_PLUS  = 4;
const byte PIN_BUTTON_MINUS = 5;
const byte PIN_BUZZER       = 6;
const byte PIN_COLON        = 7;

// -------------------- 74HC595 CONTOUR ------------------------
const byte PIN_595_DATA  = 8;
const byte PIN_595_LATCH = 9;
const byte PIN_CLOCK     = 13;

// -------------------- I2C DEVICES ----------------------------
const byte RTC_ADDRESS = 0x68;
const byte HT16K33_ADDRESS = 0x70;

// HT16K33 commands
const byte HT_CMD_SYSTEM_ON    = 0x21;
const byte HT_CMD_DISPLAY_ON   = 0x81;
const byte HT_CMD_BRIGHTNESS   = 0xE0;

// -------------------- 7-SEGMENT MAP --------------------------
// These bits correspond to HT16K33 ROW0..ROW7.
// Wire the display exactly as:
// ROW0=A, ROW1=B, ROW2=C, ROW3=D,
// ROW4=E, ROW5=F, ROW6=G, ROW7=DP.
const byte SEG_A  = 0x01;
const byte SEG_B  = 0x02;
const byte SEG_C  = 0x04;
const byte SEG_D  = 0x08;
const byte SEG_E  = 0x10;
const byte SEG_F  = 0x20;
const byte SEG_G  = 0x40;
const byte SEG_DP = 0x80;

const byte DIGIT_MASK[10] = {
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,                    // 0
  SEG_B | SEG_C,                                                    // 1
  SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                            // 2
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                            // 3
  SEG_B | SEG_C | SEG_F | SEG_G,                                    // 4
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                            // 5
  SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,                    // 6
  SEG_A | SEG_B | SEG_C,                                            // 7
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,            // 8
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G                     // 9
};

// HT16K33 display RAM mapping:
// COM0 -> RAM 0x00/0x01
// COM1 -> RAM 0x02/0x03
// COM2 -> RAM 0x04/0x05
// COM3 -> RAM 0x06/0x07
// For a 4-digit display, only the first byte of each pair is needed.

void htCommand(byte command) {
  Wire.beginTransmission(HT16K33_ADDRESS);
  Wire.write(command);
  Wire.endTransmission();
}

void htClear() {
  Wire.beginTransmission(HT16K33_ADDRESS);
  Wire.write((byte)0x00); // display RAM pointer
  for (byte i = 0; i < 16; i++) {
    Wire.write((byte)0x00);
  }
  Wire.endTransmission();
}

void htInit() {
  htCommand(HT_CMD_SYSTEM_ON);  // internal oscillator ON
  htClear();

  // Display ON, blink OFF
  htCommand(HT_CMD_DISPLAY_ON);

  // Brightness 0..15. 8 is a safe starting value.
  htCommand(HT_CMD_BRIGHTNESS | 0x08);
}

void htWriteDigit(byte digitIndex, byte segments) {
  if (digitIndex > 3) return;

  // COM0, COM1, COM2, COM3 are at RAM addresses 0x00, 0x02, 0x04, 0x06.
  byte ramAddress = digitIndex * 2;

  Wire.beginTransmission(HT16K33_ADDRESS);
  Wire.write(ramAddress);
  Wire.write(segments);
  Wire.write((byte)0x00); // second byte = ROW8..ROW15, unused here
  Wire.endTransmission();
}

void htDisplayHHMM(byte h, byte m) {
  htWriteDigit(0, DIGIT_MASK[h / 10]);
  htWriteDigit(1, DIGIT_MASK[h % 10]);
  htWriteDigit(2, DIGIT_MASK[m / 10]);
  htWriteDigit(3, DIGIT_MASK[m % 10]);
}

void htDisplayEdit() {
  byte hTens = editHour / 10;
  byte hUnits = editHour % 10;
  byte mTens = editMinute / 10;
  byte mUnits = editMinute % 10;

  if (editField == EDIT_HOUR && !editVisible) {
    htWriteDigit(0, 0);
    htWriteDigit(1, 0);
  } else {
    htWriteDigit(0, DIGIT_MASK[hTens]);
    htWriteDigit(1, DIGIT_MASK[hUnits]);
  }

  if (editField == EDIT_MINUTE && !editVisible) {
    htWriteDigit(2, 0);
    htWriteDigit(3, 0);
  } else {
    htWriteDigit(2, DIGIT_MASK[mTens]);
    htWriteDigit(3, DIGIT_MASK[mUnits]);
  }
}

void setColon(bool on) {
  digitalWrite(PIN_COLON, on ? HIGH : LOW);
}

// -------------------- DS3231 --------------------------------
byte bcdToDec(byte value) {
  return ((value >> 4) * 10) + (value & 0x0F);
}

byte decToBcd(byte value) {
  return ((value / 10) << 4) | (value % 10);
}

byte hourNow = 0;
byte minuteNow = 0;
byte secondNow = 0;
unsigned long lastRTCRead = 0;
const unsigned long RTC_INTERVAL = 500;

bool rtcReadTime() {
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write((byte)0x00);
  if (Wire.endTransmission() != 0) return false;

  if (Wire.requestFrom(RTC_ADDRESS, (byte)3) != 3) return false;

  secondNow = bcdToDec(Wire.read() & 0x7F);
  minuteNow = bcdToDec(Wire.read() & 0x7F);
  hourNow   = bcdToDec(Wire.read() & 0x3F);

  if (hourNow > 23 || minuteNow > 59 || secondNow > 59) return false;
  return true;
}

bool rtcWriteTime(byte h, byte m, byte s) {
  if (h > 23 || m > 59 || s > 59) return false;

  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write((byte)0x00);
  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  return Wire.endTransmission() == 0;
}

// -------------------- CONTOUR: 7x74HC595 ---------------------
const byte SHIFT_REG_COUNT = 7;
const byte GROUPS_PER_COLOR = 17;
const byte CONTOUR_GROUPS = 51;
byte contourData[SHIFT_REG_COUNT];

const unsigned long CONTOUR_INTERVAL = 100;
unsigned long lastContourUpdate = 0;
byte contourPosition = 0;
byte contourColor = 0;

void clearContour() {
  for (byte i = 0; i < SHIFT_REG_COUNT; i++) {
    contourData[i] = 0;
  }
}

void setContourGroup(byte group, bool on) {
  if (group >= CONTOUR_GROUPS) return;

  byte chip = group / 8;
  byte bit = group % 8;

  if (on) {
    contourData[chip] |= (byte)(1 << bit);
  } else {
    contourData[chip] &= (byte)~(1 << bit);
  }
}

void writeContour() {
  digitalWrite(PIN_595_LATCH, LOW);

  for (int chip = SHIFT_REG_COUNT - 1; chip >= 0; chip--) {
    shiftOut(PIN_595_DATA, PIN_CLOCK, LSBFIRST, contourData[chip]);
  }

  digitalWrite(PIN_595_LATCH, HIGH);
}

void contourInit() {
  pinMode(PIN_595_DATA, OUTPUT);
  pinMode(PIN_595_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);

  clearContour();
  writeContour();
}

void updateContour() {
  if (!watchOn) return;
  if (millis() - lastContourUpdate < CONTOUR_INTERVAL) return;

  lastContourUpdate = millis();

  clearContour();

  byte group = contourPosition + (contourColor * GROUPS_PER_COLOR);
  setContourGroup(group, true);
  writeContour();

  contourColor++;
  if (contourColor >= 3) {
    contourColor = 0;
    contourPosition++;
    if (contourPosition >= GROUPS_PER_COLOR) contourPosition = 0;
  }
}

// -------------------- BUZZER / STARTUP -----------------------
void beepAction() {
  tone(PIN_BUZZER, 880, 60);
}

void beepDigit(byte digit) {
  const unsigned int frequencies[10] = {
    262, 294, 330, 349, 392, 440, 494, 523, 587, 659
  };
  tone(PIN_BUZZER, frequencies[digit % 10], 75);
}

struct Note {
  unsigned int frequency;
  unsigned int duration;
};

const Note STARTUP_MELODY[] = {
  {523, 120}, {659, 120}, {784, 120}, {1047, 220},
  {784, 120}, {659, 120}, {523, 260}
};

const byte STARTUP_MELODY_COUNT =
  sizeof(STARTUP_MELODY) / sizeof(STARTUP_MELODY[0]);

byte melodyIndex = 0;
unsigned long melodyNext = 0;

void startMelody() {
  melodyIndex = 0;
  melodyNext = 0;
}

void updateMelody() {
  if (!startupActive) return;

  unsigned long now = millis();
  if (now < melodyNext) return;

  if (melodyIndex >= STARTUP_MELODY_COUNT) {
    noTone(PIN_BUZZER);
    melodyNext = now + 100000UL;
    return;
  }

  tone(
    PIN_BUZZER,
    STARTUP_MELODY[melodyIndex].frequency,
    STARTUP_MELODY[melodyIndex].duration - 10
  );

  melodyNext = now + STARTUP_MELODY[melodyIndex].duration;
  melodyIndex++;
}

void displaySplash(byte frame) {
  byte mask = 0;

  switch (frame % 8) {
    case 0: mask = SEG_A; break;
    case 1: mask = SEG_B; break;
    case 2: mask = SEG_C; break;
    case 3: mask = SEG_D; break;
    case 4: mask = SEG_E; break;
    case 5: mask = SEG_F; break;
    case 6: mask = SEG_G; break;
    case 7: mask = SEG_A | SEG_B | SEG_C | SEG_D |
                      SEG_E | SEG_F | SEG_G; break;
  }

  htWriteDigit(0, mask);
  htWriteDigit(1, mask);
  htWriteDigit(2, mask);
  htWriteDigit(3, mask);

  setColon((frame % 2) == 0);
}

// -------------------- WATCH STATE ----------------------------
bool watchOn = false;
bool startupActive = false;
unsigned long startupStart = 0;
unsigned long lastStartupFrame = 0;
byte startupFrame = 0;

const unsigned long STARTUP_DURATION = 2300;
const unsigned long STARTUP_FRAME_INTERVAL = 120;

enum EditField { EDIT_HOUR, EDIT_MINUTE };
bool editMode = false;
EditField editField = EDIT_HOUR;
byte editHour = 0;
byte editMinute = 0;
bool editVisible = true;
unsigned long lastBlink = 0;
const unsigned long BLINK_INTERVAL = 350;

// -------------------- BUTTON DEBOUNCE ------------------------
struct Button {
  byte pin;
  bool raw;
  bool stable;
  unsigned long changedAt;
};

Button buttonPower = {PIN_BUTTON_POWER, HIGH, HIGH, 0};
Button buttonMode  = {PIN_BUTTON_MODE, HIGH, HIGH, 0};
Button buttonPlus  = {PIN_BUTTON_PLUS, HIGH, HIGH, 0};
Button buttonMinus = {PIN_BUTTON_MINUS, HIGH, HIGH, 0};

const unsigned long DEBOUNCE_MS = 35;
const unsigned long MODE_WINDOW_MS = 1000;
byte modeClicks = 0;
unsigned long modeDeadline = 0;

bool pressed(Button &button) {
  bool reading = digitalRead(button.pin);

  if (reading != button.raw) {
    button.raw = reading;
    button.changedAt = millis();
  }

  if (millis() - button.changedAt >= DEBOUNCE_MS &&
      reading != button.stable) {
    button.stable = reading;
    if (button.stable == LOW) return true;
  }

  return false;
}

// -------------------- WATCH CONTROL --------------------------
void startWatch() {
  watchOn = true;
  editMode = false;
  startupActive = true;
  startupStart = millis();
  lastStartupFrame = 0;
  startupFrame = 0;
  contourPosition = 0;
  contourColor = 0;
  startMelody();

  htInit();
  htClear();

  clearContour();
  writeContour();
}

void stopWatch() {
  watchOn = false;
  startupActive = false;
  editMode = false;
  modeClicks = 0;
  noTone(PIN_BUZZER);

  htClear();
  setColon(false);

  clearContour();
  writeContour();

  // Display OFF command, oscillator can remain enabled.
  htCommand(0x80);
}

void updateStartup() {
  if (!startupActive) return;

  unsigned long now = millis();

  if (now - lastStartupFrame >= STARTUP_FRAME_INTERVAL) {
    lastStartupFrame = now;
    displaySplash(startupFrame);
    startupFrame++;
  }

  if (now - startupStart >= STARTUP_DURATION) {
    startupActive = false;
    noTone(PIN_BUZZER);

    if (!rtcReadTime()) {
      hourNow = 0;
      minuteNow = 0;
      secondNow = 0;
    }

    htDisplayHHMM(hourNow, minuteNow);
    setColon(true);
  }
}

void resetClock() {
  if (rtcWriteTime(0, 0, 0)) {
    hourNow = 0;
    minuteNow = 0;
    secondNow = 0;
  }

  beepAction();
  htDisplayHHMM(hourNow, minuteNow);
}

void enterEditMode() {
  if (!rtcReadTime()) return;

  editHour = hourNow;
  editMinute = minuteNow;
  editField = EDIT_HOUR;
  editVisible = true;
  editMode = true;
  lastBlink = millis();

  htDisplayEdit();
  beepAction();
}

void validateEditMode() {
  if (rtcWriteTime(editHour, editMinute, 0)) {
    hourNow = editHour;
    minuteNow = editMinute;
    secondNow = 0;
  }

  editMode = false;
  editVisible = true;

  htDisplayHHMM(hourNow, minuteNow);
  setColon(true);
  beepAction();
}

void processModeClicks() {
  if (modeClicks == 0) return;
  if (millis() < modeDeadline) return;

  if (!editMode) {
    if (modeClicks == 1) {
      resetClock();
    } else if (modeClicks == 2) {
      enterEditMode();
    }
  } else {
    if (modeClicks == 1) {
      editField =
        (editField == EDIT_HOUR) ? EDIT_MINUTE : EDIT_HOUR;

      editVisible = true;
      lastBlink = millis();
      htDisplayEdit();
      beepAction();

    } else if (modeClicks == 3) {
      validateEditMode();
    }
  }

  modeClicks = 0;
}

void handleButtons() {
  if (pressed(buttonPower)) {
    if (watchOn) stopWatch();
    else startWatch();
  }

  if (!watchOn || startupActive) return;

  if (pressed(buttonMode)) {
    modeClicks++;
    if (modeClicks > 3) modeClicks = 3;
    modeDeadline = millis() + MODE_WINDOW_MS;
  }

  if (editMode) {
    if (pressed(buttonPlus)) {
      if (editField == EDIT_HOUR) {
        editHour = (editHour + 1) % 24;
        beepDigit(editHour % 10);
      } else {
        editMinute = (editMinute + 1) % 60;
        beepDigit(editMinute % 10);
      }

      editVisible = true;
      lastBlink = millis();
      htDisplayEdit();
    }

    if (pressed(buttonMinus)) {
      if (editField == EDIT_HOUR) {
        editHour = (editHour == 0) ? 23 : editHour - 1;
        beepDigit(editHour % 10);
      } else {
        editMinute = (editMinute == 0) ? 59 : editMinute - 1;
        beepDigit(editMinute % 10);
      }

      editVisible = true;
      lastBlink = millis();
      htDisplayEdit();
    }
  }

  processModeClicks();
}

void updateEditBlink() {
  if (!editMode) return;

  if (millis() - lastBlink >= BLINK_INTERVAL) {
    lastBlink = millis();
    editVisible = !editVisible;
    htDisplayEdit();
  }
}

// -------------------- SETUP / LOOP ---------------------------
void setup() {
  pinMode(PIN_BUTTON_POWER, INPUT_PULLUP);
  pinMode(PIN_BUTTON_MODE, INPUT_PULLUP);
  pinMode(PIN_BUTTON_PLUS, INPUT_PULLUP);
  pinMode(PIN_BUTTON_MINUS, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_COLON, OUTPUT);

  digitalWrite(PIN_COLON, LOW);

  Wire.begin();
  Wire.setClock(100000UL);

  // Initialize the HT16K33 once so the display starts in a known state.
  htInit();

  contourInit();

  rtcReadTime();

  watchOn = false;
  startupActive = false;
  editMode = false;

  htClear();
  htCommand(0x80); // display OFF
}

void loop() {
  handleButtons();

  if (!watchOn) return;

  updateMelody();
  updateStartup();
  updateContour();

  if (startupActive) return;

  if (!editMode && millis() - lastRTCRead >= RTC_INTERVAL) {
    lastRTCRead = millis();

    if (rtcReadTime()) {
      htDisplayHHMM(hourNow, minuteNow);
    }
  }

  updateEditBlink();
}

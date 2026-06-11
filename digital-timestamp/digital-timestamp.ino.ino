#include <SPI.h>
#include <SD.h> // for memory card
#include <Wire.h> // Required for I2C communication
#include "RTClib.h" // Include the DS3231 library
#include <Keypad.h> // for keypad
#include <LiquidCrystal_I2C.h> // for the I2C LCD

LiquidCrystal_I2C lcd(0x27, 16, 2); // OR "0x3F" if lcd is blank
RTC_DS3231 rtc;
const int chipSelect = 10;

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS]  = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

enum stampstate {
  HOME_STATE,
  INPUT_STATE,
  PROCESSING_STATE,
  RESULT_STATE
};
stampstate defaultState = HOME_STATE;

typedef struct {
  char  inputBuf[5];           // 4 digits + null terminator
  byte  inputLen;
} forinput;
forinput session = {"", 0};

bool  sdCardReady  = false; //to check if the SDcard is available in the module to avoid it crashing
unsigned long lastClockUpdate = 0;

// ── RAM-safe LCD helper ───────────────────────────────────
// Prints two PROGMEM strings (F() strings) on the LCD
void lcdShow(const __FlashStringHelper *r0, const __FlashStringHelper *r1) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(r0);
  lcd.setCursor(0, 1); lcd.print(r1);
}

// Prints one PROGMEM string on row 0, one RAM string on row 1
void lcdShow(const __FlashStringHelper *r0, const char *r1) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(r0);
  lcd.setCursor(0, 1); lcd.print(r1);
}

// paddeding with zero and slash for time & date
void printPadded(LiquidCrystal_I2C &lcd, byte value, char separator) {
  if (value < 10) {
    lcd.print('0');
  }
  lcd.print(value);
  lcd.print(separator);
}

// ── Home screen ───────────────────────────────
void showHomeClock(bool forceRedraw) {
  DateTime now = rtc.now();

  // Row 0: date  — only redraw when forced (no flicker)
  if (forceRedraw) {
    lcd.setCursor(0, 0);
    lcd.print(F("Date: "));
    printPadded(lcd, now.day(),    '/');
    printPadded(lcd, now.month(),  '/');
    lcd.print(now.year());
  }

  // Row 1: time — updates every second
  lcd.setCursor(0, 1);
  lcd.print(F("Time: "));
  printPadded(lcd, now.hour(),   ':');
  printPadded(lcd, now.minute(), ':');
  printPadded(lcd, now.second(), ' ');
}

// for resetting the input and home mode
void resetSession() {
  session.inputLen    = 0;
  session.inputBuf[0] = '\0';
}

// ── Enter input mode ──────────────────────────────────────
void enterinputMode() {
  resetSession();
  defaultState = INPUT_STATE;
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("Matric(last 4):"));
  lcd.setCursor(0, 1); lcd.print(F("_   "));
}

// ── Return to home screen ─────────────────────────────────
void exitToHome() {
  resetSession();
  defaultState = HOME_STATE;
  showHomeClock(true);   // full redraw
  lastClockUpdate = millis();
}

// ── Read one CSV line from an open File into a char buffer ─
// Returns false when no more lines.
bool readLine(File &f, char *buf, byte bufSize) {
  byte i = 0;
  while (f.available()) {
    char c = f.read();
    if (c == '\n') break;
    if (c == '\r') continue;
    if (i < bufSize - 1) buf[i++] = c;
  }
  buf[i] = '\0';
  return (i > 0);
}

// ── Look up last-4 digits in STUDENTS.CSV ─────────────────
// File format per line:  MCE/23/0042,Amina,Yusuf
//   col 0 = full matric   col 1 = first name   col 2 = surname (ignored)
// Matching rule: last 4 chars of stored matric == session.inputBuf
//
// Copies first name into nameOut (max nameOutSize chars).
// Returns true if found.
bool lookupStudent(const char *last4, char *nameOut, byte nameOutSize) {
  nameOut[0] = '\0';
  if (!sdCardReady) return false;

  File f = SD.open(F("STUDENTS.CSV"));
  if (!f) {
    lcdShow(F("SD Read Error!"), F("STUDENTS.CSV"));
    delay(2000);
    return false;
  }

  char line[40];   // enough for  MCE/24/0099,Firstname,Surname
  bool found = false;

  while (f.available()) {
    if (!readLine(f, line, sizeof(line))) continue;

    // Find first comma → that is end of matric field
    char *comma1 = strchr(line, ',');
    if (!comma1) continue;

    // Compare last 4 chars of the matric field
    byte matricLen = comma1 - line;
    if (matricLen < 4) continue;
    const char *stored4 = line + matricLen - 4;  // pointer to last 4

    // strncmp: compare stored4 with last4 (4 chars)
    if (strncmp(stored4, last4, 4) != 0) continue;

    // Match — extract first name (between comma1 and comma2)
    char *comma2 = strchr(comma1 + 1, ',');
    byte nameLen;
    if (comma2) {
      nameLen = comma2 - (comma1 + 1);
    } else {
      nameLen = strlen(comma1 + 1);
    }
    nameLen = min(nameLen, (byte)(nameOutSize - 1));
    strncpy(nameOut, comma1 + 1, nameLen);
    nameOut[nameLen] = '\0';
    found = true;
    break;
  }

  f.close();
  return found;
}

// paddeding with zero and slash for time & date
void printPadded (File & f, byte value, char separator) {
  if (value < 10)
  { f.print('0');
  }
  f.print(value);
  f.print(separator);
}

// ── Log submission to LOGS.CSV ────────────────────────────
// Line written:  FULL_MATRIC,FirstName,DD/MM/YYYY,HH:MM:SS,SUBMITTED
void logSubmission(const char *last4, const char *firstName) {
  if (!sdCardReady) return;

  DateTime now = rtc.now();

  File f = SD.open(F("LOGS.CSV"), FILE_WRITE);
  if (!f) {
    lcdShow(F("SD Write Error!"), F("Not saved!"));
    delay(2000);
    return;
  }

  // Write header only if file is brand-new (size == 0)
  if (f.size() == 0) {
    f.println(F("Matric,Name,Date,Time,Status"));
  }

  // Reconstruct a readable matric label e.g. "xxxx"
  f.print(F("MCE/??/"));  f.print(last4);   f.print(',');
  f.print(firstName);                         f.print(',');

  // Date
  printPadded(f, now.day(),    '/');
  printPadded(f, now.month(),  '/');
  f.print(now.year()); f.print(',');

  // Time
  printPadded(f, now.hour(),   ':');
  printPadded(f, now.minute(), ':');
  printPadded(f, now.second(), ',');

  f.println(F("SUBMITTED"));
  f.close();
}

// ── Process 4-digit input ─────────────────────────────────
void processInput() {
  lcdShow(F("Checking..."), session.inputBuf);

  char firstName[16];
  bool found = lookupStudent(session.inputBuf, firstName, sizeof(firstName));

  if (!found) {
    lcdShow(F("NOT REGISTERED"), F("Press any key"));
    Serial.print(F("[DENIED] ")); Serial.println(session.inputBuf);
    delay(3000);
    exitToHome();
    return;
  }

  // Log to SD
  logSubmission(session.inputBuf, firstName);

  // Show result
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(firstName); lcd.print(F(" -SUBMITTED"));
  lcd.setCursor(0, 1); lcd.print(F("Saved to SD!"));

  Serial.print(F("[OK] ")); Serial.print(firstName);
  Serial.print(F(" | ")); Serial.println(session.inputBuf);

  delay(3000);
  exitToHome();
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  delay(200);

  lcd.begin();
  lcd.backlight();
  lcdShow(F("Initializing..."), F("Please wait..."));
  delay(500);

  // RTC
  if (!rtc.begin()) {
    lcdShow(F("RTC ERROR!"), F("Check wiring."));
    while (1);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    lcdShow(F("RTC Adjusted"), F("Time set."));
    delay(1000);
  }

  // SD
  if (!SD.begin(chipSelect)) {
    lcdShow(F("SD Card FAILED"), F("No logging!"));
    delay(2000);
  } else {
    sdCardReady = true;
    if (!SD.exists("STUDENTS.CSV")) {
      lcdShow(F("WARNING:"), F("No STUDENTS.CSV"));
      delay(2500);
    }
  }

  exitToHome();
}

void handleHomeState() {
  if (millis() - lastClockUpdate >= 1000) {
    showHomeClock(false);   // only redraw time row
    lastClockUpdate = millis();
  }

  char key = keypad.getKey();
  if (!key) return;

  // ── HOME SCREEN key handling ─────────────────────────────
  // Any digit or letter wakes up input mode
  enterinputMode();
  // If the key that woke us is already a digit, count it as first input
  if (isDigit(key)) {
    session.inputBuf[session.inputLen++] = key;
    session.inputBuf[session.inputLen]   = '\0';
    lcd.setCursor(0, 1);
    lcd.print(session.inputBuf);
    // Pad remaining slots with underscore
    for (byte i = session.inputLen; i < 4; i++) lcd.print('_');
  }
}

void handleInputState() {
  char key = keypad.getKey();
  if (!key) return;

  if (key == '*') {           // Cancel → back to home
    exitToHome();
    return;
  }

  if (key == '#') {           // Confirm
    if (session.inputLen == 4) {
      processInput();
    } else {
      lcdShow(F("Need 4 digits!"), F("Try again"));
      delay(1500);
      enterinputMode();
    }
    return;
  }

  if (isDigit(key) && session.inputLen < 4) {
    session.inputBuf[session.inputLen++] = key;
    session.inputBuf[session.inputLen]   = '\0';
    lcd.setCursor(0, 1);
    lcd.print(session.inputBuf);
    for (byte i = session.inputLen; i < 4; i++) lcd.print('_');

    if (session.inputLen   == 4) {      // Auto-submit when 4 digits entered
      delay(300);             // Brief pause so user sees 4th digit
      processInput();
    }
  }
}

// ── Loop ──────────────────────────────────────────────────
void loop() {
  switch (defaultState) {
    case HOME_STATE:
      handleHomeState();
      break;
    case INPUT_STATE:
      handleInputState();
      break;
  }
}

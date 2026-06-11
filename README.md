# Digital Timestamp — Assignment Submission System

An Arduino-based system that allows students to submit assignments
by entering the last 4 digits of their matric number on a keypad.
Submissions are verified against a student database on an SD card
and logged with a real-time timestamp.

---

## Hardware Required

| Component | Purpose |
|---|---|
| Arduino nano (FT232 USB chip) | Main microcontroller |
| DS3231 RTC Module | Real-time clock for timestamps |
| SD Card Module | Student database and submission logs |
| 4x4 Matrix Keypad | Student input interface |
| 16x2 I2C LCD | Display feedback to student |

---

## Wiring

| Component | Arduino Pin |
|---|---|
| SD Card CS | Pin 10 |
| Keypad Rows | Pins 9, 8, 7, 6 |
| Keypad Cols | Pins 5, 4, 3, 2 |
| LCD + RTC | SDA (A4), SCL (A5) |

---

## How It Works

1. System boots and displays live date and time
2. Student presses any key to enter input mode
3. Student types last 4 digits of matric number
4. System looks up the number in STUDENTS.CSV
5. If found — logs submission to LOGS.CSV with timestamp
6. If not found — displays NOT REGISTERED

---

## State Machine
HOME_STATE → (keypress) → INPUT_STATE → (4 digits) → lookup → HOME_STATE

---

## SD Card File Formats

**STUDENTS.CSV**
MCE/23/0042,Amina,Yusuf
MCE/23/0055,Ibrahim,Musa

**LOGS.CSV** (auto-generated)
Matric,Name,Date,Time,Status
MCE/??/0042,Amina,06/06/2026,14:32:07,SUBMITTED

> **Note:** The STUDENTS.CSV file in this repository contains
> sample data only. Replace with real student records before deployment.

---

## Libraries Used

- [RTClib](https://github.com/adafruit/RTClib)
- [SD](https://www.arduino.cc/en/Reference/SD)
- [Keypad](https://github.com/Chris--A/Keypad)
- [LiquidCrystal_I2C](https://github.com/johnrickman/LiquidCrystal_I2C)

---

## What I Learned

- Implementing a state machine on embedded hardware
- Pointer arithmetic and CSV parsing in C
- SRAM optimization using the F() macro
- Struct-based variable organization
- Modular, reusable function design
- Pass by reference vs pass by value

---

## 👨‍💻 Author

## ✍️ Author
* **Your Name** ([@marvelousolukunle-crypto](https://github.com)) — Core Code & System Integration

## 🤝 Acknowledgments
* Commissioned by MCE Group 2 for their embedded systems project.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Initial commit - Arduino Digital-timestamp

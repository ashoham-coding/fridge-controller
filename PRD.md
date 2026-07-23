# Product Requirements Document
## Wine Refrigerator Controller — Embedded Firmware

**Version:** 1.2
**Date:** 2026-03-29
**Status:** Draft

---

## 1. Product Overview

A standalone embedded controller for a wine refrigerator, based on an Arduino-compatible microcontroller. The system manages compressor cycling, interior lighting, temperature display, and user input via capacitive touch buttons. Temperature is measured with an NTC thermistor. Settings are persisted across power cycles via EEPROM.

---

## 2. Hardware Summary

| Component | Part | Interface |
|---|---|---|
| Microcontroller | Arduino (AVR) | — |
| Display | Adafruit 4-digit 7-segment (HT16K33) | I2C (0x70) |
| Touch input | Adafruit CAP1188 | I2C (0x29) |
| Temperature sensor | NTC thermistor (10kΩ, B=3950) | Analog (A0) |
| Compressor relay | — | Digital out (D7) |
| Fan relay | — | Digital out (D9) |
| Interior light relay | — | Digital out (D4) |
| Buzzer | — | Digital out (D8) |

---

## 3. Functional Requirements

### 3.1 Temperature Sensing

- **FR-TEMP-01:** The system shall read the NTC thermistor on pin A0 and compute temperature in °C using the Steinhart-Hart simplified (B-coefficient) equation:
  - Series resistor: 10,000 Ω
  - Nominal resistance: 10,000 Ω at 25°C
  - B coefficient: 3950
- **FR-TEMP-02:** The system shall average 30 ADC samples before computing a new temperature value, to reduce noise.
- **FR-TEMP-03:** ADC readings outside the range [5, 1018] shall be treated as a sensor fault (open or shorted thermistor).
- **FR-TEMP-04:** The displayed temperature shall use a ±0.5°C hysteresis band to prevent display flicker — it shall only update when the measured temperature differs from the displayed value by more than 0.5°C.

> **Implementation note:** The 30-sample average runs at loop speed with no time gate. Sample intervals are therefore variable. This is acceptable given the slow thermal time constant of the system, but the implementation should ensure the loop is not blocked by any long-running operation.

### 3.2 Compressor Control

- **FR-COMP-01:** The compressor and fan shall turn ON together when:
  - No sensor fault is active, AND
  - `currentTemp >= setTemp + 1°C`, AND
  - At least 180 seconds have elapsed since the compressor last turned OFF (anti-short-cycle protection).
- **FR-COMP-02:** The compressor and fan shall turn OFF together when `currentTemp <= setTemp - 1°C`.
- **FR-COMP-03:** The compressor shall never turn on within 180 seconds of the previous shutoff, regardless of temperature, to protect the compressor motor.
- **FR-COMP-04:** On initial power-up, the anti-short-cycle timer shall begin counting from boot, preventing compressor startup for the first 3 minutes. This is intentional safe behavior.

### 3.3 Temperature Set Point

- **FR-SET-01:** The user shall be able to adjust the set temperature using Up and Down capacitive touch buttons.
- **FR-SET-02:** The valid set temperature range is **3°C to 25°C** (inclusive).
- **FR-SET-03:** A single button press shall increment or decrement the set point by 1°C.
- **FR-SET-04:** Holding a button for more than 500 ms shall begin auto-repeat at 500 ms intervals (hold-to-repeat).
- **FR-SET-05:** Up and Down shall each track their own press-start timestamp independently, so pressing both simultaneously does not corrupt the hold-repeat timing of either.
- **FR-SET-06:** The set temperature shall be persisted to EEPROM after the user has not changed it for 3 seconds. See Section 3.7 for EEPROM requirements.

### 3.4 Interior Light

- **FR-LIGHT-01:** The Light capacitive touch button shall toggle the interior light on or off.
- **FR-LIGHT-02:** The light shall automatically turn off after 5 minutes (300,000 ms) of continuous use.
- **FR-LIGHT-03:** The light shall turn off immediately when the system is powered off via the Power button.

### 3.5 Power On/Off

- **FR-PWR-01:** The Power capacitive touch button shall toggle the system between ON and OFF states.
- **FR-PWR-02:** When the system is turned OFF:
  - The compressor and fan shall be turned off immediately.
  - The interior light shall be turned off immediately.
  - The display shall show `"OFF"`.
  - All other processing (temperature control, alarm) shall be suspended.
- **FR-PWR-03:** The system power state shall be persisted to EEPROM. See Section 3.7.
- **FR-PWR-04:** On power-up, the system shall restore the last saved ON/OFF state from EEPROM. If the stored value is not a valid boolean, the system shall default to **ON**.

### 3.6 Alarm

- **FR-ALARM-01:** A temperature alarm condition exists when `currentTemp >= setTemp + 3°C` OR `currentTemp <= setTemp - 3°C`, and no sensor fault is active.
- **FR-ALARM-02:** The alarm shall not activate immediately — it shall only trigger after the alarm condition has been sustained for 5 continuous minutes (300,000 ms).
- **FR-ALARM-03:** Once active, the alarm shall emit a short beep (1200 Hz, 120 ms) once per second via the buzzer.
- **FR-ALARM-04:** The alarm shall clear immediately when the temperature returns within the ±3°C band. The buzzer shall stop and the alarm timer shall reset.
- **FR-ALARM-05:** The low-temperature alarm (`currentTemp <= setTemp - 3°C`) shall be suppressed while the compressor is running, to avoid false alarms caused by normal cooling overshoot.
- **FR-ALARM-06:** When the alarm is active, the display shall show `"HI"` (high-temperature alarm) or `"LO"` (low-temperature alarm), blinking at the standard 700 ms interval. This display takes priority over both the current-temperature and set-temperature displays.
- **FR-ALARM-07:** A single press of any touch button shall silence the buzzer for 10 minutes (600,000 ms). The `"HI"` / `"LO"` display shall continue blinking during the silence period so the user remains aware of the fault condition. Pressing any button while already silenced resets the 10-minute timer. The silence expires automatically; if the alarm condition persists, the buzzer resumes.

### 3.7 EEPROM Persistence

- **FR-EEP-01:** The system shall persist `setTemp` (address 0) and `systemOn` (address 4) to EEPROM.
- **FR-EEP-02:** EEPROM writes shall only occur when a value has **actually changed** since the last save. A dirty flag shall be used to track pending changes.
- **FR-EEP-03:** EEPROM writes shall be deferred by at least 3 seconds after the last user change, to batch rapid button presses into a single write.
- **FR-EEP-04:** The write operation shall use `EEPROM.update()` (not `EEPROM.put()`) to skip the physical write when the stored value already matches, preserving write endurance (~100,000 cycles).
- **FR-EEP-05:** On startup, if `setTemp` is outside the valid range [3, 25], it shall be reset to the default value of **22°C**.
- **FR-EEP-06:** On startup, if `systemOn` is not a valid boolean (i.e., not `0x00` or `0x01`), it shall be reset to **true** (system ON).

> **Background:** The original implementation had a critical bug where EEPROM was written every ~3 seconds continuously, regardless of whether any values changed. This would exhaust the EEPROM write endurance within days of operation. FR-EEP-02 through FR-EEP-04 address this.

### 3.8 Display

- **FR-DISP-01:** The 7-segment display shall show the current temperature in °C during normal operation.
- **FR-DISP-01a:** Display priority (highest to lowest): sensor error (`"Err"`) → active alarm (`"HI"` / `"LO"`) → set-temperature window → current temperature.
- **FR-DISP-02:** When the user adjusts the set temperature, the display shall switch to show the **set temperature** for a 3-second window.
- **FR-DISP-03:** Within the set-temperature window:
  - For the first 1 second after a button press: the set temperature is shown **solid** (no blinking).
  - From 1 second to 3 seconds: the set temperature **blinks** at 700 ms intervals.
- **FR-DISP-04:** After the 3-second set window expires, the display returns to showing the current (measured) temperature.
- **FR-DISP-05:** The temperature shall be displayed as a two-digit integer (e.g., `" 8"` or `"14"`) followed by a `°C` indicator. Single-digit values shall **not** display a leading zero — the tens digit shall be blank.
- **FR-DISP-06:** When a sensor fault is active, the display shall show `"Err"` (not a numeric temperature).
- **FR-DISP-07:** When the system is OFF, the display shall show `"OFF"`.
- **FR-DISP-08:** The degree symbol and `"C"` indicator shall be rendered on the correct display digit. On the Adafruit 4-digit HT16K33 backpack, digit positions 0, 1, 3, 4 are full 7-segment digits; position 2 is the **colon** indicator. The `°C` character must be placed at position 3 or 4, not position 2.

- **FR-DISP-09:** While the compressor is blocked by the anti-short-cycle delay (including the mandatory 3-minute startup delay), the decimal point on the tens digit shall blink at the standard 700 ms interval. This provides a silent, non-intrusive indication that the compressor is temporarily inhibited. The indicator is visible during both the current-temperature and set-temperature display modes; it is not shown during alarm (`"HI"` / `"LO"`) or error (`"Err"`) displays.

> **Note on FR-DISP-08:** The current implementation writes `0x39` ("C") to position 2, which is the colon position on the 4-digit HT16K33 backpack. This is likely a display bug. Verify against the actual hardware; if using a custom 3-digit module with different position mapping, document the mapping explicitly.

> **Note on FR-DISP-05:** The current implementation uses `matrix.writeDigitNum(0, value / 10)`, which writes `0` for single-digit temperatures, producing a leading zero. The tens digit should be suppressed when `value < 10`.

---

## 4. Non-Functional Requirements

### 4.1 Reliability
- **NFR-REL-01:** The system shall protect the compressor from short-cycling (minimum 3-minute off time between starts).
- **NFR-REL-02:** The system shall not write to EEPROM more than necessary. Total EEPROM writes shall be minimized to ensure multi-year service life.
- **NFR-REL-03:** The system shall handle sensor faults gracefully — turning off the compressor and displaying an error — rather than making control decisions based on invalid readings.

### 4.2 Responsiveness
- **NFR-RESP-01:** Button presses shall be recognized within one loop iteration of the touch event occurring.
- **NFR-RESP-02:** The display shall refresh at least once per loop iteration.

### 4.3 Safety
- **NFR-SAFE-01:** On any power-up (including unexpected resets), the compressor shall not start for at least 3 minutes.
- **NFR-SAFE-02:** The compressor output pin shall be initialized LOW in `setup()` before any other operation.

---

## 5. Default Values

| Parameter | Default | Range |
|---|---|---|
| Set temperature | 22°C | 3–25°C |
| System state | ON | ON / OFF |
| Anti-short-cycle delay | 180 s | — |
| Light auto-off timeout | 300 s | — |
| Alarm delay before activation | 300 s | — |
| Alarm temperature deviation | ±3°C | — |
| Alarm silence duration | 600 s | — |
| Compressor hysteresis | ±1°C | — |
| Hold-to-repeat delay | 500 ms | — |
| Hold-to-repeat rate | 500 ms | — |
| Set-temp display window | 3000 ms | — |
| Set-temp blink-start offset | 1000 ms | — |
| Display blink interval | 700 ms | — |
| EEPROM write deferral | 3000 ms | — |

---

## 6. Known Issues & Open Items

| ID | Severity | Description |
|---|---|---|
| BUG-01 | Critical | EEPROM written every ~3 s continuously due to missing dirty flag — resolved by FR-EEP-02/03/04 |
| BUG-02 | Medium | `systemOn` not validated after EEPROM read — resolved by FR-EEP-06 |
| BUG-03 | Medium | `pressStartTime` shared between Up and Down buttons — resolved by FR-SET-05 |
| BUG-04 | Low | Leading zero displayed for single-digit temperatures — resolved by FR-DISP-05 |
| BUG-05 | Low | Degree/C indicator written to colon position on 4-digit display — resolved by FR-DISP-08 |
| BUG-06 | Low | Low-temp alarm fires during normal compressor overshoot — resolved by FR-ALARM-05 |
| OPEN-01 | — | Verify actual 7-segment display model and digit position mapping |
| OPEN-02 | — | Confirm whether systemOn should be restored on power-up or always boot ON |

---

## 7. Out of Scope

- Wi-Fi / Bluetooth connectivity
- Data logging or temperature history
- Multiple temperature zones
- Over-the-air firmware updates

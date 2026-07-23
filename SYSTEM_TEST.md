# Wine Fridge Controller — System Test Document

**Project:** WineFridgeController
**Board:** Arduino Leonardo (ATmega32U4)
**Date:** 2026-03-29
**Firmware file:** src/main.cpp

---

## Test Environment

| Item | Detail |
|---|---|
| Hardware | Arduino Leonardo + CAP1188 touch board (I2C 0x29) + 7-segment display (I2C 0x70) |
| Temperature sensor | NTC 10kΩ thermistor on A0, 10kΩ series resistor, B=3950 |
| Outputs | Compressor (pin 7), Fan (pin 9), Light (pin 4), Buzzer (pin 8) |
| Touch pads | C1=Power, C2=Light, C3=Temp Up, C4=Temp Down, C5=Unit toggle |
| Reference thermometer | Calibrated digital thermometer placed inside fridge |

---

## Section 1 — Power-On & Safety (NFR-SAFE-02)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 1.1 | Outputs LOW on boot | Power on board, measure pins 7, 9, 4 immediately | All pins LOW before any loop code runs | |
| 1.2 | Display initialises | Power on board | 7-segment shows current temperature within ~1 second | |
| 1.3 | Default set-point on blank EEPROM | Flash fresh firmware to new board | Set-point defaults to 12°C | |
| 1.4 | systemOn defaults ON on blank EEPROM | Flash fresh firmware to new board | System starts in ON state | |

---

## Section 2 — Temperature Reading (FR-TEMP-01 .. FR-TEMP-04)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 2.1 | Normal temperature reading | Place calibrated thermometer next to NTC sensor | Display matches reference ±2°C | |
| 2.2 | ADC averaging | Observe display over 30 seconds | Display is stable, no single-sample noise spikes | |
| 2.3 | Display hysteresis | Slowly change temperature by <0.5°C | Display value does not change | |
| 2.4 | Display hysteresis triggers | Change temperature by >0.5°C | Display updates to new value | |
| 2.5 | Sensor fault — open circuit | Disconnect NTC sensor wire | Display shows "Err", compressor turns OFF if running | |
| 2.6 | Sensor fault — short circuit | Short NTC pins together | Display shows "Err", compressor turns OFF if running | |
| 2.7 | Recovery from sensor fault | Reconnect NTC after fault | "Err" clears, normal operation resumes | |

---

## Section 3 — Power Button (FR-PWR-01 .. FR-PWR-02)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 3.1 | Toggle OFF | Touch C1 once | Display shows "OFF", compressor and light turn OFF | |
| 3.2 | Toggle ON | Touch C1 again | System resumes normal operation | |
| 3.3 | No processing while OFF | Touch C2–C5 while system is OFF | No response to any button | |
| 3.4 | Compressor shuts on power OFF | Run compressor, then press C1 | Compressor and fan turn OFF immediately | |
| 3.5 | Power state saved to EEPROM | Turn OFF, wait 4 seconds, power-cycle board | System restarts in OFF state | |

---

## Section 4 — Temperature Set-Point (FR-SET-01 .. FR-SET-05)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 4.1 | Increment set-point | Touch C3 once | Set-point increases by 1°C, display shows new value | |
| 4.2 | Decrement set-point | Touch C4 once | Set-point decreases by 1°C, display shows new value | |
| 4.3 | Upper limit | Hold C3 until set-point reaches 25°C | Set-point does not exceed 25°C | |
| 4.4 | Lower limit | Hold C4 until set-point reaches 3°C | Set-point does not go below 3°C | |
| 4.5 | Hold-repeat — Up | Hold C3 for >1 second | Set-point increments repeatedly every ~800 ms | |
| 4.6 | Hold-repeat — Down | Hold C4 for >1 second | Set-point decrements repeatedly every ~800 ms | |
| 4.7 | Set-point saved to EEPROM | Change set-point, wait 4 seconds, power-cycle | Set-point is restored after reboot | |
| 4.8 | EEPROM write deferred | Change set-point rapidly | No EEPROM write until 3 seconds after last press | |

---

## Section 5 — Display Behaviour (FR-DISP-02 .. FR-DISP-07)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 5.1 | Set-point display window | Press C3 or C4 | Display switches to set-point value for 3 seconds, then returns to current temp | |
| 5.2 | Solid then blink | Press C3 and observe display | Set-point shows solid for first 1 second, then blinks at ~700 ms until 3 second window ends | |
| 5.3 | No leading zero | When temperature is single digit (e.g. 9°C) | Tens digit is blank, not "0" | |
| 5.4 | Celsius indicator | Default mode | Third digit shows "C" | |
| 5.5 | "OFF" text | System in OFF state | Display shows "OFF" | |
| 5.6 | "Err" text | Sensor fault active | Display shows "Err" | |

---

## Section 6 — Unit Toggle / °C ↔ °F (C5)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 6.1 | Toggle to Fahrenheit | Touch C5 once | Display converts to °F, third digit shows "F" | |
| 6.2 | Toggle back to Celsius | Touch C5 again | Display reverts to °C, third digit shows "C" | |
| 6.3 | Fahrenheit conversion accuracy | Set-point = 12°C, toggle to °F | Display shows 54°F (12×9/5+32=53.6, rounds to 54) | |
| 6.4 | Set-point display in °F | Press C3/C4 while in °F mode | Set-point blink window shows value in °F | |
| 6.5 | No leading zero in °F | Temperature below 100°F (e.g. 9°C = 48°F) | Tens digit shows "4", units digit shows "8" correctly | |

---

## Section 7 — Light (FR-LIGHT-01 .. FR-LIGHT-02)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 7.1 | Toggle ON | Touch C2 once | Light turns ON (pin 4 HIGH) | |
| 7.2 | Toggle OFF | Touch C2 again | Light turns OFF (pin 4 LOW) | |
| 7.3 | Auto-off after 5 minutes | Turn light on, wait 5 minutes | Light turns off automatically | |
| 7.4 | Auto-off timer resets on press | Turn light on, wait 4 minutes, press C2 twice (off then on) | New 5-minute timer starts from last press | |
| 7.5 | Light off when system powered off | Light is ON, press C1 to power off | Light turns OFF immediately | |

---

## Section 8 — Compressor Control (FR-COMP-01 .. FR-COMP-04)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 8.1 | Compressor turns ON | Allow temperature to rise 1°C above set-point | Compressor (pin 7) and fan (pin 9) turn ON | |
| 8.2 | Compressor turns OFF | Allow temperature to fall 1°C below set-point | Compressor and fan turn OFF | |
| 8.3 | Anti-short-cycle — blocked | Turn compressor off manually, immediately trigger on condition | Compressor does NOT start within 3 minutes of last stop | |
| 8.4 | Anti-short-cycle — released | Wait 3 minutes after compressor stop with temperature above set+1 | Compressor starts normally | |
| 8.5 | Fan follows compressor | Observe pin 9 during compressor cycle | Fan is always ON when compressor is ON, OFF when compressor is OFF | |
| 8.6 | Compressor OFF on sensor fault | Compressor running, disconnect NTC | Compressor and fan turn OFF immediately | |

---

## Section 9 — Alarm (FR-ALARM-01 .. FR-ALARM-05)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 9.1 | High temp alarm — delayed | Raise temperature to set-point +3°C | No beep for first 5 minutes | |
| 9.2 | High temp alarm — fires | Keep temperature at set-point +3°C for >5 minutes | Buzzer beeps 1200 Hz, 120 ms, once per second | |
| 9.3 | High temp alarm — clears | Return temperature to normal range | Buzzer stops immediately | |
| 9.4 | Low temp alarm — fires | Lower temperature to set-point −3°C, compressor OFF | Buzzer beeps after 5-minute delay | |
| 9.5 | Low temp alarm — suppressed during cooling | Compressor running causes overshoot below set−3°C | No alarm while compressor is ON | |
| 9.6 | Low temp alarm clears | Temperature returns to normal range | Buzzer stops | |

---

## Section 10 — EEPROM (FR-EEP-02 .. FR-EEP-06)

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 10.1 | Set-point persists across reboot | Set to 18°C, wait 4s, power cycle | Restores to 18°C | |
| 10.2 | systemOn persists across reboot | Power OFF, wait 4s, power cycle | Restores to OFF state | |
| 10.3 | Out-of-range EEPROM value | Manually write 0xFF to EEPROM address 0, reboot | Defaults to 12°C (invalid value clamped) | |
| 10.4 | Dirty flag — no premature write | Change set-point, immediately power cycle (<3s) | Old set-point restored (write was not committed yet) | |

---

## Section 11 — Edge Cases & Stress

| # | Test | Steps | Expected | Pass/Fail |
|---|---|---|---|---|
| 11.1 | Rapid button presses | Press all touch pads rapidly in sequence | No crashes, no unintended state changes | |
| 11.2 | millis() rollover (optional) | Run board continuously for ~50 days OR simulate by patching millis to near 0xFFFFFFFF | Timers (anti-short-cycle, light timeout, alarm) behave correctly across rollover | |
| 11.3 | Power cycle during EEPROM write | Change set-point, cut power within 1 second | Board reboots cleanly, no corruption (EEPROM.update is atomic per byte) | |
| 11.4 | I2C bus absent | Boot with display or CAP1188 disconnected | Board does not hang (may show garbled display, but loop continues) | |

---

## Sign-off

| Tester | Date | Firmware version / commit | Notes |
|---|---|---|---|
| | | | |

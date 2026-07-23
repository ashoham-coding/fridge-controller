# System Description
## Wine Refrigerator Controller — Embedded Firmware

**Version:** 1.0
**Date:** 2026-03-30
**Status:** Current

---

## 1. Purpose

This document describes the design and runtime behaviour of the Wine Refrigerator Controller firmware. It is intended as a technical reference for anyone reading, maintaining, or extending the code. For formal requirements see [PRD.md](PRD.md).

---

## 2. Hardware Platform

| Item | Detail |
|---|---|
| Microcontroller | Arduino (AVR, e.g. Leonardo / Uno) |
| Display | Adafruit 4-digit 7-segment backpack (HT16K33, I2C 0x70) |
| Touch controller | Adafruit CAP1188 (I2C 0x29) |
| Temperature sensor | NTC thermistor 10 kΩ, B = 3950, voltage-divider to A0 |
| Compressor relay | D7 — active HIGH |
| Fan relay | D9 — active HIGH (always mirrors compressor) |
| Interior light relay | D4 — active HIGH |
| Buzzer | D8 — driven with `tone()` |

Both I2C breakout boards include on-board 10 kΩ pull-up resistors; no external pull-ups are required.

---

## 3. Firmware Architecture

The firmware is a single-file Arduino sketch ([src/main.cpp](src/main.cpp)) structured as a classic `setup()` / `loop()` pair. There is no RTOS; all timing is cooperative using `millis()` timestamps. The loop runs as fast as the hardware allows (no `delay()` calls anywhere).

### 3.1 Startup Sequence (`setup()`)

1. **Watchdog disabled** — `wdt_disable()` is called first to prevent a reset loop if the MCU rebooted due to a prior watchdog timeout (AVR leaves the WDT armed after a WDT reset).
2. **Output pins driven LOW** — compressor, fan, and light relays are forced LOW before any other initialisation, ensuring no unintended activation.
3. **Peripherals initialised** — I2C bus, display (brightness 5/15), and CAP1188 touch controller.
4. **EEPROM restored** — `setTemp` and `systemOn` are read and validated. Out-of-range values fall back to safe defaults (12 °C / ON).
5. **Anti-short-cycle timer seeded** — `lastCompressorOffTime = millis()` ensures the compressor cannot start for 3 minutes after every power-on, even after an unexpected reset.
6. **Watchdog enabled** at 8 s (`WDTO_8S`).

### 3.2 Main Loop (`loop()`)

Each iteration, in order:

| Step | Action |
|---|---|
| 1 | `wdt_reset()` — pet the watchdog |
| 2 | ADC sample accumulation and temperature calculation |
| 3 | Touch button scan |
| 4 | Power button handling |
| 5 | Early return if system is OFF (show `"OFF"`, skip all control) |
| 6 | Light button handling (short press / long press) |
| 7 | Up / Down set-temperature handling (press + hold-repeat) |
| 8 | Unit toggle (°C ↔ °F) |
| 9 | Compressor on/off decisions |
| 10 | Alarm evaluation and buzzer control |
| 11 | Deferred EEPROM write |
| 12 | Display update |

---

## 4. Subsystem Descriptions

### 4.1 Temperature Measurement

The NTC thermistor is connected as a voltage divider against a 10 kΩ series resistor. The ADC reading is averaged over 30 consecutive samples (no time gate — sample rate follows loop speed) to reduce electrical noise.

The resistance is converted to temperature using the simplified Steinhart-Hart (B-coefficient) equation:

```
R = R_series × ADC / (1023 − ADC)
T(K) = 1 / ( ln(R / R_nominal) / B  +  1 / (T_nominal + 273.15) )
T(°C) = T(K) − 273.15
```

ADC values outside [5, 1018] indicate an open or shorted thermistor and set `sensorError = true`. On sensor fault the compressor is turned off immediately and the display shows `"Err"`.

A ±0.5 °C hysteresis band prevents the displayed temperature from flickering.

### 4.2 Compressor Control

The compressor and fan always switch together. Control logic:

- **Turn ON:** `currentTemp ≥ setTemp + 1 °C` AND no sensor fault AND at least 180 s since last shutoff.
- **Turn OFF:** `currentTemp ≤ setTemp − 1 °C`.
- **Anti-short-cycle:** The 180 s minimum-off timer is enforced on every shutoff, including startup. This protects the compressor motor from rapid cycling.

When the compressor is blocked by the anti-short-cycle timer and temperature demands cooling, the decimal point on the tens digit of the display blinks at 700 ms to silently indicate the inhibit state.

#### Duty Cycle Tracking

`compressorOnAccum` accumulates total compressor ON time (ms). `compressorOnStart` records the timestamp of the most recent ON transition. When the compressor turns off, `now − compressorOnStart` is added to `compressorOnAccum`.

Duty cycle at any instant:

```
pct = (compressorOnAccum + (compressorOn ? now − compressorOnStart : 0)) × 100 / now
```

This is a lifetime average from boot. It converges toward the true long-term duty cycle over time.

### 4.3 Touch Button Interface

Five capacitive touch channels on the CAP1188 are polled each loop iteration. Edge detection (previous state vs current state) is used for all single-press actions. Hold-repeat is implemented for Up and Down with independent per-button timestamps.

| Channel | Button | Action |
|---|---|---|
| C1 | Power | Toggle system ON / OFF (on press-down) |
| C2 | Light | Short press: toggle light; Long press (≥ 3 s): show duty cycle |
| C3 | Up | Increase set temperature by 1 °C per press; hold-repeat after 1 s |
| C4 | Down | Decrease set temperature by 1 °C per press; hold-repeat after 1 s |
| C5 | Unit | Toggle display between °C and °F |

**Light long-press detection:** the press-start timestamp is recorded on press-down. The duty cycle screen activates while the button is still held once the 3 s threshold passes. The light toggle fires on release only if the button was held for less than 3 s.

**Alarm silence:** pressing any button while an alarm is active silences the buzzer for 10 minutes. The display continues blinking `"HI"` or `"LO"` throughout the silence period.

### 4.4 Temperature Set Point

Range: 3 °C to 25 °C inclusive. Changes are reflected on the display immediately (set-temperature window). The display returns to current temperature 3 s after the last button press. Within the window: solid for the first 1 s, blinking from 1 s to 3 s.

In °F mode the set point is stored internally as °C and converted for display only.

### 4.5 Interior Light

Toggled by a short press of C2. Auto-off after 5 minutes of continuous use. Turns off immediately when the system is powered off.

### 4.6 Alarm

Alarm condition: `|currentTemp − setTemp| ≥ 3 °C` sustained for 5 continuous minutes. Exception: the low-temperature alarm is suppressed while the compressor is running to avoid false alarms from normal cooling overshoot.

Once active: 1200 Hz / 120 ms beep once per second. Display shows blinking `"HI"` or `"LO"`. Any button press silences the buzzer for 10 minutes; the visual indication persists. The alarm resets immediately when temperature returns within the ±3 °C band.

### 4.7 EEPROM Persistence

`setTemp` (2 bytes at address 0) and `systemOn` (1 byte at address 4) are saved to EEPROM. Writes are deferred 3 s after the last change and use `EEPROM.update()`, which skips the physical write when the stored value already matches. This limits write cycles to intentional user changes only, preserving the ~100,000-cycle endurance budget.

### 4.8 Display Logic

Display priority (highest wins):

| Priority | Condition | Shows |
|---|---|---|
| 1 | Sensor fault | `"Err"` |
| 2 | `dutyCycleActive` | `"XXd"` (percentage + `d`) |
| 3 | Alarm active | Blinking `"HI"` or `"LO"` |
| 4 | Set-temperature window (< 3 s since last change) | Set temperature (solid 0–1 s, blinking 1–3 s) |
| 5 | Normal | Current temperature |
| — | System OFF | `"OFF"` (all other display logic bypassed) |

Temperature format: two digits + `°C` or `°F`. Leading zero suppressed for single-digit values. Decimal point on the tens digit blinks when the compressor is in anti-short-cycle delay.

Duty cycle format: two digits + `d` (e.g. `37d`). Shown for 10 s after a light long-press.

### 4.9 Watchdog

An AVR hardware watchdog is armed at 8 s in `setup()`. `wdt_reset()` is called at the top of every `loop()` iteration. If the loop ever stalls (e.g. I2C hang), the MCU resets within 8 s. `wdt_disable()` at the start of `setup()` prevents a reset loop by clearing the WDT flag left over from a prior WDT-triggered reset.

---

## 5. Global State Summary

| Variable | Type | Description |
|---|---|---|
| `setTemp` | `int` | Target temperature in °C (3–25) |
| `systemOn` | `bool` | System power state |
| `currentTemp` | `float` | Most recent computed temperature (°C) |
| `displayedTemp` | `float` | Hysteresis-filtered display temperature |
| `compressorOn` | `bool` | Compressor / fan relay state |
| `lightOn` | `bool` | Interior light relay state |
| `sensorError` | `bool` | True when ADC reading is out of valid range |
| `displayFahrenheit` | `bool` | Display unit preference |
| `blinkState` | `bool` | Current phase of 700 ms blink oscillator |
| `settingsDirty` | `bool` | Pending EEPROM write flag |
| `dutyCycleActive` | `bool` | Duty cycle screen is showing |
| `compressorOnAccum` | `ulong` | Lifetime compressor ON time accumulator (ms) |
| `compressorOnStart` | `ulong` | Timestamp of most recent compressor ON event |
| `lastCompressorOffTime` | `ulong` | Timestamp of most recent compressor OFF event |
| `alarmStartTime` | `ulong` | Timestamp when alarm condition first appeared |
| `alarmSilencedUntil` | `ulong` | Buzzer silenced until this timestamp |

---

## 6. Timing Parameters

| Constant | Value | Purpose |
|---|---|---|
| `ANTI_SHORT_CYCLE` | 180 000 ms | Minimum compressor off time |
| `LIGHT_TIMEOUT` | 300 000 ms | Interior light auto-off |
| `ALARM_DELAY` | 300 000 ms | Alarm condition hold before activation |
| `ALARM_SILENCE_DURATION` | 600 000 ms | Buzzer snooze after button press |
| `HOLD_DELAY` | 1 000 ms | Hold-to-repeat initial delay |
| `REPEAT_RATE` | 800 ms | Hold-to-repeat interval |
| `SET_WINDOW` | 3 000 ms | Set-temperature display window |
| `STABLE_AFTER_PRESS` | 1 000 ms | Solid display period at start of set window |
| `BLINK_INTERVAL` | 700 ms | Display blink half-period |
| `EEPROM_DEFERRAL` | 3 000 ms | Quiet time before EEPROM write |
| `LIGHT_LONG_PRESS` | 3 000 ms | Long-press threshold for duty cycle display |
| `DUTY_CYCLE_SHOW` | 10 000 ms | Duty cycle screen display duration |

# Wine Fridge Controller — User Manual

## Overview

The Wine Fridge Controller is an Arduino-based smart temperature controller for wine refrigerators. It continuously monitors the internal temperature via a thermistor sensor and cycles the compressor to maintain your chosen set point. It features a touch-sensitive control panel, a 7-segment LED display, audible alarms, and persistent memory so your settings are retained across power cuts.

---

## Controls

The panel has five capacitive touch buttons:

| Button | Label | Function |
|--------|-------|----------|
| C1 | **Power** | Turn the system ON or OFF |
| C2 | **Light** | Toggle the interior light |
| C3 | **Temp ▲** | Raise the set temperature |
| C4 | **Temp ▼** | Lower the set temperature |
| C5 | **Unit** | Switch between °C and °F |

**Button tips:**
- **Hold C3 or C4** for more than 1 second to change the temperature continuously (repeats every 0.8 s).
- **Long-press C2** (hold 3+ seconds) to view the compressor duty-cycle statistic for 10 seconds.
- **Any button press** while an alarm is sounding will silence the buzzer for 10 minutes.

---

## Display

The 4-digit LED display shows different information depending on the current state:

| Display | Meaning |
|---------|---------|
| `12C` / `54F` | Current temperature (normal operation) |
| `12C` blinking | Set-point temperature (shown for 3 s after pressing ▲ or ▼) |
| `OFF` | System is powered off |
| `Err` | Temperature sensor fault — see Troubleshooting |
| `HI` blinking | High-temperature alarm active |
| `LO` blinking | Low-temperature alarm active |
| `37d` | Compressor duty-cycle percentage (shown after long-pressing Light) |

**Decimal-point indicator:** A blinking dot on the tens digit means the compressor is in its **anti-short-cycle delay** — it will start automatically once the delay expires.

---

## First Use

1. Power on the unit. The display will briefly show the previous set point, then settle on the current temperature.
2. Press **Temp ▲ / ▼** to set your desired temperature. The recommended range for red wine is **12–18 °C**; for white or sparkling wine, **6–12 °C**.
3. Press **Unit (C5)** if you prefer Fahrenheit. This setting is not saved across power cycles.
4. The compressor will start automatically (after the 3-minute anti-short-cycle delay on first power-up).

---

## Setting the Temperature

- Press **Temp ▲ (C3)** to raise the set point, **Temp ▼ (C4)** to lower it.
- The display switches to the set-point value for **3 seconds** so you can see what you have chosen, then returns to the current temperature.
- Allowed range: **3 °C – 25 °C** (37 °F – 77 °F).
- The new set point is automatically saved to memory **3 seconds** after the last button press.

---

## Temperature Control

The controller uses a hysteresis band to protect the compressor:

- **Compressor starts** when the temperature rises to **set point + 1 °C**.
- **Compressor stops** when the temperature falls to **set point − 1 °C**.

This ±1 °C hysteresis prevents the compressor from starting and stopping too frequently.

---

## Anti-Short-Cycle Protection

To protect the compressor motor, the controller enforces a **3-minute minimum off-time** after every shutdown (including power-up). During this delay:

- The decimal point on the tens digit blinks to indicate the wait.
- All other functions (light, display, buttons) work normally.
- The compressor starts automatically once the delay expires, if cooling is needed.

---

## Interior Light

- Press **Light (C2)** to toggle the interior light on or off.
- The light turns off automatically after **5 minutes** of continuous use.
- The light is disabled when the system is powered off.

---

## Powering Off

Press **Power (C1)** to turn the system off.

- The compressor, fan, and light are immediately disabled.
- The display shows `OFF`.
- All buttons except Power are ignored while the system is off.
- The off state is saved to memory and will be restored after a power cut.

Press **Power (C1)** again to turn the system back on. The anti-short-cycle delay will restart from the beginning.

---

## Alarms

### High-Temperature Alarm

**Trigger:** Temperature exceeds set point + 3 °C for more than 5 minutes.

**Indication:**
- Display blinks `HI`
- Buzzer beeps at 1200 Hz, once per second

**Action:** Check that the door is fully closed and the unit is not overfilled. The alarm clears automatically once the temperature returns to within 3 °C of the set point.

---

### Low-Temperature Alarm

**Trigger:** Temperature falls below set point − 3 °C for more than 5 minutes (only when compressor is off).

**Indication:**
- Display blinks `LO`
- Buzzer beeps at 1200 Hz, once per second

**Action:** Check whether the ambient room temperature is unusually low. The alarm clears automatically once the temperature returns to within 3 °C of the set point.

---

### Silencing the Buzzer

Press **any button** to silence the buzzer for **10 minutes**. The visual alarm (blinking `HI` or `LO`) continues until the temperature recovers — this is by design so you can confirm the alarm has not cleared on its own.

---

## Sensor Fault (`Err`)

If the thermistor is disconnected, broken, or short-circuited, the display shows `Err`.

- The compressor turns off immediately (fail-safe).
- All temperature control and alarm processing is suspended.
- The fault clears automatically when the sensor returns to a valid reading.

If `Err` persists, check the thermistor wiring at the sensor and at the controller board.

---

## Duty-Cycle Statistics

Long-press **Light (C2)** (hold for 3+ seconds) to view the compressor duty cycle. The display shows a number followed by `d` (e.g., `37d` = 37 %). This is the percentage of total system-on time that the compressor has been running. A high duty cycle may indicate a temperature set point that is too low for the ambient conditions, a door seal issue, or an undersized unit.

The display returns to normal after 10 seconds.

---

## Settings Saved to Memory

The following settings are retained across power cuts:

| Setting | Default |
|---------|---------|
| Set temperature | 12 °C |
| System power state (on/off) | On |

The temperature unit (°C / °F) is **not** saved and resets to °C on power-up.

---

## Specifications

| Parameter | Value |
|-----------|-------|
| Set-point range | 3 – 25 °C (37 – 77 °F) |
| Control hysteresis | ±1 °C |
| Alarm threshold | ±3 °C from set point |
| Alarm activation delay | 5 minutes |
| Buzzer silence duration | 10 minutes |
| Anti-short-cycle delay | 3 minutes |
| Light auto-off timeout | 5 minutes |
| Display brightness | Fixed (medium) |
| Temperature sensor | NTC thermistor 10 kΩ, B = 3950 |

---

## Troubleshooting

| Symptom | Likely cause | Action |
|---------|-------------|--------|
| Display shows `Err` | Thermistor disconnected or broken | Check sensor wiring |
| Compressor does not start | Anti-short-cycle delay active (dot blinking) | Wait up to 3 minutes |
| Compressor does not start (no dot) | System off, or temperature already at/below set point | Check power state and current temperature |
| Display shows `OFF` | System powered off by C1 | Press Power (C1) to turn on |
| Buzzer beeping | Temperature alarm active (`HI` or `LO` on display) | Press any button to silence; check conditions |
| Temperature drifts out of range | Door seal, heavy load, or high ambient temperature | Adjust set point or inspect fridge seal |
| Settings lost after power cut | EEPROM write did not complete | Wait 3 s after the last button press before removing power |

---

## Wiring Reference

| Signal | Pin |
|--------|-----|
| Thermistor (NTC, voltage divider) | A0 |
| Compressor relay | D7 |
| Fan relay | D9 |
| Interior light relay | D4 |
| Buzzer (piezo) | D8 |
| 7-segment display (I2C, addr 0x70) | SDA / SCL |
| Touch controller (I2C, addr 0x29) | SDA / SCL |

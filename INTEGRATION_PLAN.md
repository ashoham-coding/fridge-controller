# Integration Plan — Wine Fridge Controller

**Version:** 1.0
**Date:** 2026-04-01
**Status:** Draft

---

## Phase 1: Pre-Integration Checks

### 1.1 Software
- Flash the current firmware and verify it compiles clean with no warnings.
- Confirm EEPROM defaults are sane (setTemp = 22°C, systemOn = ON) — will be validated on first boot.

### 1.2 Hardware bench test (before mounting)
- Verify I2C bus: scan for devices at 0x70 (display) and 0x29 (CAP1188). Both boards have on-board pull-ups so no external resistors needed.
- Confirm relay outputs on D7, D9, D4 toggle correctly with a multimeter before connecting to fridge loads.
- Confirm buzzer fires on D8 (`tone()`).
- Confirm thermistor reads a plausible temperature on A0 at room temperature (~25°C → ADC ≈ 512).

---

## Phase 2: Wiring

### 2.1 Power

| Rail | Source | Notes |
|---|---|---|
| Arduino 5V | Fridge power supply (regulated 5V) or USB during dev | Must remain powered when fridge body is ON |
| Relay coil voltage | Match relay board spec (typically 5V or 12V) | Verify relay module's rated coil voltage |

### 2.2 Load connections

| Pin | Load | Wiring note |
|---|---|---|
| D7 | Compressor relay NO contact | In series with compressor motor circuit |
| D9 | Fan relay NO contact | In series with fan motor circuit |
| D4 | Interior light relay NO contact | In series with light circuit |
| D8 | Buzzer | Small passive buzzer, series resistor if needed |

> All relay outputs are **active HIGH**. Confirm relay module polarity — some modules are active LOW and will need firmware adjustment.

### 2.3 Sensor
- Place the NTC thermistor inside the fridge cavity, away from the evaporator (cold plate) and the compressor heat.
- Ideally mid-shelf height, not touching any surface.
- Wire: one leg to A0, other leg to GND. Series 10 kΩ resistor from A0 to 5V (voltage divider).

### 2.4 UI board (display + touch panel)
- Mount display and touch panel on the front panel.
- CAP1188: run short wire traces for touch pads; long runs increase parasitic capacitance and can cause false triggers — keep traces under ~15 cm if possible.
- I2C lines: SDA → A4, SCL → A5 (standard Arduino Uno/Leonardo pinout).

---

## Phase 3: First Power-On Sequence

1. Power on with compressor/fan/light **disconnected** (relays only, no loads).
2. Verify display shows current temperature (room temp, ~20–25°C).
3. Verify touch buttons respond: Up/Down change setpoint, Power toggles OFF/ON, Light toggles relay D4.
4. Verify anti-short-cycle timer: confirm D7 stays LOW for 3 minutes after boot (decimal point on display blinks during inhibit).
5. Reconnect loads. Verify compressor + fan energize together after the 3-minute timer expires and temperature exceeds setTemp + 1°C.

---

## Phase 4: Known Issues to Resolve During Integration

These are open items from the PRD that can only be confirmed against real hardware:

| ID | Item | What to check |
|---|---|---|
| OPEN-01 / BUG-05 | Display digit mapping | Confirm °C indicator appears on the correct digit (not the colon segment at position 2). If wrong, adjust `writeDigitRaw()` call in firmware. |
| BUG-04 | Leading zero | Confirm single-digit temps (e.g., 8°C) show ` 8` not `08`. |
| OPEN-02 | systemOn on power-up | Decide: should the fridge always boot ON, or restore last saved state? EEPROM restore is currently implemented — verify this is desired. |
| Relay polarity | Active HIGH assumption | If relay module is active LOW, invert logic in firmware for D4/D7/D9. |

---

## Phase 5: Functional Sign-Off Tests

Tie directly to SYSTEM_TEST.md once the system is installed:

1. **Cooling cycle**: set setTemp below room temp; verify compressor + fan start after 3 min, cool to within ±1°C, then stop.
2. **Anti-short-cycle**: turn compressor off, immediately set high setTemp to demand cooling — confirm 3-minute lockout is enforced.
3. **Alarm**: force temperature deviation ≥ 3°C for 5 minutes; verify `HI`/`LO` display + beep. Press any button; verify beep stops but display continues.
4. **Sensor fault**: disconnect thermistor; confirm `Err` display and compressor OFF.
5. **EEPROM**: change setTemp, power cycle; verify value is restored.
6. **Light auto-off**: turn light on, wait 5 minutes; confirm it turns off automatically.
7. **Watchdog**: (optional, careful) stall the I2C bus and verify MCU resets within 8 s.

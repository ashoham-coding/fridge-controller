#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <Adafruit_CAP1188.h>
#include <math.h>
#include <EEPROM.h>

// ---------- Objects ----------
Adafruit_7segment matrix = Adafruit_7segment();
Adafruit_CAP1188 cap = Adafruit_CAP1188();

// ---------- Pins ----------
#define NTC_PIN        A0
#define COMPRESSOR_PIN 7
#define FAN_PIN        9
#define LIGHT_PIN      4
#define BUZZER_PIN     8

// ---------- EEPROM addresses ----------
#define EEPROM_SET_TEMP  0   // 2 bytes (int)
#define EEPROM_SYSTEM_ON 4   // 1 byte  (bool stored as uint8_t)

// ---------- NTC thermistor constants ----------
const float SERIES_RESISTOR     = 10000.0;
const float NOMINAL_RESISTANCE  = 10000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float B_COEFFICIENT       = 3950.0;

// ---------- Settings ----------
int  setTemp  = 12;       // FR-EEP-05 default
bool systemOn = true;

float currentTemp   = 0;
float displayedTemp = -100;  // sentinel: not yet initialised

bool compressorOn      = false;
bool lightOn           = false;
bool sensorError       = false;
bool displayFahrenheit = false;

// ---------- Timing constants ----------
const unsigned long ANTI_SHORT_CYCLE        = 180000UL;   // FR-COMP-03
const unsigned long LIGHT_TIMEOUT           = 300000UL;   // FR-LIGHT-02
const unsigned long ALARM_DELAY             = 300000UL;   // FR-ALARM-02
const unsigned long ALARM_SILENCE_DURATION  = 600000UL;   // 10-minute snooze
const unsigned long HOLD_DELAY         = 1000UL;     // FR-SET-04
const unsigned long REPEAT_RATE        = 800UL;      // FR-SET-04 (defaults table)
const unsigned long SET_WINDOW         = 3000UL;     // FR-DISP-02
const unsigned long STABLE_AFTER_PRESS = 1000UL;     // FR-DISP-03
const unsigned long BLINK_INTERVAL     = 700UL;      // FR-DISP-03
const unsigned long EEPROM_DEFERRAL    = 3000UL;     // FR-EEP-03
const unsigned long LIGHT_LONG_PRESS   = 3000UL;     // FR-DISP-10: long press to show duty cycle
const unsigned long DUTY_CYCLE_SHOW    = 10000UL;    // FR-DISP-10: duty cycle display duration

// ---------- Runtime timestamps ----------
unsigned long lastCompressorOffTime = 0;
unsigned long lightOnTime           = 0;
unsigned long alarmStartTime        = 0;
unsigned long alarmSilencedUntil    = 0;
unsigned long lastSetChangeTime     = 0;
unsigned long lastBlinkTime         = 0;
unsigned long lightPressStart       = 0;
unsigned long dutyCycleUntil        = 0;

// ---------- Compressor duty cycle tracking ----------
unsigned long compressorOnAccum     = 0;  // total accumulated ON time (ms)
unsigned long compressorOnStart     = 0;  // millis() when compressor last turned ON

// ---------- State flags ----------
bool blinkState       = true;
bool settingsDirty    = false;  // FR-EEP-02
bool dutyCycleActive  = false;  // FR-DISP-10

// ---------- Per-button hold-repeat timestamps (FR-SET-05) ----------
unsigned long upPressStart   = 0;
unsigned long upLastRepeat   = 0;
unsigned long downPressStart = 0;
unsigned long downLastRepeat = 0;

// ---------- Previous touch states ----------
bool powerPressed = false;
bool lightPressed = false;
bool upPressed    = false;
bool downPressed  = false;
bool unitPressed  = false;

// ---------- ADC averaging ----------
float adcSum  = 0;
int   adcCount = 0;

// ---------- External C declarations ----------
extern "C" void wdt_enable_safe(void);

// ---------- Forward declarations ----------
void compressorOff();
void showTemp(int value, bool dotFirst = false);
void showOFF();
void showError();
void showHI();
void showLO();
void showDutyCycle(int pct);
void beepPattern();

// =====================================================

void setup() {

  // NFR-SAFE-02: initialise outputs LOW before anything else
  pinMode(COMPRESSOR_PIN, OUTPUT);
  pinMode(FAN_PIN,        OUTPUT);
  pinMode(LIGHT_PIN,      OUTPUT);
  pinMode(BUZZER_PIN,     OUTPUT);

  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(FAN_PIN,        LOW);
  digitalWrite(LIGHT_PIN,      LOW);

  Wire.begin();
  matrix.begin(0x70);
  matrix.setBrightness(5);
  cap.begin(0x29);

  // Restore settings from EEPROM
  EEPROM.get(EEPROM_SET_TEMP, setTemp);

  // FR-EEP-05: validate setTemp
  if (setTemp < 3 || setTemp > 25) setTemp = 12;

  // FR-EEP-06: validate systemOn — read raw byte to detect uninitialised EEPROM
  uint8_t sysByte;
  EEPROM.get(EEPROM_SYSTEM_ON, sysByte);
  if      (sysByte == 0x00) systemOn = false;
  else if (sysByte == 0x01) systemOn = true;
  else                      systemOn = true;  // default for corrupt/blank EEPROM

  // FR-COMP-03: enforce anti-short-cycle on startup — compressor blocked for 3 min after power-on
  lastCompressorOffTime = millis();

  // NFR-SAFE-03: enable watchdog in interrupt+reset mode — resets MCU if loop
  // stalls for more than 8 s; ISR writes Caterina magic key so uploads work
  // without double-tapping the reset button (see wdt_init.c)
  wdt_enable_safe();
}

// =====================================================

void loop() {

  wdt_reset();  // NFR-SAFE-03: pet the watchdog each iteration

  unsigned long now = millis();

  // ---------- Temperature reading (FR-TEMP-01 .. FR-TEMP-04) ----------
  adcSum += analogRead(NTC_PIN);
  adcCount++;

  if (adcCount >= 30) {

    float avg = adcSum / 30.0;
    adcSum   = 0;
    adcCount = 0;

    if (avg < 5 || avg > 1018) {
      // FR-TEMP-03: sensor fault
      sensorError = true;
      // NFR-REL-03: shut compressor off immediately on sensor fault
      if (compressorOn) compressorOff();

    } else {
      sensorError = false;

      float resistance = SERIES_RESISTOR * (avg / (1023.0 - avg));
      float steinhart  = log(resistance / NOMINAL_RESISTANCE);
      steinhart /= B_COEFFICIENT;
      steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
      currentTemp = 1.0 / steinhart - 273.15;

      // FR-TEMP-04: ±0.5 °C hysteresis on display value
      if (displayedTemp == -100)             displayedTemp = currentTemp;
      if (currentTemp > displayedTemp + 0.5) displayedTemp = currentTemp;
      if (currentTemp < displayedTemp - 0.5) displayedTemp = currentTemp;
    }
  }

  // ---------- Touch buttons ----------
  uint8_t touched = cap.touched();

  bool powerNow = touched & (1 << 0);  // C1 — FR-PWR-01
  bool lightNow = touched & (1 << 1);  // C2 — FR-LIGHT-01
  bool upNow    = touched & (1 << 2);  // C3 — FR-SET-01
  bool downNow  = touched & (1 << 3);  // C4 — FR-SET-01
  bool unitNow  = touched & (1 << 4);  // C5 — unit toggle

  // FR-ALARM-07: track any new press — used later to silence the alarm only when active
  bool anyNewPress = (powerNow && !powerPressed) || (lightNow && !lightPressed) ||
                     (upNow    && !upPressed)    || (downNow  && !downPressed)  ||
                     (unitNow  && !unitPressed);

  // ---- Power (C1) ----
  if (powerNow && !powerPressed) {
    systemOn      = !systemOn;
    settingsDirty = true;
    lastSetChangeTime = now;

    if (!systemOn) {
      if (compressorOn) compressorOff();
      digitalWrite(LIGHT_PIN, LOW);
      lightOn = false;
    }
  }
  powerPressed = powerNow;

  // FR-PWR-02: suspend all processing when OFF, show "OFF"
  if (!systemOn) {
    showOFF();
    matrix.writeDisplay();
    return;
  }

  // ---- Light (C2) ----
  if (lightNow && !lightPressed) {
    lightPressStart = now;  // record press start for long-press detection
  }
  if (lightNow && lightPressed && !dutyCycleActive &&
      now - lightPressStart >= LIGHT_LONG_PRESS) {
    // FR-DISP-10: long press → show duty cycle for 10 s
    dutyCycleActive = true;
    dutyCycleUntil  = now + DUTY_CYCLE_SHOW;
  }
  if (!lightNow && lightPressed) {
    // Short release: toggle light (only if long press was not triggered)
    if (now - lightPressStart < LIGHT_LONG_PRESS) {
      lightOn = !lightOn;
      digitalWrite(LIGHT_PIN, lightOn ? HIGH : LOW);
      lightOnTime = now;
    }
  }
  lightPressed = lightNow;

  // FR-LIGHT-02: auto-off after 5 minutes
  if (lightOn && now - lightOnTime > LIGHT_TIMEOUT) {
    lightOn = false;
    digitalWrite(LIGHT_PIN, LOW);
  }

  // ---- Temp Up (C3) — FR-SET-03, FR-SET-04, FR-SET-05 ----
  if (upNow && !upPressed) {
    if (setTemp < 25) { setTemp++; settingsDirty = true; }
    upPressStart  = now;
    upLastRepeat  = now;
    lastSetChangeTime = now;
    blinkState = true;
  }

  if (upNow &&
      now - upPressStart > HOLD_DELAY &&
      now - upLastRepeat > REPEAT_RATE) {
    if (setTemp < 25) { setTemp++; settingsDirty = true; }
    upLastRepeat = now;
    lastSetChangeTime = now;
  }

  // ---- Temp Down (C4) — FR-SET-03, FR-SET-04, FR-SET-05 ----
  if (downNow && !downPressed) {
    if (setTemp > 3) { setTemp--; settingsDirty = true; }
    downPressStart = now;
    downLastRepeat = now;
    lastSetChangeTime = now;
    blinkState = true;
  }

  if (downNow &&
      now - downPressStart > HOLD_DELAY &&
      now - downLastRepeat > REPEAT_RATE) {
    if (setTemp > 3) { setTemp--; settingsDirty = true; }
    downLastRepeat = now;
    lastSetChangeTime = now;
  }

  upPressed   = upNow;
  downPressed = downNow;

  // ---- Unit toggle (C5) ----
  if (unitNow && !unitPressed) {
    displayFahrenheit = !displayFahrenheit;
  }
  unitPressed = unitNow;

  // ---------- Compressor control (FR-COMP-01 .. FR-COMP-04) ----------
  if (!sensorError &&
      currentTemp >= setTemp + 1 &&
      now - lastCompressorOffTime > ANTI_SHORT_CYCLE) {
    if (!compressorOn) {
      compressorOn = true;
      compressorOnStart = now;
      digitalWrite(COMPRESSOR_PIN, HIGH);
      digitalWrite(FAN_PIN, HIGH);
    }
  }

  if (compressorOn && currentTemp <= setTemp - 1) {
    compressorOnAccum += now - compressorOnStart;
    compressorOff();
  }

  // ---------- Alarm (FR-ALARM-01 .. FR-ALARM-05) ----------
  bool highTempAlarm = !sensorError && (currentTemp >= setTemp + 3);
  // FR-ALARM-05: suppress low-temp alarm while compressor is running (normal overshoot)
  bool lowTempAlarm  = !sensorError && (currentTemp <= setTemp - 3) && !compressorOn;

  bool alarmActive = false;
  if (highTempAlarm || lowTempAlarm) {
    if (alarmStartTime == 0) alarmStartTime = now;
    if (now - alarmStartTime > ALARM_DELAY) {
      alarmActive = true;
      if (anyNewPress) alarmSilencedUntil = now + ALARM_SILENCE_DURATION;
      if (now >= alarmSilencedUntil) beepPattern();
      else noTone(BUZZER_PIN);
    }
  } else {
    alarmStartTime = 0;
    noTone(BUZZER_PIN);
  }

  // ---------- EEPROM save (FR-EEP-02 .. FR-EEP-04) ----------
  // Only write when values have changed (dirty flag) and the user has
  // stopped adjusting for at least EEPROM_DEFERRAL ms.
  if (settingsDirty && now - lastSetChangeTime > EEPROM_DEFERRAL) {
    // EEPROM.update() skips the physical write when the stored byte
    // already matches, preserving write endurance (~100,000 cycles).
    EEPROM.update(EEPROM_SET_TEMP,     lowByte(setTemp));
    EEPROM.update(EEPROM_SET_TEMP + 1, highByte(setTemp));
    EEPROM.update(EEPROM_SYSTEM_ON,    (uint8_t)systemOn);
    settingsDirty = false;
  }

  // ---------- Display ----------
  matrix.clear();

  if (sensorError) {
    showError();
  } else {

    bool inSetWindow = (now - lastSetChangeTime < SET_WINDOW);
    bool justPressed = (now - lastSetChangeTime < STABLE_AFTER_PRESS);

    if (now - lastBlinkTime > BLINK_INTERVAL) {
      blinkState    = !blinkState;
      lastBlinkTime = now;
    }

    bool compressorDelayed = !compressorOn && (now - lastCompressorOffTime <= ANTI_SHORT_CYCLE);

    // FR-DISP-10: expire duty cycle display
    if (dutyCycleActive && now >= dutyCycleUntil) dutyCycleActive = false;

    if (dutyCycleActive) {
      // Compute duty cycle: (accumulated ON time + current ON stretch) / uptime
      unsigned long onTime = compressorOnAccum + (compressorOn ? (now - compressorOnStart) : 0);
      int pct = (int)((onTime * 100UL) / now);
      showDutyCycle(pct);
    } else if (inSetWindow) {
      // FR-DISP-03: solid for first 1 s, then blink until 3 s
      // Set window takes priority over alarm so user can see what they're changing
      int dispSet = displayFahrenheit ? (int)(setTemp * 9.0 / 5.0 + 32.5) : setTemp;
      if (justPressed || blinkState) showTemp(dispSet, compressorOn || (compressorDelayed && blinkState));
    } else if (alarmActive) {
      // Blink HI or LO to show alarm condition
      if (blinkState) {
        if (highTempAlarm) showHI();
        else               showLO();
      }
    } else {
      int dispCur = displayFahrenheit ? (int)(displayedTemp * 9.0 / 5.0 + 0.5) + 32 : (int)displayedTemp;
      showTemp(dispCur, compressorOn || (compressorDelayed && blinkState));
    }
  }

  matrix.writeDisplay();
}

// =====================================================

void compressorOff() {
  compressorOn = false;
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  lastCompressorOffTime = millis();
}

// FR-DISP-05: suppress leading zero for single-digit values
// 3-digit display layout: [tens] [units°] [C/F]
// dotFirst: blink the decimal point on the tens digit to indicate compressor delay
void showTemp(int value, bool dotFirst) {
  if (value >= 10) {
    matrix.writeDigitNum(0, value / 10, dotFirst);
  } else {
    matrix.writeDigitRaw(0, dotFirst ? 0x80 : 0x00);  // blank tens digit, optionally with dot
  }
  matrix.writeDigitNum(1, value % 10, false);
  matrix.writeDigitRaw(2, displayFahrenheit ? 0xF1 : 0xB9);  // "F" or "C" with decimal point as degree symbol
}

// FR-DISP-07
void showOFF() {
  matrix.writeDigitRaw(0, 0x3F);  // "O"
  matrix.writeDigitRaw(1, 0x71);  // "F"
  matrix.writeDigitRaw(2, 0x71);  // "F"
}

// FR-DISP-06: show "Err"
void showError() {
  matrix.writeDigitRaw(0, 0x79);  // "E"
  matrix.writeDigitRaw(1, 0x50);  // "r"
  matrix.writeDigitRaw(2, 0x50);  // "r"
}

void showHI() {
  matrix.writeDigitRaw(0, 0x76);  // "H"
  matrix.writeDigitRaw(1, 0x30);  // "I"
  matrix.writeDigitRaw(2, 0x00);  // blank
}

void showLO() {
  matrix.writeDigitRaw(0, 0x38);  // "L"
  matrix.writeDigitRaw(1, 0x3F);  // "O"
  matrix.writeDigitRaw(2, 0x00);  // blank
}

// FR-DISP-10: show duty cycle percentage, e.g. " 37" with a "d" on the last digit
// Layout: [tens] [units] [d]  — leading blank for single-digit values
void showDutyCycle(int pct) {
  if (pct > 99) pct = 99;
  if (pct < 0)  pct = 0;
  if (pct >= 10) {
    matrix.writeDigitNum(0, pct / 10);
  } else {
    matrix.writeDigitRaw(0, 0x00);  // blank tens
  }
  matrix.writeDigitNum(1, pct % 10);
  matrix.writeDigitRaw(2, 0x5E);  // "d"
}

// FR-ALARM-03: 1200 Hz beep, 120 ms, once per second
void beepPattern() {
  static unsigned long lastBeep = 0;
  unsigned long now = millis();
  if (now - lastBeep > 1000) {
    lastBeep = now;
    tone(BUZZER_PIN, 1200, 120);
  }
}

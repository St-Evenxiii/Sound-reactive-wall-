// Sound-Reactive Wall: Arduino UNO firmware
//
// Reads 7 audio bands from an MSGEQ7 (fed by a MAX4466 mic) and drives three
// LED filament channels (bass / mids / treble) through IRLZ44N MOSFETs.
// An LDR module turns the effect on only when the room is dark, with a fade.
//
// Pin map matches sound_reactive_wall.py (the PCB):
//   D3  bass PWM      D6  mids PWM      D9  treble PWM
//   A0  MSGEQ7 OUT    D4  MSGEQ7 RESET  D5  MSGEQ7 STROBE
//   A1  LDR analog    D2  LDR digital (unused by default, see USE_LDR_DIGITAL)

#include "audio_dsp.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
// Print band levels and the LDR reading over Serial (115200 baud) for tuning.
#define DEBUG_SERIAL 0

// Run the lights regardless of ambient light (handy while testing in daylight).
#define ALWAYS_ON 0

// 1 = use the LDR module's comparator output (set its threshold with the
//     module's trim pot); 0 = use the analog output with the thresholds below.
#define USE_LDR_DIGITAL 0

// Analog LDR thresholds (0..1023). Most 4-pin LDR modules read HIGHER when it
// is darker; if yours reads lower, set LDR_DARK_WHEN_HIGH to false.
// Use DEBUG_SERIAL to see what your room reads.
const uint16_t LDR_DARK_ABOVE = 700;
const uint16_t LDR_LIGHT_BELOW = 600;  // hysteresis gap prevents flicker at dusk
const bool LDR_DARK_WHEN_HIGH = true;
// The comparator output on most modules goes HIGH when dark.
const uint8_t LDR_DIGITAL_DARK_LEVEL = HIGH;

// Fade time when it gets dark / light, in seconds.
const float FADE_SECONDS = 2.0f;

// Perceived brightness (0..1) while dark but silent; 0 = fully off.
// This is before gamma correction: 0.15 is a faint glow (PWM ~4/255).
const float IDLE_GLOW = 0.15f;

// Master brightness cap (0..1), e.g. to limit current or heat.
const float MAX_BRIGHTNESS = 1.0f;

// Main loop period. One MSGEQ7 read takes ~0.6 ms, so 5 ms = 200 updates/s.
const uint16_t UPDATE_INTERVAL_MS = 5;

// ---------------------------------------------------------------------------
// Pins
// ---------------------------------------------------------------------------
const uint8_t PIN_BASS = 3;
const uint8_t PIN_MIDS = 6;
const uint8_t PIN_TREBLE = 9;
const uint8_t PIN_EQ_OUT = A0;
const uint8_t PIN_EQ_RESET = 4;
const uint8_t PIN_EQ_STROBE = 5;
const uint8_t PIN_LDR_ANALOG = A1;
const uint8_t PIN_LDR_DIGITAL = 2;

// ---------------------------------------------------------------------------
dsp::Channel bass, mids, treble;
dsp::DarkDetector darkness(LDR_DARK_ABOVE, LDR_LIGHT_BELOW, LDR_DARK_WHEN_HIGH);
dsp::Fader master(UPDATE_INTERVAL_MS / (FADE_SECONDS * 1000.0f));
uint16_t bands[7];

// Read all 7 bands from the MSGEQ7 (timing per the datasheet: strobe pulse
// >= 18 us, output settles 36 us after strobe falls, strobe period >= 72 us).
void readMsgeq7(uint16_t out[7]) {
  digitalWrite(PIN_EQ_RESET, HIGH);
  delayMicroseconds(1);  // reset pulse >= 100 ns
  digitalWrite(PIN_EQ_RESET, LOW);
  delayMicroseconds(72);  // reset-to-strobe >= 72 us
  for (uint8_t i = 0; i < 7; i++) {
    digitalWrite(PIN_EQ_STROBE, LOW);
    delayMicroseconds(36);
    out[i] = analogRead(PIN_EQ_OUT);
    digitalWrite(PIN_EQ_STROBE, HIGH);
    delayMicroseconds(36);
  }
}

bool roomIsDark() {
#if ALWAYS_ON
  return true;
#elif USE_LDR_DIGITAL
  return digitalRead(PIN_LDR_DIGITAL) == LDR_DIGITAL_DARK_LEVEL;
#else
  return darkness.update(analogRead(PIN_LDR_ANALOG));
#endif
}

void setup() {
  // Outputs off first, so the filaments never flash at power-up.
  pinMode(PIN_BASS, OUTPUT);
  pinMode(PIN_MIDS, OUTPUT);
  pinMode(PIN_TREBLE, OUTPUT);
  analogWrite(PIN_BASS, 0);
  analogWrite(PIN_MIDS, 0);
  analogWrite(PIN_TREBLE, 0);

  pinMode(PIN_EQ_RESET, OUTPUT);
  pinMode(PIN_EQ_STROBE, OUTPUT);
  digitalWrite(PIN_EQ_RESET, LOW);
  digitalWrite(PIN_EQ_STROBE, HIGH);
  pinMode(PIN_LDR_DIGITAL, INPUT);

#if DEBUG_SERIAL
  Serial.begin(115200);
  Serial.println(F("bass mids treble | b0..b6 | ldr dark master"));
#endif
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < UPDATE_INTERVAL_MS) return;
  last = now;

  readMsgeq7(bands);
  float b = bass.update(dsp::bandEnergy(bands, dsp::kBass));
  float m = mids.update(dsp::bandEnergy(bands, dsp::kMids));
  float t = treble.update(dsp::bandEnergy(bands, dsp::kTreble));

  bool dark = roomIsDark();
  float level = master.update(dark) * MAX_BRIGHTNESS;

  analogWrite(PIN_BASS, dsp::toPwm(b, level, IDLE_GLOW));
  analogWrite(PIN_MIDS, dsp::toPwm(m, level, IDLE_GLOW));
  analogWrite(PIN_TREBLE, dsp::toPwm(t, level, IDLE_GLOW));

#if DEBUG_SERIAL
  static uint8_t div = 0;
  if (++div >= 20) {  // ~10 lines per second
    div = 0;
    Serial.print(b, 2); Serial.print(' ');
    Serial.print(m, 2); Serial.print(' ');
    Serial.print(t, 2); Serial.print(F(" |"));
    for (uint8_t i = 0; i < 7; i++) { Serial.print(' '); Serial.print(bands[i]); }
    Serial.print(F(" | "));
    Serial.print(analogRead(PIN_LDR_ANALOG)); Serial.print(' ');
    Serial.print(dark); Serial.print(' ');
    Serial.println(level, 2);
  }
#endif
}

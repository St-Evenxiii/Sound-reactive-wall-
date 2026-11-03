// Host-side tests: DSP unit tests + a simulation of the full sketch.
//   g++ -std=c++11 -Wall -Wextra -I. -I../sound_reactive_wall test_firmware.cpp -o t && ./t
#include <stdio.h>
#include <string.h>
#include "Arduino.h"

// ---- Mock hardware ---------------------------------------------------------
static uint32_t g_millis = 0;
static uint32_t g_micros = 0;
static int g_pwm[20];
static int g_pin[20];
static int g_mode[20];
static uint16_t g_bands[7];     // what the simulated MSGEQ7 outputs
static int g_eq_index = -1;     // MSGEQ7 multiplexer position
static uint32_t g_strobe_low_at = 0;
static int g_ldr = 300;
static int g_failures = 0;
static int g_timing_violations = 0;
SerialMock Serial;

void pinMode(uint8_t p, uint8_t m) { g_mode[p] = m; }
void digitalWrite(uint8_t p, uint8_t v) {
  if (p == 4 && v == HIGH) g_eq_index = -1;          // RESET
  if (p == 5) {                                       // STROBE
    if (g_pin[5] == HIGH && v == LOW) {
      g_eq_index = (g_eq_index + 1) % 7;              // falling edge advances
      g_strobe_low_at = g_micros;
    }
  }
  g_pin[p] = v;
}
int digitalRead(uint8_t p) { return g_pin[p]; }
int analogRead(uint8_t p) {
  if (p == A0) {
    if (g_pin[5] != LOW || g_eq_index < 0) { g_timing_violations++; return 0; }
    if (g_micros - g_strobe_low_at < 36) g_timing_violations++;  // not settled
    return g_bands[g_eq_index];
  }
  if (p == A1) return g_ldr;
  return 0;
}
void analogWrite(uint8_t p, int v) { g_pwm[p] = v; }
void delayMicroseconds(unsigned int us) { g_micros += us; }
uint32_t millis() { return g_millis; }

#include "../sound_reactive_wall/sound_reactive_wall.ino"

// ---- Helpers ----------------------------------------------------------------
#define CHECK(cond, ...)                                              \
  do {                                                                \
    if (!(cond)) {                                                    \
      g_failures++;                                                   \
      printf("FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond);        \
      printf(__VA_ARGS__);                                            \
      printf("\n");                                                   \
    }                                                                 \
  } while (0)

static void setBands(int b0, int b1, int b2, int b3, int b4, int b5, int b6) {
  int v[7] = {b0, b1, b2, b3, b4, b5, b6};
  for (int i = 0; i < 7; i++) g_bands[i] = (uint16_t)v[i];
}
static void run(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) { g_millis++; loop(); }
}

// ---- DSP unit tests -----------------------------------------------------------
static void test_dsp() {
  using namespace dsp;
  CHECK(gammaCorrect(0) == 0.0f, "gamma(0)");
  CHECK(fabsf(gammaCorrect(1) - 1.0f) < 1e-6f, "gamma(1)");
  CHECK(gammaCorrect(0.5f) < 0.25f && gammaCorrect(0.5f) > 0.2f, "gamma(0.5)=%f", gammaCorrect(0.5f));
  CHECK(toPwm(1, 1, 0) == 255, "full = 255");
  CHECK(toPwm(0, 1, 0) == 0, "zero = 0");
  CHECK(toPwm(1, 0, 0.1f) == 0, "master 0 forces off");
  CHECK(toPwm(0, 1, 0.1f) > 0, "idle glow");

  uint16_t quiet[7] = {80, 85, 70, 60, 90, 75, 88};  // below the noise floor
  CHECK(bandEnergy(quiet, kBass) == 0 && bandEnergy(quiet, kTreble) == 0, "floor");
  uint16_t b[7] = {590, 390, 90, 90, 90, 90, 90};
  CHECK(fabsf(bandEnergy(b, kBass) - 400.0f) < 1e-3f, "bass avg=%f", bandEnergy(b, kBass));
  CHECK(bandEnergy(b, kMids) == 0, "mids 0");

  // Envelope: fast attack, slow release, auto-gain normalises loud input.
  Channel c;
  float up = c.update(800);
  CHECK(up > 0.5f, "attack in one step: %f", up);
  for (int i = 0; i < 20; i++) c.update(800);
  CHECK(c.level() > 0.99f, "loud steady -> full: %f", c.level());
  float after1 = c.update(0);
  CHECK(after1 > 0.8f, "release is slow: %f", after1);
  for (int i = 0; i < 100; i++) c.update(0);
  CHECK(c.level() < 0.01f, "decays to 0: %f", c.level());
  // Auto-gain: peak relaxes in silence, then a moderate signal reads high.
  for (int i = 0; i < 3000; i++) c.update(0);
  CHECK(c.peak() <= kMinPeak + 1, "peak decays to min: %f", c.peak());
  for (int i = 0; i < 30; i++) c.update(200);
  CHECK(c.level() > 0.95f, "auto-gain on quieter music: %f", c.level());
  // Small noise isn't amplified to full scale.
  Channel n;
  for (int i = 0; i < 50; i++) n.update(20);
  CHECK(n.level() < 0.2f, "noise stays low: %f", n.level());

  // Darkness hysteresis.
  DarkDetector d(700, 600, true);
  CHECK(!d.update(650), "start light");
  for (int i = 0; i < 500; i++) d.update(800);
  CHECK(d.isDark(), "goes dark");
  for (int i = 0; i < 500; i++) d.update(650);
  CHECK(d.isDark(), "stays dark inside hysteresis band");
  for (int i = 0; i < 500; i++) d.update(500);
  CHECK(!d.isDark(), "goes light");
  DarkDetector inv(700, 600, false);
  for (int i = 0; i < 500; i++) inv.update(100);
  CHECK(inv.isDark(), "inverted polarity");
  // A single shadow sample doesn't trigger.
  DarkDetector s(700, 600, true);
  s.update(300); s.update(1023);
  CHECK(!s.isDark(), "brief shadow ignored");

  Fader f(0.1f);
  for (int i = 0; i < 5; i++) f.update(true);
  CHECK(fabsf(f.value() - 0.5f) < 1e-4f, "fade halfway");
  for (int i = 0; i < 50; i++) f.update(true);
  CHECK(f.value() == 1.0f, "fade clamps");
}

// ---- Whole-sketch simulation -----------------------------------------------------
static void test_sketch() {
  g_pin[5] = LOW;
  setup();
  CHECK(g_mode[3] == OUTPUT && g_mode[6] == OUTPUT && g_mode[9] == OUTPUT, "outputs");
  CHECK(g_pwm[3] == 0 && g_pwm[6] == 0 && g_pwm[9] == 0, "off at boot");
  CHECK(g_pin[5] == HIGH && g_pin[4] == LOW, "MSGEQ7 idle state");

  // Daylight + loud music: lights stay off.
  g_ldr = 300;
  setBands(900, 900, 900, 900, 900, 900, 900);
  run(3000);
  CHECK(g_pwm[3] == 0 && g_pwm[6] == 0 && g_pwm[9] == 0, "off in daylight: %d %d %d", g_pwm[3], g_pwm[6], g_pwm[9]);

  // It gets dark: fades in over ~FADE_SECONDS, not instantly.
  g_ldr = 900;
  run(600);  // detector smoothing + part of the fade
  int partial = g_pwm[3];
  CHECK(partial > 0 && partial < 255, "fading in: %d", partial);
  run(3000);
  CHECK(g_pwm[3] == 255 && g_pwm[6] == 255 && g_pwm[9] == 255, "full when dark+loud: %d %d %d", g_pwm[3], g_pwm[6], g_pwm[9]);

  // Bass-only music: bass bright, treble falls back towards idle glow.
  setBands(900, 800, 90, 90, 90, 90, 90);
  run(2000);
  CHECK(g_pwm[3] > 200, "bass on: %d", g_pwm[3]);
  CHECK(g_pwm[9] < 10, "treble near idle: %d", g_pwm[9]);
  const int idle = dsp::toPwm(0, 1, IDLE_GLOW);
  CHECK(idle > 0, "IDLE_GLOW is visible: %d", idle);
  CHECK(g_pwm[9] >= idle, "idle glow keeps treble lit: %d", g_pwm[9]);

  // Treble-only.
  setBands(90, 90, 90, 90, 90, 700, 800);
  run(2000);
  CHECK(g_pwm[9] > 200 && g_pwm[3] < 10, "treble on, bass idle: %d %d", g_pwm[9], g_pwm[3]);

  // Silence in the dark: idle glow only.
  setBands(80, 80, 80, 80, 80, 80, 80);
  run(3000);
  CHECK(g_pwm[3] == dsp::toPwm(0, 1, IDLE_GLOW), "silence -> idle glow: %d", g_pwm[3]);

  // Lights come back on: fade out to fully off.
  g_ldr = 300;
  run(6000);
  CHECK(g_pwm[3] == 0 && g_pwm[6] == 0 && g_pwm[9] == 0, "off again in light");

  CHECK(g_timing_violations == 0, "MSGEQ7 read timing violations: %d", g_timing_violations);
}

int main() {
  test_dsp();
  test_sketch();
  if (g_failures) { printf("%d check(s) failed\n", g_failures); return 1; }
  printf("All firmware tests passed\n");
  return 0;
}

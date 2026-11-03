// Hardware-independent signal processing for the sound-reactive wall.
// Kept free of Arduino APIs so it can be unit-tested on a PC
// (see firmware/test/test_audio_dsp.cpp).
#pragma once

#include <math.h>
#include <stdint.h>

namespace dsp {

// ---------------------------------------------------------------------------
// Tuning
// ---------------------------------------------------------------------------
// MSGEQ7 output idles around 60-120 counts with no input; subtract this floor.
static const float kNoiseFloor = 90.0f;
// Auto-gain never amplifies a peak smaller than this (above the floor), so a
// quiet room doesn't get boosted into flicker.
static const float kMinPeak = 120.0f;
// Per-update multiplier for the auto-gain peak tracker (~200 updates/s, so
// 0.998 halves the peak in ~1.7 s of silence).
static const float kPeakDecay = 0.998f;
// Envelope smoothing: fraction of the gap closed per update.
static const float kAttack = 0.6f;    // rise quickly on a beat
static const float kRelease = 0.12f;  // fall slowly so it doesn't strobe
// Perceptual gamma for filament brightness (PWM is linear, eyes are not).
static const float kGamma = 2.2f;

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

inline float gammaCorrect(float x) {
  x = clamp01(x);
  return x <= 0.0f ? 0.0f : powf(x, kGamma);
}

// Which MSGEQ7 bands (63, 160, 400, 1k, 2.5k, 6.25k, 16k Hz) feed a channel.
struct BandRange {
  uint8_t first;
  uint8_t last;  // inclusive
};
static const BandRange kBass = {0, 1};    // 63 Hz, 160 Hz
static const BandRange kMids = {2, 4};    // 400 Hz, 1 kHz, 2.5 kHz
static const BandRange kTreble = {5, 6};  // 6.25 kHz, 16 kHz

// Average of the bands in `range`, with the noise floor removed.
inline float bandEnergy(const uint16_t bands[7], BandRange range) {
  float sum = 0.0f;
  for (uint8_t i = range.first; i <= range.last; i++) {
    float v = (float)bands[i] - kNoiseFloor;
    sum += v > 0.0f ? v : 0.0f;
  }
  return sum / (float)(range.last - range.first + 1);
}

// One output channel: auto-gain + attack/release envelope.
class Channel {
 public:
  Channel() : peak_(kMinPeak), level_(0.0f) {}

  // Feed one floor-subtracted energy sample; returns the smoothed level 0..1.
  float update(float energy) {
    peak_ *= kPeakDecay;
    if (peak_ < kMinPeak) peak_ = kMinPeak;
    if (energy > peak_) peak_ = energy;

    float target = clamp01(energy / peak_);
    float k = target > level_ ? kAttack : kRelease;
    level_ += (target - level_) * k;
    return level_;
  }

  float level() const { return level_; }
  float peak() const { return peak_; }

 private:
  float peak_;
  float level_;
};

// Darkness detector with hysteresis, so lamps don't chatter at dusk.
// `reading` is the LDR module's analog output (0..1023).
class DarkDetector {
 public:
  DarkDetector(uint16_t darkAbove, uint16_t lightBelow, bool darkWhenHigh)
      : darkAbove_(darkAbove), lightBelow_(lightBelow),
        darkWhenHigh_(darkWhenHigh), dark_(false), smoothed_(-1.0f) {}

  bool update(uint16_t reading) {
    float r = darkWhenHigh_ ? (float)reading : (float)(1023 - reading);
    // Slow low-pass: ignore passing shadows and the lamps' own flicker.
    smoothed_ = smoothed_ < 0.0f ? r : smoothed_ + (r - smoothed_) * 0.02f;
    if (!dark_ && smoothed_ > darkAbove_) dark_ = true;
    else if (dark_ && smoothed_ < lightBelow_) dark_ = false;
    return dark_;
  }

  bool isDark() const { return dark_; }

 private:
  uint16_t darkAbove_;
  uint16_t lightBelow_;
  bool darkWhenHigh_;
  bool dark_;
  float smoothed_;
};

// Linear fade of the master brightness toward on/off.
class Fader {
 public:
  explicit Fader(float stepPerUpdate) : step_(stepPerUpdate), value_(0.0f) {}
  float update(bool on) {
    value_ += on ? step_ : -step_;
    value_ = clamp01(value_);
    return value_;
  }
  float value() const { return value_; }

 private:
  float step_;
  float value_;
};

// Final 0..255 PWM duty for a channel.
inline uint8_t toPwm(float level, float master, float idle) {
  float x = idle + (1.0f - idle) * clamp01(level);
  return (uint8_t)(gammaCorrect(x) * clamp01(master) * 255.0f + 0.5f);
}

}  // namespace dsp

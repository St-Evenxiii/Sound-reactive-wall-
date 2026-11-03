# Sound-reactive-wall-
Music influenced led wall sculpture

An Arduino UNO listens through a MAX4466 microphone and an MSGEQ7 7-band
spectrum analyzer, then PWM-drives three LED filament channels (bass / mids /
treble) through IRLZ44N MOSFETs. An LDR module enables the effect only when
the room is dark.

## Hardware

The carrier PCB is described in code with [SKiDL](https://github.com/devbisme/skidl):

| File | What |
|---|---|
| `sound_reactive_wall.py` | Circuit source (edit this) |
| `sound_reactive_wall.kicad_sch` | Generated schematic, opens in KiCad 7+ |
| `sound_reactive_wall.net` | Generated netlist, import into Pcbnew for layout |
| `tests/test_circuit.py` | Connectivity tests |

### Pin map

| Arduino | Function |
|---|---|
| D3 / D6 / D9 | Bass / mids / treble PWM → 220 Ω → MOSFET gate (10 k pull-down) |
| A0 | MSGEQ7 OUT |
| D4 | MSGEQ7 RESET |
| D5 | MSGEQ7 STROBE |
| A1 | LDR analog out |
| D2 | LDR digital (comparator) out |

### Power

* J1 takes a **regulated 5 V** supply, through fuse F1 (5 A, 5×20 mm; size it
  to your filament load) and a 1000 µF bulk capacitor that absorbs PWM current
  spikes so they don't disturb the audio readings.
* The 5 V rail back-feeds the UNO's 5V pin, bypassing its regulator. **Don't
  feed more than 5.5 V into J1**, and preferably unplug the external supply
  while the UNO is on USB.
* Each filament output terminal is `+5V` (pin 1) / MOSFET drain (pin 2).

### Audio

J7 carries the MAX4466 output plus ground; wire it to the MSGEQ7 module's
audio input.

## Firmware

`firmware/sound_reactive_wall/` is the Arduino UNO sketch. Open
`sound_reactive_wall.ino` in the Arduino IDE (it needs no libraries) and upload.

* **Bands:** the MSGEQ7's 7 bands are grouped into bass (63, 160 Hz), mids
  (400 Hz, 1 kHz, 2.5 kHz) and treble (6.25, 16 kHz).
* **Auto-gain:** each channel tracks its recent peak, so the wall reacts the
  same to quiet and loud music; a noise floor keeps silence dark.
* **Smoothing:** fast attack, slow release, plus gamma correction so the
  filament brightness looks even.
* **Darkness:** the LDR enables the effect with hysteresis and a 2 s fade.

Settings at the top of the sketch:

| Setting | Default | Purpose |
|---|---|---|
| `DEBUG_SERIAL` | 0 | Print levels + LDR reading at 115200 baud for tuning |
| `ALWAYS_ON` | 0 | Ignore the LDR (testing in daylight) |
| `USE_LDR_DIGITAL` | 0 | Use the module's comparator (trim pot) instead of `LDR_DARK_ABOVE`/`LDR_LIGHT_BELOW` |
| `LDR_DARK_WHEN_HIGH` | true | Flip if your module reads *lower* in the dark |
| `IDLE_GLOW` | 0.15 | Faint glow while dark but silent (0 = off) |
| `MAX_BRIGHTNESS` | 1.0 | Overall brightness cap |

Further tuning (noise floor, attack/release, gamma) is in `audio_dsp.h`.
If the lights flicker in a silent room, raise `kNoiseFloor`.

### Bringing it up

1. Set `DEBUG_SERIAL 1` and `ALWAYS_ON 1`, upload, and open the Serial Monitor.
2. In silence, note the band values `b0..b6`; set `kNoiseFloor` just above them.
3. Note the LDR value with the room lit and dark; set `LDR_DARK_ABOVE` /
   `LDR_LIGHT_BELOW` between them (or flip `LDR_DARK_WHEN_HIGH`).
4. Set both flags back to 0 and upload.

## Regenerating / testing

```sh
pip install --no-deps skidl==3.0.0
pip install -r requirements.txt
export KICAD_SYMBOL_DIR=/usr/share/kicad/symbols   # wherever KiCad's symbols live
pytest -q                       # connectivity tests
python sound_reactive_wall.py   # ERC, netlist, schematic

# Firmware logic, simulated on a PC with a mock Arduino API
cd firmware/test
g++ -std=c++11 -Wall -Wextra -I. -I../sound_reactive_wall test_firmware.cpp -o test_firmware && ./test_firmware
```

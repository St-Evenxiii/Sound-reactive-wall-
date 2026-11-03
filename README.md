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
  The board has no current limiting on these outputs: put each filament
  string's series resistor in the wiring off the board, sized for the
  filament's rated voltage and current.

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

## Breadboard bring-up

Every part is through-hole or a plug-in module, so the circuit can be proven
on a breadboard before the PCB is made. Start with ordinary LEDs on USB power,
then move to the filaments.

### Parts

* Arduino UNO, MSGEQ7 module, MAX4466 mic module, 4-pin LDR module
* 3× IRLZ44N, 3× 220 Ω, 3× 10 kΩ
* 100 nF ceramic and 100–1000 µF electrolytic capacitor
* For the first test: 3 ordinary LEDs and 3× 220 Ω resistors

Leave out the screw terminals (wrong pitch; use jumpers) and the fuse (USB is
current-limited).

### Wiring

Same nets as the PCB:

| UNO pin | Connects to |
|---|---|
| 5V | + rail: MSGEQ7 5V, mic VCC, LDR VCC |
| GND | − rail: all module GNDs, the three MOSFET sources |
| D3 / D6 / D9 | 220 Ω → bass / mids / treble MOSFET gate; 10 kΩ from each gate to GND |
| A0 | MSGEQ7 OUT |
| D4 | MSGEQ7 RESET |
| D5 | MSGEQ7 STROBE |
| A1 | LDR AO |
| D2 | LDR DO |
| — | Mic OUT + GND → MSGEQ7 module's audio input |

* IRLZ44N legs, printed side facing you: **gate, drain, source**.
* 100 nF across the rails next to the MSGEQ7; the electrolytic across the
  rails anywhere (stripe to GND).
* Test load for each channel: + rail → 220 Ω → LED → MOSFET drain (~15 mA).

> **Bare MSGEQ7 chip instead of a module?** It needs a clock resistor and
> capacitor, an input coupling capacitor and resistor, and a capacitor on its
> reference pin. That pin is labelled GND but sits at about 2.5 V, so don't
> wire it to ground. Take the values from the MSGEQ7 datasheet.

### Steps

1. Set `ALWAYS_ON 1` and `DEBUG_SERIAL 1` at the top of the sketch, upload,
   and open the Serial Monitor at 115200 baud.
2. **Noise floor:** in silence, note the band values `b0..b6`, and set
   `kNoiseFloor` in `audio_dsp.h` just above them.
3. **Audio path:** clap or play music; the band values should jump. If they
   stay at 0 or at 1023, check the RESET/STROBE wiring and the audio jumper.
4. **Channels:** play test tones from a phone tone generator. 100 Hz should
   light mainly bass, 1 kHz mainly mids, 10 kHz mainly treble (the MSGEQ7's
   bands are wide, so a little spill into the neighbour is normal).
5. **LDR:** note the LDR reading with the room lit and with the sensor
   covered. Set `LDR_DARK_ABOVE` / `LDR_LIGHT_BELOW` between them, or flip
   `LDR_DARK_WHEN_HIGH` if it reads lower in the dark.
6. Set `ALWAYS_ON 0`, upload, and check that the LEDs fade in when you cover
   the LDR and fade out when you uncover it.
7. Set `DEBUG_SERIAL 0` for the final upload.

### Moving to the filaments

* Size each filament's series resistor (off the board) before connecting it.
* **Switch to the external 5 V supply and unplug USB.** USB can't supply the
  filaments, and the external supply feeds the UNO's 5V pin directly, so the
  two would fight if both were connected.
* **Breadboard contacts are rated around 1 A.** Run the filament + wires and
  the MOSFET source-to-GND returns directly to the supply, not through the
  breadboard; only the gate and signal wiring should stay on it.
* If a MOSFET runs hot, check the gate is reaching 5 V and the source return
  is solid; at a few amps per channel the IRLZ44N should stay cool without a
  heatsink.

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

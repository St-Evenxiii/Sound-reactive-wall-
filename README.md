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

## Regenerating / testing

```sh
pip install --no-deps skidl==3.0.0
pip install -r requirements.txt
export KICAD_SYMBOL_DIR=/usr/share/kicad/symbols   # wherever KiCad's symbols live
pytest -q                       # connectivity tests
python sound_reactive_wall.py   # ERC, netlist, schematic
```

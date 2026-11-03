"""Connectivity tests for the Sound-Reactive Wall PCB.

Run with:  KICAD_SYMBOL_DIR=/path/to/kicad/symbols pytest
"""

import builtins
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sound_reactive_wall as srw  # noqa: E402


def net_of(pin):
    nets = pin.nets
    assert len(nets) == 1, f"{pin} is on {len(nets)} nets"
    return nets[0]


def gnd_pins(part):
    return [p for p in part.pins if p.name == "GND"]


@pytest.fixture(scope="module")
def board():
    return srw.build()


def test_erc_clean(board):
    from skidl import ERC, erc_logger
    ERC()
    assert erc_logger.error.count == 0
    assert erc_logger.warning.count == 0


def test_power_rails(board):
    gnd = net_of(gnd_pins(board["uno"])[0])
    vcc5 = net_of(board["uno"]["+5V"])
    assert gnd.name == "GND" and vcc5.name == "+5V"
    for part in ("msgeq7", "mic", "ldr"):
        assert net_of(board[part]["GND"]) is gnd, part
    for pin in (board["msgeq7"]["5V"], board["mic"]["VCC"], board["ldr"]["VCC"]):
        assert net_of(pin) is vcc5
    # Every Arduino GND pin is tied together.
    assert len(gnd_pins(board["uno"])) >= 3
    for p in gnd_pins(board["uno"]):
        assert net_of(p) is gnd


def test_supply_is_fused(board):
    fuse, terminal = board["fuse"], board["power_in"]
    assert net_of(terminal[1]) is net_of(fuse[1])
    assert net_of(fuse[2]).name == "+5V"
    assert net_of(terminal[1]).name != "+5V", "supply bypasses the fuse"
    assert net_of(terminal[2]).name == "GND"


@pytest.mark.parametrize("ch,gpio", [("BASS", "D3"), ("MIDS", "D6"), ("TREBLE", "D9")])
def test_led_channel(board, ch, gpio):
    fet, uno = board[ch], board["uno"]
    gate = net_of(fet["G"])
    pwm = net_of(uno[gpio])
    # GPIO goes through a series resistor (not straight to the gate).
    assert gate is not pwm
    gate_parts = {p.part for p in gate.pins}
    pwm_parts = {p.part for p in pwm.pins}
    series = (gate_parts & pwm_parts) - {fet, uno}
    assert len(series) == 1 and next(iter(series)).value == "220"
    # Gate has a 10k pull-down to GND.
    pulldowns = [p.part for p in gate.pins
                 if p.part.name == "R" and p.part.value == "10k"]
    assert len(pulldowns) == 1
    assert any(net_of(p).name == "GND" for p in pulldowns[0].pins)
    # Low-side switch: source to GND, drain to the LED terminal.
    assert net_of(fet["S"]).name == "GND"
    drain_parts = {p.part.name for p in net_of(fet["D"]).pins}
    assert "Screw_Terminal_01x02" in drain_parts


def test_pwm_pins_are_pwm_capable(board):
    # Each FET must be driven from a pin that supports analogWrite() on the UNO.
    pwm_capable = {"D3", "D5", "D6", "D9", "D10", "D11"}
    uno = board["uno"]
    for ch in ("BASS", "MIDS", "TREBLE"):
        gate = net_of(board[ch]["G"])
        series = [p.part for p in gate.pins if p.part.name == "R" and p.part.value == "220"][0]
        drivers = {p.name for pin in series.pins for p in net_of(pin).pins if p.part is uno}
        assert len(drivers) == 1 and drivers <= pwm_capable, (ch, drivers)


def test_msgeq7_signals(board):
    uno, eq = board["uno"], board["msgeq7"]
    assert net_of(uno["A0"]) is net_of(eq["OUT"])
    assert net_of(uno["D4"]) is net_of(eq["RST"])
    assert net_of(uno["D5"]) is net_of(eq["STR"])


def test_mic_audio_header_has_ground(board):
    hdr, mic = board["audio_out"], board["mic"]
    assert net_of(hdr[1]) is net_of(mic["OUT"])
    assert net_of(hdr[2]).name == "GND"


def test_ldr(board):
    uno, ldr = board["uno"], board["ldr"]
    assert net_of(uno["A1"]) is net_of(ldr["AO"])
    assert net_of(uno["D2"]) is net_of(ldr["DO"])
    # Must be 5 V so DO's high level clears the ATmega's 0.6*VCC V_IH.
    assert net_of(ldr["VCC"]).name == "+5V"


def test_all_footprints_assigned(board):
    for part in builtins.default_circuit.parts:
        assert part.footprint and ":" in part.footprint, part.ref

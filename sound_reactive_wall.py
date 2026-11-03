"""SKiDL description of the Sound-Reactive Wall carrier PCB.

An Arduino UNO reads an MSGEQ7 spectrum-analyzer module (fed from a MAX4466
microphone) and an LDR "dark" sensor, then PWM-drives three LED filament
channels (bass / mids / treble) through logic-level IRLZ44N low-side MOSFETs.

Usage:
    python sound_reactive_wall.py            # ERC + netlist + schematic

KiCad symbol libraries are located through, in order:
    1. the KICAD_SYMBOL_DIR environment variable (or KICAD<N>_SYMBOL_DIR),
    2. the common install locations listed in SYMBOL_DIR_CANDIDATES.
"""

import builtins
import os

from skidl import (
    ERC,
    KICAD8,
    POWER,
    Net,
    Part,
    generate_netlist,
    generate_schematic,
    lib_search_paths,
    reset,
    set_default_tool,
)

SYMBOL_DIR_CANDIDATES = [
    "/usr/share/kicad/symbols",
    "/usr/local/share/kicad/symbols",
    "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols",
    r"C:\Program Files\KiCad\9.0\share\kicad\symbols",
    r"C:\Program Files\KiCad\8.0\share\kicad\symbols",
]

# Footprints
FP_RES = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"
FP_CAP = "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm"
FP_CAP_BULK = "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm"
FP_FET = "Package_TO_SOT_THT:TO-220-3_Vertical"
FP_SCREW2 = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal"
FP_FUSE = "Fuse:Fuseholder_Clip-5x20mm_Keystone_3517_Inline_P23.11x6.76mm_D1.70mm_Horizontal"


def setup_library_paths():
    """Register the KiCad symbol directory for every KiCad tool SKiDL knows.

    SKiDL's ``KICAD`` alias points at a different KiCad version depending on
    the SKiDL release, so appending to only that key (as the original script
    did) silently breaks on newer SKiDL versions.
    """
    dirs = [v for k, v in os.environ.items() if k.startswith("KICAD") and k.endswith("SYMBOL_DIR")]
    dirs += [d for d in SYMBOL_DIR_CANDIDATES if os.path.isdir(d)]
    for tool, paths in lib_search_paths.items():
        if tool.startswith("kicad"):
            for d in dirs:
                if d not in paths:
                    paths.append(d)


def socket(n, *names):
    """Pin-socket header for a plug-in module, with named pin aliases."""
    conn = Part(
        "Connector",
        f"Conn_01x{n:02d}_Socket",
        footprint=f"Connector_PinSocket_2.54mm:PinSocket_1x{n:02d}_P2.54mm_Vertical",
    )
    for i, name in enumerate(names, start=1):
        conn[i].aliases += name
    return conn


def led_channel(name, gpio, vcc, gnd):
    """One low-side PWM channel: GPIO -> 220R -> gate (10k pull-down) -> IRLZ44N.

    The series gate resistor limits the inrush current from the AVR pin into
    the ~3 nF gate capacitance and damps ringing; the pull-down keeps the
    filament off while the Arduino is resetting/booting.
    """
    gate = Net(f"{name}_GATE")
    drain = Net(f"{name}_LED-")
    pwm = Net(f"{name}_PWM")

    fet = Part("Transistor_FET", "IRLZ44N", footprint=FP_FET)
    r_gate = Part("Device", "R", value="220", footprint=FP_RES)
    r_pd = Part("Device", "R", value="10k", footprint=FP_RES)
    out = Part("Connector", "Screw_Terminal_01x02", footprint=FP_SCREW2)

    pwm += gpio, r_gate[1]
    gate += r_gate[2], r_pd[1], fet["G"]
    gnd += r_pd[2], fet["S"]
    drain += fet["D"], out[2]
    vcc += out[1]
    return fet


def build():
    """Create the circuit in SKiDL's default circuit and return a dict of key parts."""
    reset()
    # Pin the tool so output is stable across SKiDL releases; the KICAD8
    # backend writes a schematic that opens in KiCad 7 and newer.
    set_default_tool(KICAD8)
    setup_library_paths()

    gnd = Net("GND")
    vcc5 = Net("+5V")
    vin = Net("VIN_UNFUSED")
    # The screw terminal is passive; tell ERC these nets are driven.
    gnd.drive = POWER
    vcc5.drive = POWER

    # --- MCU ---
    uno = Part("MCU_Module", "Arduino_UNO_R3", footprint="Module:Arduino_UNO_R3")

    # --- Power input: 5 V regulated supply -> fuse -> +5V rail ---
    power_in = Part("Connector", "Screw_Terminal_01x02", footprint=FP_SCREW2)
    fuse = Part("Device", "Fuse", value="5A", footprint=FP_FUSE)
    c_bulk = Part("Device", "C_Polarized", value="1000uF 10V", footprint=FP_CAP_BULK)
    c_dec = Part("Device", "C", value="100nF", footprint=FP_CAP)

    vin += power_in[1], fuse[1]
    vcc5 += fuse[2], c_bulk[1], c_dec[1]
    gnd += power_in[2], c_bulk[2], c_dec[2]

    # Arduino power: the regulated 5 V rail back-feeds the UNO's 5V pin.
    vcc5 += uno["+5V"]
    for p in uno.pins:
        if p.name == "GND":
            gnd += p

    # --- LED filament channels (D3/D6/D9 = three different PWM timers) ---
    channels = {
        "BASS": led_channel("BASS", uno["D3"], vcc5, gnd),
        "MIDS": led_channel("MIDS", uno["D6"], vcc5, gnd),
        "TREBLE": led_channel("TREBLE", uno["D9"], vcc5, gnd),
    }

    # --- MSGEQ7 spectrum module ---
    msgeq7 = socket(5, "5V", "RST", "STR", "OUT", "GND")
    c_audio = Part("Device", "C", value="100nF", footprint=FP_CAP)  # local decoupling
    vcc5 += msgeq7["5V"], c_audio[1]
    gnd += msgeq7["GND"], c_audio[2]
    Net("EQ_OUT").connect(uno["A0"], msgeq7["OUT"])
    Net("EQ_RST").connect(uno["D4"], msgeq7["RST"])
    Net("EQ_STROBE").connect(uno["D5"], msgeq7["STR"])

    # --- MAX4466 microphone module ---
    mic = socket(3, "VCC", "GND", "OUT")
    vcc5 += mic["VCC"]
    gnd += mic["GND"]
    # Signal + ground header for the lead to the MSGEQ7 module's audio input.
    # (Previously a single pin with no ground reference.)
    audio_out = Part(
        "Connector", "Conn_01x02_Pin",
        footprint="Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
    )
    Net("MIC_AUDIO").connect(mic["OUT"], audio_out[1])
    gnd += audio_out[2]

    # --- LDR darkness sensor module ---
    # Powered from 5 V: with 3.3 V the comparator's DO high level (~3.3 V) sits
    # right at the ATmega328P's 3.0 V V_IH threshold, so D2 reads unreliably.
    ldr = socket(4, "VCC", "GND", "DO", "AO")
    vcc5 += ldr["VCC"]
    gnd += ldr["GND"]
    Net("LDR_ANALOG").connect(uno["A1"], ldr["AO"])
    Net("LDR_DIGITAL").connect(uno["D2"], ldr["DO"])

    # Mark every unused Arduino pin as intentionally unconnected.
    for p in uno.pins:
        if not p.is_connected():
            p += builtins.NC  # SKiDL rebinds this on reset()

    return {
        "uno": uno, "fuse": fuse, "power_in": power_in, "msgeq7": msgeq7,
        "mic": mic, "ldr": ldr, "audio_out": audio_out, **channels,
    }


def main():
    build()
    ERC()
    generate_netlist(file_="sound_reactive_wall.net")
    print("Netlist written to sound_reactive_wall.net")
    # Draw every net as a named label rather than a wire: SKiDL's auto-router
    # fails (RoutingFailure) on this board's large GND/+5V nets, and labels
    # are easier to read anyway.
    for net in builtins.default_circuit.get_nets():
        net.stub = True
    try:
        generate_schematic(top_name="sound_reactive_wall", title="Sound-Reactive Wall")
        print("Schematic written to sound_reactive_wall.kicad_sch")
    except Exception as e:  # schematic placement/routing is best-effort
        print(f"Schematic generation failed ({type(e).__name__}: {e}); netlist is still valid.")


if __name__ == "__main__":
    main()

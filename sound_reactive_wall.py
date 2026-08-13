from skidl import *

# Set up library path for KiCad
lib_search_paths[KICAD].append('/usr/share/kicad/symbols')

# Create nets
gnd = Net('GND')
vcc5 = Net('+5V')
vcc33 = Net('+3.3V')

# --- Instantiate Components ---

# MCU: Arduino Uno R3 footprint shield
uno = Part('MCU_Module', 'Arduino_UNO_R3', footprint='Module:Arduino_UNO_R3')

# MOSFETs: 3x IRLZ44N (TO-220)
mosfet_bass = Part('Transistor_FET', 'IRLZ44N', footprint='Package_TO_SOT_THT:TO-220-3_Vertical')
mosfet_mids = Part('Transistor_FET', 'IRLZ44N', footprint='Package_TO_SOT_THT:TO-220-3_Vertical')
mosfet_treble = Part('Transistor_FET', 'IRLZ44N', footprint='Package_TO_SOT_THT:TO-220-3_Vertical')

# Pull-down Resistors for MOSFETs: 3x 10k (THT)
r_bass = Part('Device', 'R', value='10k', footprint='Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal')
r_mids = Part('Device', 'R', value='10k', footprint='Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal')
r_treble = Part('Device', 'R', value='10k', footprint='Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal')

# Decoupling Capacitor: 100nF
c_dec = Part('Device', 'C', value='100nF', footprint='Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm')

# Main Power Input (Screw Terminal)
power_in = Part('Connector', 'Screw_Terminal_01x02', footprint='TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal')

# Filament Outputs (Screw Terminals)
out_bass = Part('Connector', 'Screw_Terminal_01x02', footprint='TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal')
out_mids = Part('Connector', 'Screw_Terminal_01x02', footprint='TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal')
out_treble = Part('Connector', 'Screw_Terminal_01x02', footprint='TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal')

# MSGEQ7 Module Connector (5 pins: 5V, RST, STR, OUT, GND)
msgeq7_conn = Part('Connector', 'Conn_01x05_Socket', footprint='Connector_PinSocket_2.54mm:PinSocket_1x05_P2.54mm_Vertical')
msgeq7_conn[1].aliases += '5V'
msgeq7_conn[2].aliases += 'RST'
msgeq7_conn[3].aliases += 'STR'
msgeq7_conn[4].aliases += 'OUT'
msgeq7_conn[5].aliases += 'GND'

# MAX4466 Microphone Connector (3 pins: VCC, GND, OUT)
max4466_conn = Part('Connector', 'Conn_01x03_Socket', footprint='Connector_PinSocket_2.54mm:PinSocket_1x03_P2.54mm_Vertical')
max4466_conn[1].aliases += 'VCC'
max4466_conn[2].aliases += 'GND'
max4466_conn[3].aliases += 'OUT'

# Test point / Header for MAX4466 Audio OUT (to connect to MSGEQ7 Audio Jack)
audio_out_tp = Part('Connector', 'Conn_01x01_Pin', footprint='Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical')

# 4-Pin LDR Module Connector (4 pins: VCC, GND, DO, AO)
ldr_conn = Part('Connector', 'Conn_01x04_Socket', footprint='Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical')
ldr_conn[1].aliases += 'VCC'
ldr_conn[2].aliases += 'GND'
ldr_conn[3].aliases += 'DO'
ldr_conn[4].aliases += 'AO'

# --- Wiring ---

# Find specific pins on Arduino for power manually
# The Arduino Uno R3 part has these names exactly
uno_5v = None
uno_3v3 = None
uno_gnds = []

for p in uno.pins:
    if p.name == '+5V':
        uno_5v = p
    elif p.name == '3V3':
        uno_3v3 = p
    elif p.name == 'GND':
        uno_gnds.append(p)

# Power Supply
vcc5 += power_in[1], uno_5v, msgeq7_conn['5V'], max4466_conn['VCC'], out_bass[1], out_mids[1], out_treble[1]
for g in uno_gnds:
    gnd += g
gnd += power_in[2], msgeq7_conn['GND'], max4466_conn['GND'], ldr_conn['GND'], mosfet_bass['S'], mosfet_mids['S'], mosfet_treble['S'], c_dec[2], r_bass[2], r_mids[2], r_treble[2]
vcc33 += uno_3v3, ldr_conn['VCC']

# Decoupling cap near audio
vcc5 += c_dec[1]

# MOSFET Gate Pull-downs and Control
r_bass[1] += mosfet_bass['G']
r_mids[1] += mosfet_mids['G']
r_treble[1] += mosfet_treble['G']

uno['D3'] += mosfet_bass['G']
uno['D6'] += mosfet_mids['G']
uno['D9'] += mosfet_treble['G']

# MOSFET Drain to LED Negative (Screw Terminal Pin 2)
mosfet_bass['D'] += out_bass[2]
mosfet_mids['D'] += out_mids[2]
mosfet_treble['D'] += out_treble[2]

# MSGEQ7 Connections
uno['A0'] += msgeq7_conn['OUT']
uno['D4'] += msgeq7_conn['RST']
uno['D5'] += msgeq7_conn['STR']

# LDR Connections
uno['A1'] += ldr_conn['AO']
uno['D2'] += ldr_conn['DO']

# MAX4466 Output to Test Point
max4466_conn['OUT'] += audio_out_tp[1]

print("Generating schematic...")
try:
    generate_schematic(file_='sound_reactive_wall.kicad_sch')
    print("Schematic generated successfully!")
except Exception as e:
    print(f"Schematic auto-routing failed ({e}), generating a netlist instead as a fallback...")

print("Generating netlist...")
generate_netlist(file_='sound_reactive_wall.net')
print("Netlist generated successfully!")

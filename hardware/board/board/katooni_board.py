"""Katooni main board (KATOO-1): an ESP32-S3-MINI-1 cable tester. Two clusters of test ports along
the front edge (A end: USB-C, USB-A 3.0, RJ45; B end: USB-C, micro-B, RJ45); every conductor of
every port runs through a series resistor to its own MCP23017 pin, so the firmware can drive one
line and read the rest. See DESIGN.md for the parts, the circuit and the reasoning.

Board frame (KiCad, y down): x 0..W, y 0..H. The module's antenna sits on the y = 0 edge (the
back), the test ports open across the y = H edge (the front).
"""
from board.netlist import Circuit

W, H = 100.0, 70.0

# GPIO choice. USB (19/20) and UART0 (43/44) are fixed by the silicon; GPIO0 is the BOOT button.
# The rest avoid the strapping pins (3, 45, 46), the flash pins (26-32) and 33-37 (octal-flash
# pins on other variants), as tinycrypt does.
GPIO = {"BOOT": 0, "SDA0": 1, "SCL0": 2, "SDA1": 4, "SCL1": 5, "FUSB_INT": 6, "LCD_BL": 7,
        "LCD_RST": 8, "LCD_DC": 9, "LCD_CS": 10, "LCD_MOSI": 11, "LCD_SCK": 12, "BUZZER": 13,
        "LOAD_EN": 14, "AGND_EN": 15, "BSHORT_EN": 16, "EXP_RST": 17, "USB_DM": 19, "USB_DP": 20,
        "UART_TX": 43, "UART_RX": 44}
_MODPIN = {43: "TXD0", 44: "RXD0"}

# I2C addresses (7-bit). Bus 0 holds the five expanders; bus 1 the FUSB302 (fixed at 0x22, which
# is inside the MCP23017 range, hence two buses) and the three INA226s.
EXP_ADDR = {"U10": 0x20, "U11": 0x21, "U12": 0x22, "U13": 0x23, "U14": 0x24}
INA_ADDR = {"U5": 0x40, "U6": 0x41, "U7": 0x44}     # current, VBUS drop, GND drop
FUSB_ADDR = 0x22

# USB-C receptacle pads, by function (the cable's view): CC, D+/D- on both rows (a plug only
# touches one row's pair, so the pair it lands on tells the orientation), SBU, the four SuperSpeed
# pairs. VBUS and GND are split by row: the A row carries the test current (force) and the B row
# senses (Kelvin); the plug's own contacts join them.
USBC_SIGNALS = {"CC1": "A5", "CC2": "B5", "DP1": "A6", "DN1": "A7", "DP2": "B6", "DN2": "B7",
                "SBU1": "A8", "SBU2": "B8", "TX1P": "A2", "TX1N": "A3", "RX1P": "B11", "RX1N": "B10",
                "TX2P": "B2", "TX2N": "B3", "RX2P": "A11", "RX2N": "A10"}
USBC_FORCE = {"VBUS": ("A4", "A9"), "GND": ("A1", "A12")}
USBC_SENSE = {"VBUS": ("B4", "B9"), "GND": ("B1", "B12")}
USBA_SIGNALS = {"DN": "2", "DP": "3", "SSRXN": "5", "SSRXP": "6", "GD": "7", "SSTXN": "8", "SSTXP": "9"}
MICROB_SIGNALS = {"DN": "D-", "DP": "D+", "ID": "ID"}

# Which expander pin each test line lands on: (expander ref, pin name) per line net. Lines are
# grouped so that each expander serves the ports nearest it.
def _lines():
    a1 = [f"A_E{n}" for n in range(1, 9)] + ["A_E_SH"] + [f"A_A_{k}" for k in USBA_SIGNALS]
    a2 = [f"A_C_{k}" for k in USBC_SIGNALS]
    a3 = ["A_VBUS", "A_GND", "A_C_SH", "A_A_SH"]
    b1 = [f"B_C_{k}" for k in USBC_SIGNALS]
    b2 = ["B_VBUS", "B_GND", "B_C_SH"] + [f"B_M_{k}" for k in MICROB_SIGNALS] + ["B_M_SH"] + \
         [f"B_E{n}" for n in range(1, 9)] + ["B_E_SH"]
    pins = [f"GPA{n}" for n in range(8)] + [f"GPB{n}" for n in range(8)]
    out = {}
    for ref, nets in (("U10", a1), ("U11", a2), ("U12", a3), ("U13", b1), ("U14", b2)):
        assert len(nets) <= 16, ref
        for net, pin in zip(nets, pins):
            out[net] = (ref, pin)
    return out


LINES = _lines()


def series_ref(net):
    """R100-R115 for U10's lines, R120-R135 for U11's, ... in pin order (GPA0-7, GPB0-7)."""
    ref, pin = LINES[net]
    slot = int(pin[3]) + (8 if pin[2] == "B" else 0)
    return f"R{100 + 20 * (int(ref[1:]) - 10) + slot}"


TIE_REFS = {"A_VBUS": "R13", "A_GND": "R14", "B_VBUS": "R15", "B_GND": "R16"}
# Series resistor per line: 1k, except the CC lines. The A end's CC lines see VCONN (5 V) from the
# FUSB302, so 10k keeps the expander's clamp current to ~0.1 mA. The B end's are 5k1, so that
# driving one low presents a USB-C sink's Rd and the cable sees a partner.
SERIES = {net: "R1K" for net in LINES}
SERIES.update({"A_C_CC1": "R10K", "A_C_CC2": "R10K", "B_C_CC1": "R5K1", "B_C_CC2": "R5K1"})


def modpin(gpio):
    return _MODPIN.get(gpio, f"IO{gpio}")


J1_PINS = {"1": "VBUS_RAW", "2": "USB_DM", "3": "USB_DP", "4": "CC", "5": "GND"}
DEBUG_PINS = {"1": "EN", "2": "UART_RX", "3": "UART_TX", "4": "GND", "5": "+3V3"}
DEBUG_LABELS = {"1": "EN", "2": "RX", "3": "TX", "4": "G", "5": "3V3"}
# Panel FPC (HS20HS072RX, 12 pins at 0.5 mm): 1 GND, 2 CS, 3 RS (D/C), 4 SCL, 5 SDA, 6 RST, 7 NC,
# 8 IOVCC, 9 VCC, 10 A (backlight anode), 11 K (cathode), 12 GND.
FPC_PINS = {"1": "GND", "2": "LCD_CS", "3": "LCD_DC", "4": "LCD_SCK", "5": "LCD_MOSI", "6": "LCD_RST",
            "8": "+3V3", "9": "+3V3", "10": "LED_A", "11": "LED_K", "12": "GND"}

POWER_NETS = ("VBUS_RAW", "VBUS", "+3V3", "GND", "LOAD_HI", "LOAD_SH", "A_VBUS", "A_GND", "B_VBUS",
              "B_GND", "B_MID")


def _cap(c, key, net, ref, gnd="GND"):
    cap = c.add(key, ref)
    c.connect(net, (cap, "1"))
    c.connect(gnd, (cap, "2"))
    return cap


def _res(c, key, a, b, ref):
    r = c.add(key, ref)
    c.connect(a, (r, "1"))
    c.connect(b, (r, "2"))
    return r


def build_katooni():
    c = Circuit()
    gnd, v33, vbus = "GND", "+3V3", "VBUS"
    u1 = c.add("ESP32", "U1")

    # --- Power in: the house JST-SH connector (J1), PTC, TVS, ESD, 3.3 V LDO.
    j1 = c.add("JST5", "J1")
    for pad, net in J1_PINS.items():
        c.connect(net, (j1, pad))
    c.connect(gnd, (j1, "6"), (j1, "7"))
    _res(c, "R5K1", "CC", gnd, "R1")                                 # Rd: a 5 V sink, no PD
    f1 = c.add("PTC", "F1")
    c.connect("VBUS_RAW", (f1, "1"))
    c.connect(vbus, (f1, "2"))
    d1 = c.add("TVS", "D1")                                      # clamps surges; reverse supply trips F1
    c.connect(vbus, (d1, "K"))
    c.connect(gnd, (d1, "A"))
    u3 = c.add("ESD", "U3")
    c.connect("USB_DM", (u3, "1"), (u3, "6"), (u1, modpin(GPIO["USB_DM"])))
    c.connect("USB_DP", (u3, "3"), (u3, "4"), (u1, modpin(GPIO["USB_DP"])))
    c.connect(vbus, (u3, "5"))
    c.connect(gnd, (u3, "2"))
    u2 = c.add("LDO", "U2")
    c.connect(vbus, (u2, "VIN"), (u2, "EN"))
    c.connect(gnd, (u2, "GND"))
    c.connect(v33, (u2, "VOUT"))
    c.no_connect(u2, "NC")
    _cap(c, "C10U", vbus, "C1")
    _cap(c, "C10U", v33, "C2")
    _cap(c, "C100N", v33, "C3")

    # --- Module: decoupling, EN RC, BOOT button, debug pads.
    c.connect(v33, (u1, "3V3"))
    c.connect(gnd, (u1, "GND"))
    _cap(c, "C10U", v33, "C4")
    _cap(c, "C100N", v33, "C5")
    _res(c, "R10K", "EN", v33, "R2")
    c.connect("EN", (u1, "EN"))
    _cap(c, "C1U", "EN", "C6")
    sw = c.add("SW", "SW1")
    c.connect("BOOT", (u1, modpin(GPIO["BOOT"])), (sw, "B"))
    _res(c, "R10K", "BOOT", v33, "R3")
    c.connect(gnd, (sw, "C"))
    c.connect("UART_TX", (u1, modpin(GPIO["UART_TX"])))
    c.connect("UART_RX", (u1, modpin(GPIO["UART_RX"])))
    tp = c.add("DEBUG", "TP1")
    for pad, net in DEBUG_PINS.items():
        c.connect(net, (tp, pad))
    for name in ("SDA0", "SCL0", "SDA1", "SCL1", "FUSB_INT", "LCD_BL", "LCD_RST", "LCD_DC", "LCD_CS",
                 "LCD_MOSI", "LCD_SCK", "BUZZER", "LOAD_EN", "AGND_EN", "BSHORT_EN", "EXP_RST"):
        c.connect(name, (u1, modpin(GPIO[name])))
    for ref, net in (("R4", "SDA0"), ("R5", "SCL0"), ("R6", "SDA1"), ("R7", "SCL1")):
        _res(c, "R4K7", net, v33, ref)

    # --- Expanders (bus 0): address straps, shared reset, decoupling, a series resistor per line.
    for ref, addr in EXP_ADDR.items():
        u = c.add("EXP", ref)
        c.connect(v33, (u, "VDD"))
        c.connect(gnd, (u, "VSS"))
        c.connect("SDA0", (u, "SDA"))
        c.connect("SCL0", (u, "SCK"))
        c.connect("EXP_RST", (u, "~{RESET}"))
        for k, pin in enumerate(("A0", "A1", "A2")):
            c.connect(v33 if (addr >> k) & 1 else gnd, (u, pin))
        _cap(c, "C100N", v33, "C1" + ref[2])
    _res(c, "R10K", "EXP_RST", v33, "R8")
    for net, (ref, pin) in LINES.items():
        _res(c, SERIES[net], net, f"X_{net}", series_ref(net))
        c.connect(f"X_{net}", (c.instances[ref], pin))
    # Force nets carry the INA226 inputs (~10 uA bias each), so they get firm pull-ups: a weak
    # internal 100k one could be pulled below the expander's VIH and read as connected.
    for k, net in enumerate(("A_VBUS", "A_GND", "B_VBUS", "B_GND")):
        _res(c, "R10K", net, v33, f"R{9 + k}")

    # --- Test ports.
    for end in ("A", "B"):
        jc = c.add("USBC", "J2" if end == "A" else "J5")
        for sig, pad in USBC_SIGNALS.items():
            c.connect(f"{end}_C_{sig}", (jc, _usbc_pin(jc, pad), pad))
        for kind in ("VBUS", "GND"):
            for pad in USBC_FORCE[kind]:
                c.connect(f"{end}_{kind}", (jc, kind, pad))
            for pad in USBC_SENSE[kind]:
                c.connect(f"{end}_{kind}_S", (jc, kind, pad))
            _res(c, "R10R", f"{end}_{kind}", f"{end}_{kind}_S", TIE_REFS[f"{end}_{kind}"])   # sense follows force with no plug
        c.connect(f"{end}_C_SH", (jc, "SHELL"))
        je = c.add("RJ45", "J4" if end == "A" else "J7")
        for n in range(1, 9):
            c.connect(f"{end}_E{n}", (je, str(n)))
        c.connect(f"{end}_E_SH", (je, "SH"))
    ja = c.add("USBA", "J3")
    c.connect("A_VBUS", (ja, "VBUS"))
    c.connect("A_GND", (ja, "GND"))
    for sig, pad in USBA_SIGNALS.items():
        c.connect(f"A_A_{sig}", (ja, _pin_by_pad(ja, pad), pad))
    c.connect("A_A_SH", (ja, "SH"))
    jm = c.add("MICROB", "J6")
    c.connect("B_VBUS", (jm, "VCC"))
    c.connect("B_GND", (jm, "GND"))
    for sig, name in MICROB_SIGNALS.items():
        c.connect(f"B_M_{sig}", (jm, name))
    c.connect("B_M_SH", (jm, "SHLD"))

    # --- E-marker: FUSB302B on the A-end USB-C's CC pins (bus 1). Its VCONN switch powers the
    # cable's e-marker from VBUS.
    u4 = c.add("FUSB", "U4")
    c.connect(v33, (u4, "VDD"))
    c.connect(gnd, (u4, "GND"), (u4, "EP"))
    c.connect(vbus, (u4, "VCONN"))
    c.connect("A_VBUS", (u4, "VBUS"))
    c.connect("A_C_CC1", (u4, "CC1"))
    c.connect("A_C_CC2", (u4, "CC2"))
    c.connect("FUSB_INT", (u4, "INT_N"))
    c.connect("SDA1", (u4, "SDA"))
    c.connect("SCL1", (u4, "SCL"))
    _res(c, "R10K", "FUSB_INT", v33, "R17")
    _cap(c, "C100N", v33, "C15")
    _cap(c, "C1U", v33, "C16")
    _cap(c, "C10U", vbus, "C17")                                         # VCONN source

    # --- Resistance check: VBUS -> PMOS -> 20R -> 100 mR shunt -> A_VBUS -> cable -> B_VBUS ->
    # back-to-back NMOS short -> B_GND -> cable -> A_GND -> NMOS -> GND. About 0.24 A.
    q1 = c.add("PMOS", "Q1")
    c.connect(vbus, (q1, "S"))
    c.connect("LOAD_HI", (q1, "D"))
    c.connect("LOAD_G", (q1, "G"))
    _res(c, "R10K", "LOAD_G", vbus, "R18")
    q2 = c.add("SMALLN", "Q2")
    c.connect("LOAD_G", (q2, "D"))
    c.connect(gnd, (q2, "S"))
    c.connect("LOAD_EN", (q2, "G"))
    _res(c, "R100K", "LOAD_EN", gnd, "R19")
    _res(c, "RLOAD", "LOAD_HI", "LOAD_SH", "R20")
    _res(c, "RSHUNT", "LOAD_SH", "A_VBUS", "R21")
    q3 = c.add("NMOS", "Q3")
    c.connect("A_GND", (q3, "D"))
    c.connect(gnd, (q3, "S"))
    c.connect("AGND_EN", (q3, "G"))
    _res(c, "R100K", "AGND_EN", gnd, "R22")
    # Back-to-back, so that with the gate low neither body diode can join B_VBUS to B_GND.
    q4, q5 = c.add("NMOS", "Q4"), c.add("NMOS", "Q5")
    c.connect("B_VBUS", (q4, "D"))
    c.connect("B_GND", (q5, "D"))
    c.connect("B_MID", (q4, "S"), (q5, "S"))
    c.connect("BSHORT_EN", (q4, "G"), (q5, "G"))
    _res(c, "R100K", "BSHORT_EN", gnd, "R23")

    # INA226s (bus 1): U5 the current through the shunt, U6 the VBUS conductor's drop, U7 the GND
    # conductor's drop (both Kelvin, from the sense rows).
    for ref, (inp, inn, bus) in {"U5": ("LOAD_SH", "A_VBUS", "A_VBUS"),
                                 "U6": ("A_VBUS_S", "B_VBUS_S", "B_VBUS_S"),
                                 "U7": ("B_GND_S", "A_GND_S", gnd)}.items():
        u = c.add("INA", ref)
        addr = INA_ADDR[ref] - 0x40              # A1:A0 straps: GND/GND 0x40, GND/VS 0x41, VS/GND 0x44
        c.connect(v33, (u, "VS+"))
        c.connect(gnd, (u, "GND"))
        c.connect(v33 if addr & 0x01 else gnd, (u, "A0"))
        c.connect(v33 if addr & 0x04 else gnd, (u, "A1"))
        c.connect(inp, (u, "VIN+"))
        c.connect(inn, (u, "VIN-"))
        c.connect(bus, (u, "VBUS"))
        c.connect("SDA1", (u, "SDA"))
        c.connect("SCL1", (u, "SCL"))
        c.no_connect(u, "Alert")
        _cap(c, "C100N", v33, f"C{13 + int(ref[1])}")         # C18, C19, C20

    # --- Display: 12-pin 0.5 mm ZIF for the panel's FPC; backlight from VBUS through 100R,
    # switched low-side.
    jf = c.add("FPC12", "J8")
    for pad, net in FPC_PINS.items():
        c.connect(net, (jf, pad))
    c.no_connect(jf, "7")
    c.connect(gnd, (jf, "13"), (jf, "14"))
    _res(c, "R100R", vbus, "LED_A", "R24")
    q6 = c.add("SMALLN", "Q6")
    c.connect("LED_K", (q6, "D"))
    c.connect(gnd, (q6, "S"))
    c.connect("LCD_BL", (q6, "G"))
    _res(c, "R100K", "LCD_BL", gnd, "R25")
    _cap(c, "C10U", v33, "C21")

    # --- Buzzer: magnetic, from 3V3, switched low-side, with a flyback diode.
    bz = c.add("BUZZER", "BZ1")
    c.connect(v33, (bz, "+"))
    c.connect("BUZ_K", (bz, "2"))
    c.no_connect(bz, "3")
    c.no_connect(bz, "4")
    q7 = c.add("NMOS", "Q7")
    c.connect("BUZ_K", (q7, "D"))
    c.connect(gnd, (q7, "S"))
    c.connect("BUZZER", (q7, "G"))
    _res(c, "R100K", "BUZZER", gnd, "R26")
    d2 = c.add("DIODE", "D2")
    c.connect(v33, (d2, "K"))
    c.connect("BUZ_K", (d2, "A"))

    for ref in ("H1", "H2", "H3", "H4"):
        c.connect(gnd, (c.add("MH", ref), "1"))
    return c


def _pin_by_pad(inst, pad):
    return next(name for name, pads in inst.part.pins.items() if pad in pads)


def _usbc_pin(inst, pad):
    return _pin_by_pad(inst, pad)

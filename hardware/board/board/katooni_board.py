"""Katooni main board (KATOO-1): an ESP32-S3-MINI-1 cable tester. Two clusters of test ports along
the front edge (A end: USB-C, USB-A 3.0, RJ45; B end: USB-C, micro-B, RJ45); every conductor of
every port runs through a series resistor to its own TCA9555 pin, so the firmware can drive one
line and read the rest. See DESIGN.md for the parts, the circuit and the reasoning.

Board frame (KiCad, y down): x 0..W, y 0..H. The module's antenna sits on the y = 0 edge (the
back), the test ports open across the y = H edge (the front).
"""
from board.netlist import Circuit
from board.placement import Placement, bbox

W, H = 100.0, 70.0

# GPIO choice. USB (19/20) and UART0 (43/44) are fixed by the silicon; GPIO0 is the BOOT button.
# The rest avoid the strapping pins (3, 45, 46), the flash pins (26-32) and 33-37 (octal-flash
# pins on other variants), as tinycrypt does.
GPIO = {"BOOT": 0, "SDA0": 1, "SCL0": 2, "SDA1": 4, "SCL1": 5, "FUSB_INT": 6, "LCD_BL": 7,
        "LCD_RST": 8, "LCD_DC": 9, "LCD_CS": 10, "LCD_MOSI": 11, "LCD_SCK": 12, "BUZZER": 13,
        "LOAD_EN": 14, "AGND_EN": 15, "BSHORT_EN": 16, "USB_DM": 19, "USB_DP": 20,
        "UART_TX": 43, "UART_RX": 44}
_MODPIN = {43: "TXD0", 44: "RXD0"}

# I2C addresses (7-bit). Bus 0 holds the five expanders; bus 1 the FUSB302 (fixed at 0x22, which
# is inside the TCA9555 range, hence two buses) and the three INA226s.
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
    # TCA9555, not MCP23017: on current MCP23017 datasheets GPA7/GPB7 are output-only, and every
    # line must be read as well as driven. All sixteen TCA9555 pins are bidirectional.
    pins = [f"P0{n}" for n in range(8)] + [f"P1{n}" for n in range(8)]
    out = {}
    for ref, nets in (("U10", a1), ("U11", a2), ("U12", a3), ("U13", b1), ("U14", b2)):
        assert len(nets) <= 16, ref
        for net, pin in zip(nets, pins):
            out[net] = (ref, pin)
    return out


LINES = _lines()


def series_ref(net):
    """R100-R115 for U10's lines, R120-R135 for U11's, ... in pin order (P00-P07, P10-P17)."""
    ref, pin = LINES[net]
    slot = 8 * int(pin[1]) + int(pin[2])
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
                 "LCD_MOSI", "LCD_SCK", "BUZZER", "LOAD_EN", "AGND_EN", "BSHORT_EN"):
        c.connect(name, (u1, modpin(GPIO[name])))
    for ref, net in (("R4", "SDA0"), ("R5", "SCL0"), ("R6", "SDA1"), ("R7", "SCL1")):
        _res(c, "R4K7", net, v33, ref)

    # --- Expanders (bus 0): address straps, decoupling, a series resistor per line. INT is unused
    # (the firmware polls), and the TCA9555 has no reset pin.
    for ref, addr in EXP_ADDR.items():
        u = c.add("EXP", ref)
        c.connect(v33, (u, "VCC"))
        c.connect(gnd, (u, "GND"))
        c.connect("SDA0", (u, "SDA"))
        c.connect("SCL0", (u, "SCL"))
        for k, pin in enumerate(("A0", "A1", "A2")):
            c.connect(v33 if (addr >> k) & 1 else gnd, (u, pin))
        _cap(c, "C100N", v33, "C1" + ref[2])
    for net, (ref, pin) in LINES.items():
        _res(c, SERIES[net], net, f"X_{net}", series_ref(net))
        c.connect(f"X_{net}", (c.instances[ref], pin))
    # Force nets carry the INA226 inputs (~10 uA bias each), so they get firm pull-ups: the
    # expander's weak internal 100k one could be pulled below its VIH and read as connected.
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
        # One shell pad carries the line: the metal shell joins the rest, so routing between them
        # would only crowd the receptacle. (The other footprints' extra shell pads are renumbered
        # in footprints/lcsc.pretty for the same reason: RJ45 SH2, USB-A 10b, micro-B 0b-0d.)
        c.connect(f"{end}_C_SH", (jc, "SHELL", "3"))
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


# --- Placement -----------------------------------------------------------------------------------
# (x, y, rotation) per ref. KiCad rotation is counter-clockwise on screen. Ports: openings 0.3 mm
# proud of the front edge (y = H) so the case's port holes can be snug.
FRONT = H + 0.3
MOD_X, MOD_Y = 50.0, 12.8          # the module's origin; its antenna strip ends on the y = 0 edge
EXP_Y = 36.0                       # expander row
EXP_X = {"U10": 15.0, "U12": 29.0, "U11": 43.5, "U13": 56.5, "U14": 82.0}
PORT_X = {"J4": 10.5, "J3": 29.5, "J2": 43.5, "J5": 56.5, "J6": 68.0, "J7": 89.0}


def _ports():
    from board.parts import BODY
    return {
        "J4": (PORT_X["J4"], round(FRONT + BODY["RJ45"][1], 3), 180),     # RJ45 opens at its -y
        "J3": (PORT_X["J3"], round(FRONT - BODY["USBA"][3], 3), 0),
        "J2": (PORT_X["J2"], round(FRONT - BODY["USBC"][3], 3), 0),
        "J5": (PORT_X["J5"], round(FRONT - BODY["USBC"][3], 3), 0),
        "J6": (PORT_X["J6"], round(FRONT - BODY["MICROB"][3], 3), 0),
        "J7": (PORT_X["J7"], round(FRONT + BODY["RJ45"][1], 3), 180),
    }


def _series_at():
    """Each expander's 16 series resistors, vertical, in two rows of eight below it: GPA0-7 in the
    upper row, GPB0-7 in the lower, left to right."""
    at = {}
    for ref, cx in EXP_X.items():
        base = 100 + 20 * (int(ref[1:]) - 10)
        for slot in range(16):
            row, col = divmod(slot, 8)
            at[f"R{base + slot}"] = (round(cx - 5.25 + 1.5 * col, 3), EXP_Y + 7.6 + 2.2 * row, 90)
    return at


def _at():
    m = lambda dx, dy, rot=0: (round(MOD_X + dx, 3), round(MOD_Y + dy, 3), rot)
    at = {
        # module and its neighbours, at tinycrypt's offsets from the module where they apply
        "U1": (MOD_X, MOD_Y, 0),
        "C5": m(-9.0, -4.75, 90), "C4": m(-9.0, -2.1, 270), "R3": m(-9.0, 0.55, 90),
        "R2": m(9.1, -5.15, 0), "C6": m(9.1, -3.45, 180), "TP1": m(9.4, 3.25, 0),
        "SW1": (34.0, 14.5, 90),
        "R4": (38.4, 16.0, 0), "R5": (38.4, 17.2, 0), "R6": (38.4, 18.4, 0), "R7": (38.4, 19.6, 0),
        # power in, back left: J1 opens across the y = 0 edge
        "J1": (12.0, 3.21, 180), "R1": (17.6, 5.0, 90),
        "F1": (5.5, 10.0, 90), "D1": (5.5, 16.0, 90), "U3": (12.0, 11.0, 0),
        "C1": (9.5, 15.5, 90), "U2": (14.5, 15.8, 0), "C2": (19.0, 13.5, 90), "C3": (19.0, 16.0, 90),
        # display ZIF and backlight, back right; the panel's tail enters J8 from the +y side
        "J8": (80.0, 4.0, 0), "R24": (72.0, 9.0, 90), "Q6": (72.0, 13.0, 0), "R25": (72.0, 16.5, 90),
        "C21": (88.0, 9.5, 90),
        # buzzer, right
        "BZ1": (92.0, 18.0, 0), "Q7": (84.5, 17.0, 90), "D2": (84.5, 21.5, 90), "R26": (81.5, 17.0, 90),
        # mounting holes
        "H1": (3.0, 3.0, 0), "H2": (97.0, 3.0, 0), "H3": (3.0, 50.5, 0), "H4": (97.0, 50.5, 0),
        # The analog block, in the band between the module and the expanders, so that the 68 test
        # lines have the space between the resistor rows and the ports to themselves. Kelvin sensing
        # doesn't mind long traces, and the FUSB302's CC signalling is slow.
        # Left of the module: A-end pull-ups and ties, the FUSB302, the load switch and its driver.
        "R9": (20.0, 24.0, 90), "R10": (21.2, 24.0, 90), "R13": (20.0, 27.5, 90), "R14": (21.2, 27.5, 90),
        "C15": (23.4, 24.0, 90), "U4": (26.0, 24.0, 0), "C16": (28.6, 24.0, 90), "C17": (30.2, 24.0, 90),
        "R17": (26.0, 27.2, 0),
        "Q1": (35.5, 24.0, 0), "R18": (38.5, 24.0, 90), "Q2": (41.0, 25.5, 0), "R19": (41.0, 28.5, 0),
        # Below the module: the A-end return switch and the B-end short.
        "Q3": (47.0, 24.5, 0), "R22": (47.0, 27.5, 0), "Q4": (53.0, 24.5, 0), "Q5": (53.0, 28.0, 0),
        "R23": (50.0, 28.5, 90),
        # Right of the module: the 20R and the shunt, then the INA226s.
        "R20": (62.5, 24.6, 0), "R21": (62.5, 28.8, 0),
        "U5": (70.5, 24.2, 90), "C18": (74.5, 24.2, 90), "U6": (70.5, 28.4, 90), "C19": (74.5, 28.4, 90),
        "U7": (79.0, 26.3, 90), "C20": (83.0, 26.3, 90),
        # B-end pull-ups and ties, between the B-end expanders.
        "R11": (65.5, 33.0, 90), "R12": (66.7, 33.0, 90), "R15": (67.9, 33.0, 90), "R16": (69.1, 33.0, 90),
    }
    for ref, cx in EXP_X.items():
        at[ref] = (cx, EXP_Y, 90)
        at["C1" + ref[2]] = (cx + 5.8, EXP_Y - 1.3, 90)
    at.update(_ports())
    at.update(_series_at())
    return at


def place_katooni(circuit):
    at = _at()
    missing = set(circuit.instances) - set(at)
    if missing:
        raise KeyError(f"no position for {sorted(missing)}")
    bodies = {r: i.part.body for r, i in circuit.instances.items()}
    return Placement((0.0, 0.0, W, H), {r: at[r] for r in circuit.instances}, bodies)


def overlaps(placement, gap=0.0):
    """Pairs of parts whose rotated bodies overlap (or come within 'gap' mm), and parts poking
    outside the outline other than the ports' openings."""
    refs = sorted(placement.at)
    boxes = {r: bbox(placement, r) for r in refs}
    out = []
    for i, a in enumerate(refs):
        ax0, ay0, ax1, ay1 = boxes[a]
        for b in refs[i + 1:]:
            bx0, by0, bx1, by1 = boxes[b]
            if ax0 < bx1 + gap and bx0 < ax1 + gap and ay0 < by1 + gap and by0 < ay1 + gap:
                out.append((a, b))
    x0, y0, x1, y1 = placement.outline
    for r, (bx0, by0, bx1, by1) in boxes.items():
        if bx0 < x0 or by0 < y0 or bx1 > x1 or (by1 > y1 and r not in PORT_X):
            out.append((r, "outline"))
    return out


# Antenna keep-out: no copper on any layer under the antenna strip and 5 mm either side of it,
# down to the module's top pad row (Espressif's guidance for an on-board antenna at the edge).
ANTENNA_KEEPOUT = (MOD_X - 7.7 - 5.0, 0.0, MOD_X + 7.7 + 5.0, round(MOD_Y - 7.75 - 0.1, 3))


def keepouts():
    x0, y0, x1, y1 = ANTENNA_KEEPOUT
    return [("antenna", [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], ["F.Cu", "In1.Cu", "In2.Cu", "B.Cu"])]


def route_area():
    """The router's boundary: the outline less the antenna keep-out (a notch in the back edge)."""
    x0, _, x1, y1 = ANTENNA_KEEPOUT
    return [(0.0, 0.0), (x0, 0.0), (x0, y1), (x1, y1), (x1, 0.0), (W, 0.0), (W, H), (0.0, H)]


# --- Silkscreen, fab notes, firmware header ------------------------------------------------------
TITLE = "Katooni v0.1"
CREDIT = "Designed by Nakomis/C"
# Front labels only where there is room above the port: the USB-C and micro-B ports have the analog
# block right behind them, and their shapes say what they are anyway.
PORT_LABELS = {"J4": "RJ45", "J3": "USB-A", "J7": "RJ45"}
J1_LABELS = {"1": "V", "2": "D-", "3": "D+", "4": "CC", "5": "G"}


def _pad_xy(circuit, placement, ref, number):
    from board.placement import rotate
    x, y, rot = placement.at[ref]
    pad = next(q for q in circuit.instances[ref].part.footprint.pads if q.number == number)
    dx, dy = rotate(pad.x, pad.y, rot)
    return round(x + dx, 3), round(y + dy, 3)


def silk(circuit, placement):
    """(text, x, y, size[, layer[, rotation]]) entries. Front: the A and B cluster labels and port
    names above the ports, J1's pin labels, the debug pad labels. Back: the title and credit."""
    out = []
    for ref, text in PORT_LABELS.items():
        x, _, _ = placement.at[ref]
        out.append((text, x, 51.0, 1.0))
    ax = (PORT_X["J4"] + PORT_X["J2"]) / 2
    bx = (PORT_X["J5"] + PORT_X["J7"]) / 2
    out += [("A", round(ax - 7.0, 3), 51.4, 2.0), ("B", round(bx + 3.0, 3), 51.4, 2.0)]
    for n, text in J1_LABELS.items():
        x, y = _pad_xy(circuit, placement, "J1", n)
        out.append((text, x, round(y + 1.7 + (0.9 if int(n) % 2 == 0 else 0.0), 3), 0.8))
    for n, text in DEBUG_LABELS.items():
        x, y = _pad_xy(circuit, placement, "TP1", n)
        out.append((text, round(x + 1.6, 3), y, 0.8))
    out.append(("BOOT", placement.at["SW1"][0], round(placement.at["SW1"][1] + 4.2, 3), 0.8))
    out.append(("DISPLAY", placement.at["J8"][0], round(placement.at["J8"][1] + 4.6, 3), 0.8))
    out += [("Katooni", W / 2, 30.0, 3.0, "B.SilkS"), (CREDIT, W / 2, 34.0, 1.2, "B.SilkS"),
            (TITLE, W / 2, 36.5, 1.2, "B.SilkS")]
    return out


def fab_notes():
    """(text entries, user rectangles) on Dwgs.User: the antenna keep-out, drawn and labelled."""
    x0, y0, x1, y1 = ANTENNA_KEEPOUT
    return [("ANTENNA KEEP-OUT: NO COPPER, ALL LAYERS", MOD_X, -1.5, 0.8, "Dwgs.User")], \
        [(x0, y0, x1, y1, "Dwgs.User")]


def pins_h():
    """The firmware header: GPIOs, I2C addresses and which expander pin each test line is on."""
    lines = ["/* Generated by hardware/board/build.py from board/katooni_board.py: do not edit. */",
             "#pragma once", "", "/* GPIOs */"]
    for name, gpio in GPIO.items():
        lines.append(f"#define KT_PIN_{name} {gpio}")
    lines += ["", "/* I2C bus 0: the expanders; bus 1: FUSB302 and INA226s (7-bit addresses) */"]
    for ref, addr in EXP_ADDR.items():
        lines.append(f"#define KT_EXP_{ref} 0x{addr:02X}")
    lines.append(f"#define KT_FUSB302 0x{FUSB_ADDR:02X}")
    for ref, name in (("U5", "CURRENT"), ("U6", "VBUS_DROP"), ("U7", "GND_DROP")):
        lines.append(f"#define KT_INA226_{name} 0x{INA_ADDR[ref]:02X}")
    lines += ["#define KT_SHUNT_MILLIOHM 100", "",
              "/* Test lines: expander (TCA9555) index (0 = U10 at 0x20 ... 4 = U14 at 0x24) and bit",
              "   (0-7 = P00-P07, port 0; 8-15 = P10-P17, port 1). The X macro lists them in expander",
              "   order. */",
              "#define KT_LINES(X) \\"]
    order = sorted(LINES.items(), key=lambda kv: (kv[1][0], kv[1][1]))
    for k, (net, (ref, pin)) in enumerate(order):
        idx = int(ref[1:]) - 10
        bit = 8 * int(pin[1]) + int(pin[2])
        end = " \\" if k < len(order) - 1 else ""
        lines.append(f"    X({net}, {idx}, {bit}){end}")
    lines.append(f"#define KT_LINE_COUNT {len(LINES)}")
    return "\n".join(lines) + "\n"

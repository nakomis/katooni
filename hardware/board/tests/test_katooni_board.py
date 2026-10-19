from board import katooni_board as kb
from board.netlist import check
from board.parts import PARTS, part
from board.placement import bbox

C = kb.build_katooni()
P = kb.place_katooni(C)


def _pads_of(net):
    return C.nets[net]


def test_netlist_is_clean():
    assert check(C) == []


def test_every_line_has_its_own_resistor_and_expander_pin():
    used = set()
    for net, (ref, pin) in kb.LINES.items():
        x = f"X_{net}"
        assert len(_pads_of(x)) == 2, net                     # series resistor + expander pin, nothing else
        r = kb.series_ref(net)
        assert (r, "2") in _pads_of(x) and (r, "1") in _pads_of(net)
        assert C.instances[r].part.key == kb.SERIES[net]
        assert (ref, pin) not in used
        used.add((ref, pin))
    assert len(kb.LINES) == 68


def test_cc_series_resistors():
    assert {kb.SERIES[f"A_C_CC{n}"] for n in (1, 2)} == {"R10K"}     # VCONN side
    assert {kb.SERIES[f"B_C_CC{n}"] for n in (1, 2)} == {"R5K1"}     # driven low = Rd
    assert {v for k, v in kb.SERIES.items() if "_CC" not in k} == {"R1K"}


def test_usbc_vbus_and_gnd_are_split_by_row_for_kelvin_sensing():
    for end, ref in (("A", "J2"), ("B", "J5")):
        assert {p for r, p in _pads_of(f"{end}_VBUS") if r == ref} == {"A4"}
        assert {p for r, p in _pads_of(f"{end}_VBUS_S") if r == ref} == {"B9"}
        assert {p for r, p in _pads_of(f"{end}_GND") if r == ref} == {"A1"}
        assert {p for r, p in _pads_of(f"{end}_GND_S") if r == ref} == {"B1"}
        for pad in ("A9", "A12", "B4", "B12"):            # joined inside the plug, not on the board
            assert C.net_of(ref, pad) is None


def test_every_usbc_signal_pad_is_its_own_line():
    for end, ref in (("A", "J2"), ("B", "J5")):
        for sig, pad in kb.USBC_SIGNALS.items():
            assert (ref, pad) in _pads_of(f"{end}_C_{sig}")


def test_test_ports_never_touch_board_ground():
    gnd_refs = {r for r, _ in C.nets["GND"]}
    for ref in ("J2", "J3", "J4", "J5", "J6", "J7"):
        assert ref not in gnd_refs, ref


def test_ina226_inputs():
    pin = lambda ref, name: C.net_of(ref, part("INA").pins[name][0])
    assert (pin("U5", "VIN+"), pin("U5", "VIN-")) == ("LOAD_SH", "A_VBUS")
    assert (pin("U6", "VIN+"), pin("U6", "VIN-")) == ("A_VBUS_S", "B_VBUS_S")
    assert (pin("U7", "VIN+"), pin("U7", "VIN-")) == ("B_GND_S", "A_GND_S")


def test_i2c_addresses_are_unique_per_bus():
    assert len(set(kb.EXP_ADDR.values())) == 5
    bus1 = list(kb.INA_ADDR.values()) + [kb.FUSB_ADDR]
    assert len(set(bus1)) == len(bus1)
    for ref, addr in kb.EXP_ADDR.items():
        straps = [C.net_of(ref, part("EXP").pins[a][0]) for a in ("A0", "A1", "A2")]
        assert sum((s == "+3V3") << k for k, s in enumerate(straps)) == addr - 0x20, ref


def test_b_end_short_is_back_to_back():
    assert C.net_of("Q4", "2") == C.net_of("Q5", "2") == "B_MID"           # sources joined
    assert {C.net_of("Q4", "3"), C.net_of("Q5", "3")} == {"B_VBUS", "B_GND"}


def test_placement_has_no_overlaps_and_ports_open_on_the_front_edge():
    assert kb.overlaps(P) == []
    for ref in kb.PORT_X:
        assert abs(bbox(P, ref)[3] - kb.FRONT) < 1e-6, ref


def test_nothing_in_the_antenna_keepout():
    x0, y0, x1, y1 = kb.ANTENNA_KEEPOUT
    for ref in P.at:
        if ref == "U1":
            continue
        bx0, by0, bx1, by1 = bbox(P, ref)
        assert not (bx0 < x1 and x0 < bx1 and by0 < y1 and y0 < by1), ref


def test_pins_h_lists_every_line_once():
    h = kb.pins_h()
    for net in kb.LINES:
        assert h.count(f"X({net}, ") == 1, net
    assert "#define KT_LINE_COUNT 68" in h and "#define KT_PIN_LOAD_EN 14" in h


def test_every_part_has_a_footprint_and_board_only_parts_have_no_lcsc():
    for key, p in PARTS.items():
        assert p.footprint.pads, key
        assert (p.lcsc is None) == (key in ("DEBUG", "MH")), key

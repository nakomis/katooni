"""Parts catalogue: LCSC/EasyEDA footprints and symbols (plus two local board-only footprints),
keyed by role."""
from dataclasses import dataclass
from pathlib import Path

from board.kicadlib import Footprint, load_footprint, load_symbol_pin_types, load_symbol_pins

HERE = Path(__file__).resolve().parent.parent
LCSC_FP = HERE / "footprints" / "lcsc.pretty"
LCSC_SYM = HERE / "footprints" / "lcsc.kicad_sym"
LOCAL_FP = HERE / "footprints" / "local.pretty"


@dataclass
class PartDef:
    key: str
    value: str
    lcsc: str | None         # None: a board-only footprint (test pads, mounting hole), never in BOM/CPL
    footprint: Footprint
    pins: dict
    pin_types: dict
    extended: bool
    tht: bool
    rotation_offset: float   # degrees added to placement rotation for JLCPCB CPL
    body: tuple              # (xmin, ymin, xmax, ymax): the part's real extent, for placement checks


# key: (value, lcsc, footprint file stem, symbol name, extended, rotation_offset)
# Footprint stems and symbol names are exactly what easyeda2kicad wrote. lcsc None means a local,
# board-only footprint from footprints/local.pretty with pin name = pad number.
_TABLE = {
    "ESP32": ("ESP32-S3-MINI-1-N8", "C2913206", "BULETM-SMD_ESP32-S3-MINI-1-N8", "ESP32-S3-MINI-1-N8", True, 0),
    "JST5": ("SM05B-SRSS-G-TB", "C265419", "CONN-SMD_SM05B-SRSS-G-TB-LF-SN", "SM05B-SRSS-G-TB(LF)(SN)", True, 0),
    "ESD": ("USBLC6-2SC6", "C7519", "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL", "USBLC6-2SC6", True, 0),
    "LDO": ("AP2112K-3.3TRG1", "C51118", "SOT-25-5_L2.9-W1.6-P0.95-LS2.8-BL", "AP2112K-3.3TRG1", True, 0),
    "PTC": ("1.1A PTC", "C20801", "F1206", "SMD1206P110TF", True, 0),
    "TVS": ("SMBJ5.0A", "C78423", "SMB_L4.6-W3.6-LS5.3-RD", "SMBJ5.0A", True, 0),
    "SW": ("TS-1187A-B-A-B", "C318884", "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5", "TS-1187A-B-A-B", False, 0),
    "EXP": ("MCP23017T-E/SS", "C558584", "SSOP-28_L10.2-W5.3-P0.65-LS7.8-BL", "MCP23017T-E_SS", True, 0),
    "FUSB": ("FUSB302BMPX", "C132291", "MLP-14_L2.5-W2.5-P0.50-BL-EP", "FUSB302BMPX", True, 0),
    "INA": ("INA226AIDGSR", "C49851", "MSOP-10_L3.0-W3.0-P0.50-LS5.0-BL", "INA226AIDGSR", True, 0),
    "NMOS": ("AO3400A", "C20917", "SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR", "AO3400A", False, 0),
    "PMOS": ("AO3401A", "C15127", "SOT-23_L2.9-W1.3-P1.90-LS2.4-BR", "AO3401A", False, 0),
    "SMALLN": ("2N7002", "C8545", "SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR", "2N7002", False, 0),
    "DIODE": ("1N4148WS", "C2128", "SOD-323_L1.8-W1.3-LS2.5-RD", "1N4148WST4", False, 0),
    "BUZZER": ("MLT-8530", "C94599", "BUZ-SMD_4P-L8.5-W8.5-P8.50-BR", "MLT-8530", True, 0),
    "FPC12": ("05A20H12P", "C5362174", "FPC-SMD_12P-P0.50_05A20H12P", "05A20H12P", True, 0),
    "USBC": ("CX90B1-24P", "C597821", "USB-C-SMD_DX07S024JJ3R1300", "CX90B1-24P", True, 0),
    "USBA": ("HC-USB3.0-L168-WP", "C7501850", "USB-TH_USB3.0-A", "HC-USB3.0-L168-WP", True, 0),
    "MICROB": ("U-F-M5DD-Y-L", "C91146", "MICRO-USB-SMD_MICRO-USB-18", "U-F-M5DD-Y-L", True, 0),
    "RJ45": ("RJHSE5380", "C464586", "RJ45-TH_AMPHENOL_RJHSE5380", "RJHSE5380", True, 0),
    "RLOAD": ("20R 1W", "C41950", "R2512", "25121WF200JT4E", True, 0),
    "RSHUNT": ("100mR 1%", "C2903476", "RES-SMD_L6.4-W3.2-R2512", "HOJLR2512-3W-100MR-1%", True, 0),
    # Every 0402 resistor shares one footprint and (two-pin) symbol; only the value and LCSC differ.
    "R10R": ("10R", "C25077", "R0402", "0402WGF1002TCE", False, 0),
    "R100R": ("100R", "C25076", "R0402", "0402WGF1002TCE", False, 0),
    "R1K": ("1k", "C11702", "R0402", "0402WGF1002TCE", False, 0),
    "R5K1": ("5k1", "C25905", "R0402", "0402WGF5101TCE", False, 0),
    "R4K7": ("4k7", "C25900", "R0402", "0402WGF4701TCE", False, 0),
    "R10K": ("10k", "C25744", "R0402", "0402WGF1002TCE", False, 0),
    "R100K": ("100k", "C25741", "R0402", "0402WGF1002TCE", False, 0),
    "C100N": ("100nF", "C1525", "C0402", "CL05B104KO5NNNC", False, 0),
    "C1U": ("1uF", "C52923", "C0402", "CL05A105KA5NQNC", False, 0),
    "C10U": ("10uF", "C19702", "C0603", "CL10A106KP8NNNC", False, 0),
    "DEBUG": ("Debug pads", None, "DebugPads_1x5_P2.54", None, False, 0),
    "MH": ("M2 hole", None, "MountingHole_M2_2.2mm_Pad4.4", None, False, 0),
}

# fetched-name -> our-name, per key. The module symbol calls GPIOn "IOn" and spells the UART0 pins
# TXD0/RXD0 (GPIO43/44), exactly as Espressif's datasheet table does; we keep those names.
ALIASES = {}

# Real extents where the footprint's courtyard is smaller than the part. The EasyEDA module
# footprint's courtyard covers only the pad field (15.4 x 15.4); the module is 15.4 x 20.5 with
# the PCB antenna in the 5.05 mm beyond pads 46-60 (its silkscreen outline: y -12.8 .. 7.7).
# The connectors' courtyards cover their pins, not their bodies.
BODY = {"ESP32": (-7.7, -12.8, 7.7, 7.7),
        "JST5": (-3.9, -2.79, 3.9, 3.16),      # tabs x +-3.9, signal pads to y -2.79, housing front 2.83
        # Connector bodies (from the footprints' silkscreen); each opening is at the +y edge,
        # except the RJ45's, at -y.
        "USBC": (-4.6, -3.65, 4.6, 5.2),
        "USBA": (-8.42, -2.9, 8.42, 13.6),
        "MICROB": (-4.5, -2.34, 4.5, 4.03),
        "RJ45": (-9.53, -9.0, 9.53, 6.97),
        "FPC12": (-5.55, -2.42, 5.55, 3.55)}
# The antenna strip of the module, in footprint coordinates (above the top pad row).
ESP32_ANTENNA = (-7.7, -12.8, 7.7, -7.75)

# 3D models (fetch_models.sh puts them in footprints/lcsc.3dshapes). Each EasyEDA footprint names its
# model and carries its offset and rotation; parts whose footprint names none get one here.
LCSC_3D = HERE / "footprints" / "lcsc.3dshapes"
MODEL_OVERRIDE = {}
# Model offsets (x, y, z in mm, KiCad's model frame) that replace easyeda2kicad's where its are
# wrong, found from top and side renders: the SOT-23-6 and C0603 offsets push the bodies off their
# pads, the JST model's origin is 2 mm off its centre, and the capacitor models are centred on
# z = 0 (half inside the board), so they are lifted by half their height.
MODEL_OFFSET = {"ESD": (0, 0, 0), "JST5": (-2.0, -0.33, 0),
                "C10U": (0, 0, 0.4), "C100N": (0, 0, 0.25), "C1U": (0, 0, 0.25)}


def _load(key, row):
    value, lcsc, fp_stem, sym, extended, rot = row
    fp = load_footprint((LCSC_FP if lcsc else LOCAL_FP) / f"{fp_stem}.kicad_mod")
    pins = {}
    if sym is None:     # footprint-only part: one passive pin per numbered pad, name = number
        pins = {p.number: [p.number] for p in fp.pads if p.number}
        pin_types = {n: "passive" for n in pins}
    else:
        alias = ALIASES.get(key, {})
        for name, nums in load_symbol_pins(LCSC_SYM, sym).items():
            pins.setdefault(alias.get(name, name), []).extend(nums)
        pin_types = load_symbol_pin_types(LCSC_SYM, sym)
    # Through-hole only if most numbered signal pads are drilled.
    numbered = [p for p in fp.pads if p.number]
    tht = bool(numbered) and sum(p.kind == "thru_hole" for p in numbered) * 2 > len(numbered)
    return PartDef(key, value, lcsc, fp, pins, pin_types, extended, tht, rot, BODY.get(key, fp.courtyard))


# Extended parts whose CPL rotation offset has been checked. EasyEDA-fetched footprints follow
# JLCPCB's own orientation convention, so their offset is 0. The final check is a look at
# JLCPCB's placement preview at order time.
ROTATION_CHECKED = {k for k, row in _TABLE.items() if row[4] and row[5] == 0}

PARTS = {k: _load(k, row) for k, row in _TABLE.items()}


def part(key):
    return PARTS[key]

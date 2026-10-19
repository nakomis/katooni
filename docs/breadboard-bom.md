# Breadboard prototype BOM

For proving the drive-and-sense scheme, the screen and the bagging logic before the JLCPCB board
exists. **Prices are unverified estimates from memory** unless marked ✓, which means a real listing
Martin found on 19 Oct 2026 (AliExpress, incl. VAT). Check each listing's price and pinout before
buying, because generic breakouts vary.

## Warning: wire count

Even without soldering the circuit, the full tester is about **73 signal lines**, one per conductor
per port. On a breadboard that's about **150 jumper wires** (breakout → expander, plus I²C, power and
the extras). Most breakouts also ship with **loose headers**, so expect about **150–200 header
joints**: the two 24-pin USB-C breakouts alone are about 50.

So build it in stages. Stage 1 covers everything interesting with a third of the wiring.

| Stage | Ports | Lines | Expanders | Proves |
|---|---|---|---|---|
| **1** | RJ45 ↔ RJ45 | 18 | 2 | Drive/sense scheme, screen, bagging, web page |
| **2** | + USB-C ↔ USB-C | +40 | +3 (5 total) | USB classification, resistance check |
| 3 | + USB-A (A end), micro-B (B end) | +15 | +1 (6 total) | Full v1 port set |

The real PCB does all of this with no wiring at all. Stage 1 alone is a reasonable prototype.

## Core (all stages)

| Qty | Part | Notes | ≈ £ each |
|---|---|---|---|
| 1 | ESP32-S3 devkit (N16R8 or similar) | You probably have one spare. Run at 160 MHz | 7 |
| 1 | 2.0" ST7789 240×320 IPS SPI module | ✓ Estardyn GMT020-02 (CS, DC, RST, SDA, SCL, VCC, GND; backlight always on), or Adafruit 4311 ($19.95, with a level shifter) | 2.80 ✓ |
| 1 | Passive buzzer module | 3.3 V | 1 |
| 4 | 830-point breadboard | Stage 1 needs 2 | 2 |
| 1 | Jumper wire set (M-M, 200+) | Pre-cut lengths keep it tidier | 6 |
| 1 | 1 kΩ resistor pack (100) | Series protection on each line | 2 |

## Drive and sense

| Qty | Part | Notes | ≈ £ each |
|---|---|---|---|
| 2 → 6 | MCP23017 (DIP-28) or CJMCU-23017 module | **14 usable lines each**: GPA7/GPB7 are output-only on current datasheets, and every line must drive and sense. Addresses 0x20–0x26 via A0–A2. Modules save wiring | 3.22 ✓ (£6.44 for 2) |
| 2 | RJ45 8P8C breakout, **shielded with a shield pin** | Needed to detect STP vs UTP; not every breakout brings the shield out | 2 |
| 2 | USB-C 24-pin breakout (all pins) | Stage 2. ✓ Javino male-to-female test board: pass-through, so it doubles as a CC/PD sniffing point for the scope. Pads, so headers need soldering | 1.80 ✓ |
| 1 | USB-A 3.0 female breakout (9-pin) | Stage 3 | 2 |
| 1 | Micro-USB female breakout (5-pin + shell) | Stage 3 | 1 |

## Resistance check (stage 2)

| Qty | Part | Notes | ≈ £ each |
|---|---|---|---|
| 2 | INA226 module | One measures the current and the near-end voltage, one the far-end voltage, so the readings are Kelvin-style | 3 |
| 1 | Logic-level N-MOSFET module (e.g. AO3400 or IRLZ44N) | Switches the load from a GPIO | 1 |
| 1 | 10 Ω 5 W resistor | 5 V / 10 Ω = 0.5 A for a moment; the INA226 measures the real current anyway | 1 |

## E-marker read (the hard one)

There's no good cheap FUSB302 breakout right now. The Tindie ones (Reclaimer Labs, WaffleBomb) are
out of stock or not shipping, so the options are:

1. **Skip it on the breadboard** (recommended). Prove the e-marker read on the real board, where the
   FUSB302B and VCONN switch are on the PCB anyway. The decoding logic is unit-tested on the Mac.
2. onsemi **FUSB302BGEVB** evaluation board, about £60–80 from DigiKey/Farnell. Drive it over I²C
   through its test points (SCL, SDA, INT_N), plus a VCONN switch (a P-MOSFET or a load switch).
3. A bare FUSB302 breakout PCB (a PCBWay shared project), but that means hand-soldering a QFN. No.

## Rough totals

- **Stage 1:** about £30 (estimate) (devkit, screen, 2 expanders, 2 RJ45 breakouts, breadboards, wires). Less
  if you already have a devkit and breadboards.
- **Stages 1–3:** about £50.
- **Plus the FUSB302 eval board,** if you want it: +£60–80.

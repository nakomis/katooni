# Katooni

A DIY cable tester (USB-C, USB-A, RJ45) on an ESP32-S3 Youngling. It works out what each cable
really is and sorts the pile into bags of identical cables. Plane project: **KATOO**.

## Status

Design only. `docs/design.md` is the source of truth for v1 scope, non-goals and the hardware and
firmware approach. Read it before starting any work item.

## Repository layout

| Path | What it is |
|---|---|
| `docs/design.md` | The v1 design |
| `docs/architecture/` | draw.io diagrams; SVGs are generated on commit |
| `docs/logo-candidates/` | Logo candidates |
| `embedded/core/` | Portable C core: classification, e-marker decoding, resistance, bagging; Mac tests |
| `embedded/main/` | ESP-IDF 5.5 firmware: drivers, the plug-and-go loop, Wi-Fi |
| `embedded/components/katooni_core/` | The core as an ESP-IDF component (same sources) |
| `embedded/tools/make_assets.py` | Regenerates the splash image and screen fonts in `embedded/main/assets/` |
| `hardware/board/` | Code-generated PCB for JLCPCB assembly (to come) |
| `hardware/case/` | Onshape FeatureScript case (to come) |

## Conventions

- **Follow tinycrypt** (`~/repos/nakomis/tinycrypt`) for the patterns this project shares:
  - ESP-IDF layout, with a portable core that also builds and is tested on the Mac
  - the code-generated board (`hardware/key-board/build.py`: KiCad, DRC gate, JLCPCB BOM/CPL/gerbers)
  - the Onshape FeatureScript case
- **USB connector:** the house JST SH 1.0 5-pin (see `~/CLAUDE.md`). Never the 4-pin order.
- **Clock:** run the S3 at 160 MHz. The workload is light, and this sidesteps the 240 MHz
  core-voltage issue described in `~/CLAUDE.md`.
- **Honesty:** when the hardware can't tell something (bandwidth, Ethernet category), say so in
  the output rather than guessing.

## Testing

The portable core (`embedded/core/`: pure C11, no ESP-IDF) builds and is tested on the Mac.
Coverage must stay at 70% or above; it was 98.8% of lines when the core landed.

```bash
cmake -S embedded/core -B build/core && cmake --build build/core && ctest --test-dir build/core
embedded/core/coverage.sh          # the same with clang coverage, plus a per-file report
```

- Each `tests/test_*.c` is its own program using the macros in `tests/check.h`.
- `simcable.c` makes synthetic scans and e-marker replies for the tests and the Mac simulator. The
  tests also include hand-written scans and a hand-built e-marker reply, so they don't only check
  the classifier against its own tables.
- **No e-marker reply in the tests is captured from a real cable yet.** Add real captures to
  `tests/test_emarker.c` once the board runs.

### Firmware

```bash
embedded/build.sh                 # idf.py build (ESP-IDF 5.5 at ~/esp/v5.5/esp-idf)
embedded/build.sh flash monitor   # anything else is passed to idf.py
```

- **Two pin profiles** (Kconfig "Pin profile"): the PCB, and a breadboard prototype on an
  ESP32-S3-DevKitC with an Estardyn GMT020-02 display (2.0" ST7789V; no backlight pin, no MISO).
  `embedded/build.sh --breadboard` builds the latter in `build-breadboard/` from
  `sdkconfig.breadboard`. Display inversion and row/column offsets are Kconfig options, since
  ST7789V panels vary.
- **Pins are placeholders.** `embedded/main/board_pins.h` holds the pin map until KATOO-1's
  generated `pins.h` replaces it (every value is marked TODO).
- **Wi-Fi:** credentials go in `embedded/sdkconfig.secrets` (gitignored; copy
  `sdkconfig.secrets.example`). Without them Katooni runs an open "katooni" access point.
- **Screen assets** are generated and committed, so a build doesn't need Pillow:
  `python3 embedded/tools/make_assets.py --font <SourceCodePro-Bold.ttf>`. The fonts are rendered
  from Source Code Pro (OFL); don't swap in a font that isn't openly licensed.
- **Nothing in `embedded/main/` has run on hardware yet.** It compiles with no warnings.

### Core conventions

- **Every connector pin is its own line** (`lines.h`): 71 in all. The hardware must never tie a
  connector's GND, VBUS or shield to board ground, except while a PD or load test runs.
- **A scan is a set of joins** (`scan.h`, union-find). The classifier tries every USB-C plug
  orientation and keeps the one that explains the most wires.
- **Allowed incidental joins** aren't faults: Ra on a plug's VCONN pin, Rp on an A-to-C cable's
  CC, Rd on a C-to-micro-B cable's CC, a micro-B ID tied to GND (OTG), and shield to GND. The
  hardware decides whether such a resistor reads as "joined", so the core accepts either.

## Architecture diagrams

Source files are in `docs/architecture/*.drawio`. Their SVGs are regenerated on commit by
`.githooks/pre-commit`.

To activate the hook after cloning:

```bash
git config core.hooksPath .githooks
```

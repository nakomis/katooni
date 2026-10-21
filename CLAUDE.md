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
| `embedded/main/` | ESP-IDF 5.5 firmware: drivers, the plug-and-go loop, Wi-Fi, web server |
| `embedded/main/web/index.html` | The session web page (served by the device and by the simulator) |
| `embedded/sim/` | Mac simulator: core + web page + JSON API with canned cables, and its API tests |
| `embedded/components/katooni_core/` | The core as an ESP-IDF component (same sources) |
| `embedded/tools/make_assets.py` | Regenerates the splash image and screen fonts in `embedded/main/assets/` |
| `infra/` | CDK: each Katooni as an IoT Thing for Fleet (`KatooniDeviceStack`); signing and provisioning scripts |
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
cmake -S embedded/sim -B build/sim && cmake --build build/sim && ctest --test-dir build/sim   # core + simulator
build/sim/katooni-sim --demo       # the web page on http://127.0.0.1:8080/ with a drawer of canned cables
```

- **The web page's JSON comes from the core** (`json.c`), streamed to a sink: the firmware sends
  it in 1 KB chunks from a snapshot taken under the state lock, and the simulator serves the very
  same bytes. `embedded/sim/test_sim.py` drives the simulator over HTTP and parses the JSON for
  real. Add a canned cable to `sim.c` for any new case the page should show.
- **The page** is one file with no build step and no browser dialogs (New session needs two taps).
  Red means faulty, so the bag palette has no red (a test checks).

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
- **Lines meet expander pins only in `kat_line_map()`** (`board_pins.h`), which supports MCP23017
  and TCA9555. **Never map a line to an MCP23017's GPA7 or GPB7:** they're output-only on current
  datasheets, which leaves 14 lines per chip. `lines_init()` refuses a map that breaks this, reuses a
  pin, or names a missing expander. Unmapped lines read as never connected. The breadboard's
  stage 1 maps only the two RJ45 jacks (18 lines on 2 × MCP23017).
- **Pins are placeholders.** `embedded/main/board_pins.h` holds the pin map until KATOO-1's
  generated `pins.h` replaces it (every value is marked TODO).
- **Wi-Fi:** credentials go in `embedded/sdkconfig.secrets` (gitignored; copy
  `sdkconfig.secrets.example`). Without them Katooni runs an open "katooni" access point.
- **Screen assets** are generated and committed, so a build doesn't need Pillow:
  `python3 embedded/tools/make_assets.py --font <SourceCodePro-Bold.ttf>`. The fonts are rendered
  from Source Code Pro (OFL); don't swap in a font that isn't openly licensed.
- **Nothing in `embedded/main/` has run on hardware yet.** It compiles with no warnings.

### Fleet OTA (KATOO-10)

Copied from tinycrypt's CRYPT-14, without the watch approval: **Fleet only triggers an update, and
Katooni is the trust anchor.**

- **Protocol:** commands on `katooni/<thing>/commands`, answers on `katooni/<thing>/responses`.
- **Checks before installing:** the image is downloaded into the spare slot, then refused unless
  - its SHA-256 matches Fleet's,
  - `esp_ota_end` passes (signed by the key that signed the **running** image),
  - it says `katooni` at that version, and
  - it isn't a downgrade.
- **Probation:** a new image must pass `selftest()` (NVS, and the core classifying a known cable)
  and reach IoT Core within 120 s, or it's rolled back.
- **Back-off:** limits as tinycrypt's (deadline, minimum rate, back-off after a failure). A
  repeated URL is ignored with no reply.
- **Signing:** every image is signed (RSA-3072, secure boot v2 scheme, no secure boot).
  - `build.sh` builds, then `sign.sh` signs with the Keychain's `katooni-ota-signing/{env}`.
  - The master copy is in SSM `/katooni/{env}/ota/signing-key`.
  - The public half is committed: `embedded/ota-signing/{env}.pub.pem`. The sandbox key's digest is
    `75663e02…`.
  - **Replacing the key strands every Katooni in the field.** Never `idf.py flash`: it writes an
    unsigned image. Use `flash.sh <port> [thing]`.
- **Per-device settings** live in the `prov` NVS partition, never in the image:
  - contents: Wi-Fi from SSM `/Nakomis/Wifi/*`, endpoint, Thing, certificate and key
  - written by `infra/scripts/provision-device.sh unit-N` into `embedded/certs/<thing>/`
    (gitignored)
  - `sdkconfig.secrets` is the development fallback, and `publish-ota.sh` refuses a build that
    has it.
- **Devices are IoT Things** from `KatooniDeviceStack` (`infra/`). They're listed per environment
  in `infra/devices.json`: a device left out of a deploy has its Thing deleted.
  - Attributes: `project=katooni`, `deviceType=cable-tester`, `capabilities=ota,ota-signed`. That's
    how Fleet finds them, and it makes Fleet offer signed builds only.
  - Policy: connect only as itself, its own topics only.
- **Publishing (by hand):**
  1. Bump `embedded/version.txt` and commit.
  2. Run `embedded/publish-ota.sh x.y.z`. It uploads to `s3://nakomis-firmware-{env}/katooni/`, and
     the first upload is what makes `katooni` appear as a Fleet project.
  3. Update the device from Fleet's UI.

  CI publishing, like tinycrypt's CRYPT-20, isn't set up yet.
- **Infra:** `cd infra && pnpm test`, `pnpm run deploy-sandbox`. Prod is wired up but has no
  devices, and deploys only with Martin's OK.

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

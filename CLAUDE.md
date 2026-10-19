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
| `embedded/` | ESP-IDF 5.5 firmware (to come) |
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

Host-side unit tests for the portable core, with 70% coverage minimum. Commands to be added with
the first firmware work item.

## Architecture diagrams

Source files are in `docs/architecture/*.drawio`. Their SVGs are regenerated on commit by
`.githooks/pre-commit`.

To activate the hook after cloning:

```bash
git config core.hooksPath .githooks
```

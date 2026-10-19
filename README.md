# Katooni — a DIY cable tester that sorts the drawer

<img src="docs/logo.png" alt="Katooni logo" width="200" align="right"/>

## Support

If you find this useful, please consider buying me a coffee:

[![Donate with PayPal](https://www.paypalobjects.com/en_GB/i/btn/btn_donate_SM.gif)](https://www.paypal.com/donate?hosted_button_id=Q3BESC73EWVNN&custom=katooni)

## Table of Contents

<!-- toc -->

- [Overview](#overview)
- [Repository Layout](#repository-layout)
- [Architecture Diagrams](#architecture-diagrams)
- [Support](#support)

<!-- tocstop -->

## Overview

Katooni is an ESP32-S3 box with an "A end" and a "B end". Plug a cable into both ends and it works
out what the cable really is, then tells you which bag to drop it in:

- **USB-C ↔ USB-C, USB-A ↔ USB-C:** per-conductor continuity (power-only, USB 2, full-featured, or
  wired wrong), the cable's e-marker (voltage, current, speed, vendor, rough length), and a
  resistance check that catches cables claiming more than their copper can carry.
- **RJ45 ↔ RJ45:** wiremap (opens, shorts, crossovers, 4-wire leads) and shield continuity.

Identical cables share a "spec fingerprint", so the pile sorts itself into bags. A web page served
by the device lists each bag, its spec and count, and the label text for each one.

**Status:** design only. See [`docs/design.md`](docs/design.md).

## Repository Layout

| Path | What it is |
|---|---|
| `docs/design.md` | The v1 design: goals, non-goals, hardware and firmware approach |
| `docs/architecture/` | Architecture diagrams (draw.io source + generated SVG) |
| `docs/logo-candidates/` | Logo candidates awaiting a verdict |
| `embedded/` | ESP-IDF firmware (to come) |
| `hardware/board/` | Code-generated PCB for JLCPCB assembly (to come) |
| `hardware/case/` | Onshape FeatureScript case (to come) |

## Architecture Diagrams

Diagrams live in `docs/architecture/` as draw.io files. Their SVGs are regenerated on commit by the
pre-commit hook in `.githooks/pre-commit`.

To activate the hook after cloning:

```bash
git config core.hooksPath .githooks
```

## Support

If you find this useful, please consider buying me a coffee:

[![Donate with PayPal](https://www.paypalobjects.com/en_GB/i/btn/btn_donate_SM.gif)](https://www.paypal.com/donate?hosted_button_id=Q3BESC73EWVNN&custom=katooni)

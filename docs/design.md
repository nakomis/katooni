# Katooni design (v1)

Captured from the design conversation on 19 Oct 2026. Nothing is built yet. Any decision marked
*(Claude's call)* is a default that hasn't been argued over, so challenge it freely.

## Why

Martin has a drawer of USB-C, USB-A and RJ45 cables and no idea which ones do what. Which are
power-only? Which carry data, and how fast? What voltage and current will each take? A cheap
AliExpress tester didn't work, and off-the-shelf tools split the job across several gadgets: a
continuity board, a PD analyser for e-markers, and an RJ45 wiremap tester.

**The job is to sort the whole pile in one session.** Every cable gets tested and dropped into a
plastic bag of identical cables. Each bag gets one Dymo label. Cables that turn out to be one-offs
get their own label.

## Goals (v1)

1. **Continuity map.** USB-C ↔ USB-C, USB-A ↔ USB-C, USB-C/USB-A ↔ micro-B and RJ45 ↔ RJ45,
   plugged into an A end (USB-C, USB-A 3.0, RJ45) and a B end (USB-C, micro-B, RJ45). Find opens, shorts and swapped conductors.
   - USB: classify as power-only, USB 2, full-featured (SuperSpeed/USB4 lanes present) or miswired.
     The conductors are VBUS, GND, CC, VCONN, D+/D−, four SuperSpeed pairs, SBU1/2 and the shield.
   - RJ45: straight vs crossover, reversed pairs, 4-wire leads (100 Mb max), and STP vs UTP
     (shield continuity).
2. **E-marker read.** Supply VCONN and send a USB PD *Discover Identity* on SOP'. Decode the cable
   VDO: maximum VBUS voltage (20/30/40/50 V), current (3/5 A), USB speed (USB 2 to USB4 Gen4),
   active or passive, vendor ID, and the latency field (a rough length). No e-marker means
   "3 A / 20 V (60 W) by spec".
3. **Resistance check (lie detector).** Drive about 0.25 A round the VBUS/GND loop and measure the
   drop with a Kelvin (4-wire) connection at the far end. This gives the actual loop resistance
   and a rough estimate of the conductor gauge. Flag any cable whose e-marker claims more than its
   copper can carry.
4. **Auto-bagging.** Each cable gets a spec fingerprint:
   - the connector pair
   - which conductors are present
   - the e-marker fields ("no e-marker" counts as a value)
   - a resistance band and a length band

   The first cable with a new fingerprint opens a new bag ("New bag: D"). Each matching cable
   after that shows "Bag D ×4". The bands are configurable, so two identical cables don't split
   across bags and a thin cable can't hide in a good cable's bag. Faulty cables are flagged rather
   than bagged.
5. **Plug and go.** No buttons in the normal flow. Katooni notices a cable arriving, tests it in
   about 2 s, beeps, and shows the bag on the screen: a big letter on a panel in that bag's colour, so
   you can sort by colour across the room, plus the reason and a small conductor map.
6. **Session web page,** served by the S3 over Wi-Fi at `katooni.local`. It shows:
   - each bag, with its full spec and count
   - one-cable bags, flagged "label this one individually"
   - the details of the last cable (pin map, e-marker dump, resistance), so you can see *why* it
     landed where it did
   - label text ready to type into the Dymo app, e.g. `C–C 240W 40G 1.0m`
   - a "new session" control (there are no physical buttons apart from BOOT)

   Wi-Fi is only for this page and OTA updates. Credentials live in a gitignored secrets file, as
   tinycrypt's do; if the home network isn't reachable, Katooni falls back to its own access point.
   The session (bags, counts, fingerprints) is kept in NVS, so a power blip doesn't lose it.
7. **Bag colours:** a fixed palette of 12 distinct colours. The bag letter is always shown, so
   colours may repeat after 12 bags.

## Phase II (not v1)

- Print labels straight to Martin's Bluetooth Dymo (plastic tape) over BLE. Martin intends to
  reverse-engineer the Dymo protocol himself; check GitHub for prior work first.
- Per-cable IDs and history, plus "is this cable #17?" by matching fingerprints.
- More connectors (micro-B is already in v1, at the B end).
- One-end-only RJ45 length estimate from pair capacitance (about 50 pF/m).

## Non-goals

These would need lab gear, so Katooni won't attempt them:

- True bandwidth or signal-integrity testing. It reports "lanes present + e-marker claim +
  measured length" instead.
- Ethernet category. Read it off the jacket.
- Split-pair detection, which needs crosstalk measurement.
- Throughput tests. The S3's USB is full-speed only (12 Mbps).

## Anti-goals

- A tester that gives a confident answer it can't back up. When it can't tell something, it
  says so.
- Splitting obviously identical cables into different bags. That defeats the point of the
  session.

## Hardware *(Claude's call, following tinycrypt)*

**Decided (Martin, 19 Oct 2026): fully JLCPCB-assembled, no protoboard and no hand-built
prototype.** The board needs about 67 drive/sense lines (one per conductor per port), which is
roughly 550–650 joints and 70–80 wires on protoboard. Hand assembly is limited to the house USB
cable and the case.

- **Decided with Martin (19 Oct 2026):** all ports on **one edge**, the A cluster on the left and
  the B cluster on the right, so you plug both ends in from the front; **micro-B at the B end**;
  **shielded RJ45 jacks** at both ends (STP vs UTP); a **4-layer** board; a box of about
  100 × 70 × 25 mm.
- **ESP32-S3-MINI-1** with ESP-IDF 5.5. Power and flashing through the house USB connector
  (JST SH 1.0 5-pin; see CLAUDE.md in `~/` and tinycrypt `hardware/key-board/README.md`). That
  gives 5 V at 1 A max, shared between the board and the load test.
- **I²C GPIO expanders** drive every conductor at the A end in turn and read every conductor at
  the B end, through series protection resistors.
- **FUSB302B** USB PD PHY for SOP' messaging. Its built-in VCONN switches power the cable's e-marker.
- **INA226** for the Kelvin resistance measurement, plus a switched load of about 0.25 A. (The
  sketch said 0.5 A, then 1 A; 0.25 A is Claude's call when designing the board: the INA226's
  ±81.92 mV input then covers up to ~330 mΩ per conductor, enough for a long thin cable, at
  10 µΩ resolution, and it leaves the 1 A budget comfortable.) `hardware/board/DESIGN.md` has the
  circuit.
- **2.0" IPS colour TFT, ST7789, 240×320** (decided by Martin, 19 Oct 2026, after seeing the logo at
  240 px; about £3–4 bare from LCSC). SPI, with the ribbon in a JLC-soldered ZIF FPC connector, so
  no hand-soldering, and a transistor for the backlight. It shows the Katooni logo as a boot splash.
- A buzzer.
- **Board:** generated by code, as tinycrypt's `key-board` is: Python `build.py` → KiCad → DRC →
  JLCPCB BOM, CPL and gerbers, assembled by JLCPCB.
- **Case:** Onshape FeatureScript, with both port clusters along the front edge.
- **240 MHz voltage caution** (see `~/CLAUDE.md`): the work here is light, so 160 MHz is fine.

## Firmware *(Claude's call)*

- Keep the hardware-independent logic in a portable core so it builds and is tested on the Mac,
  as tinycrypt does: classification, e-marker VDO decoding, fingerprinting and bagging.
- Board drivers (expanders, FUSB302B, INA226, display) sit behind small interfaces.

## Naming

Microcontrollers are named after younglings. **Katooni** is the Tholothian youngling from *The Clone
Wars* "Young Jedi" arc. Zatt was considered but is already pencilled in twice: HAND-26 (the hit
consumer) and the kitchen radiator cap.

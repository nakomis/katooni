#!/usr/bin/env python3
"""Board generator: circuit -> placement -> (route) -> board + DRC -> fab outputs.

    ./.venv/bin/python build.py --board main [--route] [--step] [--passes N]

Output goes to out/<board>/. Fab outputs (gerbers zip, BOM, CPL, renders, pins.h) are written
only when DRC has no errors and no unconnected items. Cloned from tinycrypt's key-board generator."""
import argparse
import json
import subprocess
from pathlib import Path

from board import katooni_board as kb
from board.cleanup import prune
from board.fab import export_step, fab, write_bom, write_cpl
from board.gnd import gnd_fanout, stitch_vias
from board.kicad_pcb import write_board
from board.netlist import check
from board.route import route

HERE = Path(__file__).resolve().parent
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"

# Ordered with JLCPCB "PCB Color: Black" (which gives white silkscreen). Gerbers carry no
# colour; this only makes the KiCad renders match the order.
MASK, SILK = "Black", "White"
LAYERS = 4
PLANES = [("GND", "In1.Cu")]          # the router's view: In2.Cu is a third signal layer
POURS = PLANES + [("GND", "F.Cu"), ("GND", "B.Cu")]    # KiCad's: GND pours on the outer layers too
BOARDS = {"main": "katooni-main"}


def _expected(v):
    """DRC errors that are intended. Each USB-C receptacle's front shell pegs (pads 24 and 25) go
    in L-shaped plated slots, which the footprint draws as two overlapping oval-slot pads; KiCad
    measures each pad's ring against the other's hole and reports 0 mm. JLCPCB routes the union."""
    return v["type"] == "annular_width" and all(
        any(f"PTH pad {n} [" in i["description"] and i["description"].endswith(f"of {ref}")
            for n in ("24", "25") for ref in ("J2", "J5")) for i in v.get("items", []))


def drc(board, out):
    """Run DRC (refilling zones and saving them into the board); return (errors, warnings)."""
    subprocess.run([KICAD_CLI, "pcb", "drc", "--refill-zones", "--save-board", "--format", "json",
                    "--severity-all", "-o", str(out / "drc.json"), str(board)], check=True,
                   capture_output=True)
    d = json.loads((out / "drc.json").read_text())
    bad = [f"unconnected: {' / '.join(i['description'] for i in u['items'])}" for u in d["unconnected_items"]]
    bad += [f"{v['type']}: {v['description']} "
            f"[{'; '.join(i['description'] for i in v.get('items', []))}]"
            for v in d["violations"] if v["severity"] == "error" and not _expected(v)]
    warn = [v for v in d["violations"] if v["severity"] != "error"]
    return bad, warn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--board", choices=sorted(BOARDS), default="main")
    ap.add_argument("--route", action="store_true")
    ap.add_argument("--passes", type=int, default=100)
    ap.add_argument("--step", action="store_true",
                    help="also fetch the 3D models and write the STEP model and a 3D render")
    ap.add_argument("--allow-drc-errors", action="store_true",
                    help="write the board and report DRC, but skip the fab outputs instead of failing")
    a = ap.parse_args()
    stem = BOARDS[a.board]
    out = HERE / "out" / a.board
    tmp = out / "tmp"
    tmp.mkdir(parents=True, exist_ok=True)
    c = kb.build_katooni()
    problems = check(c)
    if problems:
        raise SystemExit("\n".join(problems))
    p = kb.place_katooni(c)
    clash = kb.overlaps(p)
    if clash:
        raise SystemExit(f"placement overlaps: {clash}")
    # GND is never routed: every SMD GND pad gets its own via into the In1.Cu plane first.
    fan_t, fan_v, misses = gnd_fanout(c, p, keepouts=[kb.ANTENNA_KEEPOUT])
    print(f"GND fanout: {len(fan_v)} vias; no room beside {misses or 'none'}")
    tracks, vias = list(fan_t), list(fan_v)
    if a.route:
        routed_tracks, routed = route(c, p, tmp, passes=a.passes, layers=LAYERS, route_area=kb.route_area(),
                                      planes=PLANES, power_nets=kb.POWER_NETS, skip_nets=("GND",),
                                      fixed_tracks=fan_t, fixed_vias=fan_v,
                                      log="/tmp/claude-501/katooni-board-route.log")
        tracks, vias = prune(c, p, fan_t + routed_tracks, fan_v + routed, keep=fan_v)
        stitch = stitch_vias(c, p, tracks, vias, keepouts=[kb.ANTENNA_KEEPOUT])
        print(f"stitching: {len(stitch)} GND vias")
        vias += stitch
    board = out / f"{stem}.kicad_pcb"
    notes, rects = kb.fab_notes()
    write_board(c, p, board, tracks, vias, layers=LAYERS, silk=kb.silk(c, p) + notes,
                mask=MASK, silkscreen=SILK, keepouts=kb.keepouts(), user_rects=rects, planes=POURS,
                hide_refs=set(c.instances),          # too dense for designators; the BOM and CPL carry them
                pad_extra=kb.pad_extra())
    (out / "pins.h").write_text(kb.pins_h())
    bad, warn = drc(board, out)
    kinds = {}
    for v in warn:
        kinds[v["type"]] = kinds.get(v["type"], 0) + 1
    print(f"wrote {board}: {len(bad)} DRC errors/unconnected, {len(warn)} warnings {kinds}")
    if bad:      # kicad-cli exits 0 even when DRC fails
        summary = {}
        for b in bad:
            summary[b.split(":")[0]] = summary.get(b.split(":")[0], 0) + 1
        print(f"DRC problems by kind: {summary}")
        (out / "drc-problems.txt").write_text("\n".join(bad) + "\n")
        if a.allow_drc_errors:
            print(f"fab outputs skipped; the list is in {out / 'drc-problems.txt'}")
            return
        raise SystemExit("DRC failed:\n" + "\n".join(bad[:40]))
    write_bom(c, out / "bom.csv")
    write_cpl(c, p, out / "cpl.csv")
    fab(board, out, layers=LAYERS)
    if a.step:
        subprocess.run([str(HERE / "fetch_models.sh")], check=True)     # resumable, skips fetched parts
        print(f"STEP: {export_step(board, out)}")
    print(f"fab outputs in {out}: bom.csv, cpl.csv, pins.h, {stem}-gerbers.zip, renders; "
          f"order with solder mask {MASK}, silkscreen {SILK}")


if __name__ == "__main__":
    main()

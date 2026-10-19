"""Ground for a 4-layer board whose In1.Cu is a solid GND plane and whose outer layers carry GND
pours: each SMD GND pad gets a short fanout track to its own via into the plane before routing (so
the router never has to find them, which it did badly: about a third were left open), and
stitching vias tie the pours to the plane after routing. Through-hole GND pads reach the plane
through their own barrels."""
import math

from board.kicad_pcb import Track, Via
from board.placement import rotate

VIA_R = 0.3            # the 0.6 mm via's radius
FANOUT_W = 0.25        # fanout track width
CLEAR = 0.2            # clearance kept to other nets' copper (the rules say 0.15)


def pad_boxes(circuit, placement):
    """(net, kind, xmin, ymin, xmax, ymax) for every copper pad (kind 'thru_hole', 'np_thru_hole'
    or 'smd'), in board coordinates."""
    out = []
    for ref, inst in circuit.instances.items():
        x, y, rot = placement.at[ref]
        for pad in inst.part.footprint.pads:
            dx, dy = rotate(pad.x, pad.y, rot)
            w, h = (pad.h, pad.w) if round(pad.rot + rot) % 180 == 90 else (pad.w, pad.h)
            out.append((circuit.net_of(ref, pad.number), pad.kind, x + dx - w / 2, y + dy - h / 2,
                        x + dx + w / 2, y + dy + h / 2))
    return out


def _box_dist(x, y, b):
    x0, y0, x1, y1 = b[-4:]
    return math.hypot(max(x0 - x, 0, x - x1), max(y0 - y, 0, y - y1))


def _seg_box_dist(ax, ay, bx, by, b, steps=12):
    return min(_box_dist(ax + (bx - ax) * k / steps, ay + (by - ay) * k / steps, b) for k in range(steps + 1))


def _seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    t = 0.0 if vx == vy == 0 else max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy)))
    return math.hypot(px - ax - t * vx, py - ay - t * vy)


def _seg_seg_dist(ax, ay, bx, by, cx, cy, dx, dy):
    """Distance between segments AB and CD: 0 if they cross, else the nearest endpoint distance."""
    def orient(px, py, qx, qy, rx, ry):
        return (qx - px) * (ry - py) - (qy - py) * (rx - px)
    o1, o2 = orient(ax, ay, bx, by, cx, cy), orient(ax, ay, bx, by, dx, dy)
    o3, o4 = orient(cx, cy, dx, dy, ax, ay), orient(cx, cy, dx, dy, bx, by)
    if o1 * o2 < 0 and o3 * o4 < 0:
        return 0.0
    return min(_seg_dist(cx, cy, ax, ay, bx, by), _seg_dist(dx, dy, ax, ay, bx, by),
               _seg_dist(ax, ay, cx, cy, dx, dy), _seg_dist(bx, by, cx, cy, dx, dy))


def _inside(x, y, outline, keepouts, margin):
    x0, y0, x1, y1 = outline
    if not (x0 + margin <= x <= x1 - margin and y0 + margin <= y <= y1 - margin):
        return False
    return not any(kx0 - margin <= x <= kx1 + margin and ky0 - margin <= y <= ky1 + margin
                   for kx0, ky0, kx1, ky1 in keepouts)


def gnd_fanout(circuit, placement, keepouts=(), net="GND"):
    """(tracks, vias, misses): a via and a short F.Cu track for every SMD pad on 'net', placed just
    outside the pad, away from its part's centre where possible, clear of every other net's pads
    and of the other fanout vias. 'misses' lists the pads no position could be found for."""
    pads = pad_boxes(circuit, placement)
    others = [b for b in pads if b[0] != net]
    tracks, vias, misses = [], [], []
    for ref, inst in circuit.instances.items():
        px, py, rot = placement.at[ref]
        for pad in inst.part.footprint.pads:
            if pad.kind != "smd" or circuit.net_of(ref, pad.number) != net:
                continue
            dx, dy = rotate(pad.x, pad.y, rot)
            cx, cy = px + dx, py + dy
            w, h = (pad.h, pad.w) if round(pad.rot + rot) % 180 == 90 else (pad.w, pad.h)
            # directions: away from the part's centre first, then the four axes
            ax, ay = dx, dy
            n = math.hypot(ax, ay)
            dirs = ([(ax / n, ay / n)] if n > 1e-6 else []) + [(1, 0), (-1, 0), (0, 1), (0, -1)]
            done = False
            for ux, uy in dirs:
                reach = abs(ux) * w / 2 + abs(uy) * h / 2      # pad edge in that direction
                for extra in (0.0, 0.3, 0.6, 1.0, 1.5):
                    d = reach + VIA_R + 0.1 + extra
                    vx, vy = round(cx + ux * d, 3), round(cy + uy * d, 3)
                    if not _inside(vx, vy, placement.outline, keepouts, 0.6):
                        continue
                    if any(_box_dist(vx, vy, b) < VIA_R + CLEAR for b in others):
                        continue
                    if any(math.hypot(vx - v.x, vy - v.y) < 2 * VIA_R + CLEAR for v in vias):
                        continue
                    if any(_seg_box_dist(cx, cy, vx, vy, b) < FANOUT_W / 2 + CLEAR for b in others):
                        continue
                    if any(_seg_dist(v.x, v.y, cx, cy, vx, vy) < VIA_R + FANOUT_W / 2 + CLEAR for v in vias):
                        continue
                    tracks.append(Track(net, "F.Cu", round(cx, 4), round(cy, 4), vx, vy, FANOUT_W))
                    vias.append(Via(net, vx, vy))
                    done = True
                    break
                if done:
                    break
            if not done:
                misses.append((ref, pad.number))
    return tracks, vias, misses


def escape_vias(circuit, placement, ref, pad_numbers, toward=(0.0, 1.0), taken=(), clear=0.16):
    """(tracks, vias, misses): a short F.Cu track from each named pad of 'ref' to its own via,
    stepping 'toward' (a unit vector in the part's frame) from the pad, with small sideways offsets,
    so the router can take each signal from the via on any layer. For a USB-C receptacle's B row,
    whose pads can only leave forward under the body: the router stranded one of them on every
    arrangement tried. 'taken' are vias already placed (the GND fanout), to keep clear of."""
    x0, y0, rot = placement.at[ref]
    near = lambda b: abs((b[2] + b[4]) / 2 - x0) < 8 and abs((b[3] + b[5]) / 2 - y0) < 8
    pads = [b for b in pad_boxes(circuit, placement) if near(b)]
    taken = [v for v in taken if abs(v.x - x0) < 8 and abs(v.y - y0) < 8]
    inst = circuit.instances[ref]
    ux, uy = rotate(toward[0], toward[1], rot)
    sx, sy = -uy, ux                                       # sideways, in board frame
    tracks, vias, misses = [], [], []
    placed = list(taken)
    for number in pad_numbers:
        pad = next(q for q in inst.part.footprint.pads if q.number == number)
        net = circuit.net_of(ref, number)
        dx, dy = rotate(pad.x, pad.y, rot)
        cx, cy = x0 + dx, y0 + dy
        reach = abs(toward[0]) * pad.w / 2 + abs(toward[1]) * pad.h / 2
        others = [b for b in pads if b[0] != net]
        done = False
        for step in (0.45, 0.85, 1.25, 1.65, 2.05, 2.45, 2.85):
            for side in (0.0, 0.25, -0.25, 0.5, -0.5, 0.75, -0.75, 1.0, -1.0, 1.25, -1.25, 1.5, -1.5, 1.75, -1.75, 2.0, -2.0):
                d = reach + VIA_R - 0.15 + step
                vx, vy = round(cx + ux * d + sx * side, 3), round(cy + uy * d + sy * side, 3)
                if any(_box_dist(vx, vy, b) < VIA_R + clear for b in others):
                    continue
                if any(math.hypot(vx - v.x, vy - v.y) < 2 * VIA_R + clear for v in placed):
                    continue
                if any(_seg_box_dist(cx, cy, vx, vy, b) < 0.075 + clear for b in others):
                    continue
                if any(_seg_dist(v.x, v.y, cx, cy, vx, vy) < VIA_R + 0.075 + clear for v in placed if v.net != net):
                    continue
                if any(_seg_seg_dist(t.x1, t.y1, t.x2, t.y2, cx, cy, vx, vy) < 0.15 + clear
                       for t in tracks if t.net != net):
                    continue
                if any(_seg_dist(vx, vy, t.x1, t.y1, t.x2, t.y2) < VIA_R + 0.075 + clear
                       for t in tracks if t.net != net):
                    continue
                tracks.append(Track(net, "F.Cu", round(cx, 4), round(cy, 4), vx, vy, 0.15))
                v = Via(net, vx, vy)
                vias.append(v)
                placed.append(v)
                done = True
                break
            if done:
                break
        if not done:
            misses.append((ref, number))
    return tracks, vias, misses


def escape_vias_any_order(circuit, placement, ref, pad_numbers, tries=400, **kw):
    """escape_vias with the pads tried in many orders (the given one, then deterministic shuffles),
    returning the first result that places every pad, or else the one with fewest misses. The
    greedy placement depends a lot on order: a via taken early can wall in a neighbour's track."""
    import random
    rng = random.Random(1)
    order = list(pad_numbers)
    best = None
    for k in range(tries):
        res = escape_vias(circuit, placement, ref, order, **kw)
        if best is None or len(res[2]) < len(best[2]):
            best = res
        if not res[2]:
            break
        order = list(pad_numbers)
        rng.shuffle(order)
    return best


def stitch_vias(circuit, placement, tracks, vias, keepouts=(), pitch=2.5, clear=0.4, net="GND"):
    """Stitching vias on a grid wherever a via clears every other net's pad, every track and every
    via by 'clear', so the outer pours meet the plane."""
    pads = [b for b in pad_boxes(circuit, placement) if b[0] != net]
    placed = list(vias)
    out = []
    x0, y0, x1, y1 = placement.outline
    y = y0 + 1.0
    while y < y1 - 1.0:
        x = x0 + 1.0
        while x < x1 - 1.0:
            ok = _inside(x, y, placement.outline, keepouts, 0.8)
            ok = ok and all(_box_dist(x, y, b) >= VIA_R + clear for b in pads)
            ok = ok and all(_seg_dist(x, y, t.x1, t.y1, t.x2, t.y2) >= VIA_R + t.width / 2 + clear
                            for t in tracks if t.net != net)
            ok = ok and all(math.hypot(x - v.x, y - v.y) >= 2 * VIA_R + clear for v in placed)
            if ok:
                v = Via(net, round(x, 3), round(y, 3))
                out.append(v)
                placed.append(v)
            x += pitch
        y += pitch
    return out

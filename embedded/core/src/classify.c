#include "katooni/classify.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MINUS "\xE2\x88\x92"
#define NDASH "\xE2\x80\x93"

/* Cable wiring, from the USB Type-C spec's cable assembly tables. SuperSpeed TX at one plug is RX at
 * the other; SBU1 and SBU2 cross over; there's one D+/D− pair (A6/A7) and one CC wire (A5). The
 * plug's B5 is VCONN, which stays inside the plug (Ra or the e-marker), so it's never a net. */
static const kat_net_t NETS_CC[] = {
    {"VBUS", KAT_G_VBUS, KAT_C_VBUS, KAT_C_VBUS},
    {"GND", KAT_G_GND, KAT_C_GND, KAT_C_GND},
    {"CC", KAT_G_CC, KAT_C_CC1, KAT_C_CC1},
    {"D+", KAT_G_D, KAT_C_DP1, KAT_C_DP1},
    {"D" MINUS, KAT_G_D, KAT_C_DN1, KAT_C_DN1},
    {"SSTX1+", KAT_G_SS, KAT_C_TX1P, KAT_C_RX1P},
    {"SSTX1" MINUS, KAT_G_SS, KAT_C_TX1N, KAT_C_RX1N},
    {"SSRX1+", KAT_G_SS, KAT_C_RX1P, KAT_C_TX1P},
    {"SSRX1" MINUS, KAT_G_SS, KAT_C_RX1N, KAT_C_TX1N},
    {"SSTX2+", KAT_G_SS, KAT_C_TX2P, KAT_C_RX2P},
    {"SSTX2" MINUS, KAT_G_SS, KAT_C_TX2N, KAT_C_RX2N},
    {"SSRX2+", KAT_G_SS, KAT_C_RX2P, KAT_C_TX2P},
    {"SSRX2" MINUS, KAT_G_SS, KAT_C_RX2N, KAT_C_TX2N},
    {"SBU1", KAT_G_SBU, KAT_C_SBU1, KAT_C_SBU2},
    {"SBU2", KAT_G_SBU, KAT_C_SBU2, KAT_C_SBU1},
    {"shield", KAT_G_SHIELD, KAT_C_SHIELD, KAT_C_SHIELD},
};

/* USB 3 Standard-A to Type-C: the A plug's SSRX meets the C plug's TX1, and its SSTX meets RX1. */
static const kat_net_t NETS_AC[] = {
    {"VBUS", KAT_G_VBUS, KAT_A_VBUS, KAT_C_VBUS},
    {"GND", KAT_G_GND, KAT_A_GND, KAT_C_GND},
    {"D+", KAT_G_D, KAT_A_DP, KAT_C_DP1},
    {"D" MINUS, KAT_G_D, KAT_A_DN, KAT_C_DN1},
    {"SSRX+", KAT_G_SS, KAT_A_SSRXP, KAT_C_TX1P},
    {"SSRX" MINUS, KAT_G_SS, KAT_A_SSRXN, KAT_C_TX1N},
    {"SSTX+", KAT_G_SS, KAT_A_SSTXP, KAT_C_RX1P},
    {"SSTX" MINUS, KAT_G_SS, KAT_A_SSTXN, KAT_C_RX1N},
    {"shield", KAT_G_SHIELD, KAT_A_SHIELD, KAT_C_SHIELD},
};

static const kat_net_t NETS_CM[] = {
    {"VBUS", KAT_G_VBUS, KAT_C_VBUS, KAT_M_VBUS},
    {"GND", KAT_G_GND, KAT_C_GND, KAT_M_GND},
    {"D+", KAT_G_D, KAT_C_DP1, KAT_M_DP},
    {"D" MINUS, KAT_G_D, KAT_C_DN1, KAT_M_DN},
    {"shield", KAT_G_SHIELD, KAT_C_SHIELD, KAT_M_SHIELD},
};

static const kat_net_t NETS_AM[] = {
    {"VBUS", KAT_G_VBUS, KAT_A_VBUS, KAT_M_VBUS},
    {"GND", KAT_G_GND, KAT_A_GND, KAT_M_GND},
    {"D+", KAT_G_D, KAT_A_DP, KAT_M_DP},
    {"D" MINUS, KAT_G_D, KAT_A_DN, KAT_M_DN},
    {"shield", KAT_G_SHIELD, KAT_A_SHIELD, KAT_M_SHIELD},
};

static const kat_net_t NETS_RJ[] = {
    {"1", KAT_G_PIN, 0, 0}, {"2", KAT_G_PIN, 1, 1}, {"3", KAT_G_PIN, 2, 2}, {"4", KAT_G_PIN, 3, 3},
    {"5", KAT_G_PIN, 4, 4}, {"6", KAT_G_PIN, 5, 5}, {"7", KAT_G_PIN, 6, 6}, {"8", KAT_G_PIN, 7, 7},
    {"shield", KAT_G_SHIELD, KAT_R_SHIELD, KAT_R_SHIELD},
};

#define N(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

typedef struct {
    kat_kind_t kind;
    kat_port_t a, b;
    const kat_net_t *nets;
    uint8_t count;
} kind_spec_t;

static const kind_spec_t KINDS[] = {
    {KAT_KIND_C_C, KAT_PORT_A_USBC, KAT_PORT_B_USBC, NETS_CC, N(NETS_CC)},
    {KAT_KIND_A_C, KAT_PORT_A_USBA, KAT_PORT_B_USBC, NETS_AC, N(NETS_AC)},
    {KAT_KIND_C_MICRO, KAT_PORT_A_USBC, KAT_PORT_B_MICROB, NETS_CM, N(NETS_CM)},
    {KAT_KIND_A_MICRO, KAT_PORT_A_USBA, KAT_PORT_B_MICROB, NETS_AM, N(NETS_AM)},
    {KAT_KIND_RJ45, KAT_PORT_A_RJ45, KAT_PORT_B_RJ45, NETS_RJ, N(NETS_RJ)},
};

static const kind_spec_t *spec_for(kat_kind_t kind)
{
    for (unsigned i = 0; i < N(KINDS); i++)
        if (KINDS[i].kind == kind) return &KINDS[i];
    return NULL;
}

const kat_net_t *kat_kind_nets(kat_kind_t kind, uint8_t *count)
{
    const kind_spec_t *k = spec_for(kind);
    if (count) *count = k ? k->count : 0;
    return k ? k->nets : NULL;
}

const char *kat_kind_name(kat_kind_t kind)
{
    switch (kind) {
    case KAT_KIND_C_C: return "C" NDASH "C";
    case KAT_KIND_A_C: return "A" NDASH "C";
    case KAT_KIND_C_MICRO: return "C" NDASH "\xC2\xB5" "B";
    case KAT_KIND_A_MICRO: return "A" NDASH "\xC2\xB5" "B";
    case KAT_KIND_RJ45: return "RJ45";
    case KAT_KIND_MULTIPLE: return "several cables";
    case KAT_KIND_NONE: break;
    }
    return "none";
}

const char *kat_class_name(kat_class_t cls)
{
    switch (cls) {
    case KAT_CLASS_FAULTY: return "faulty";
    case KAT_CLASS_POWER_ONLY: return "power only";
    case KAT_CLASS_USB2: return "USB 2";
    case KAT_CLASS_SUPERSPEED: return "SuperSpeed";
    case KAT_CLASS_RJ45_8WIRE: return "8-wire";
    case KAT_CLASS_RJ45_4WIRE: return "4-wire (100 Mb max)";
    case KAT_CLASS_RJ45_CROSSOVER: return "crossover";
    case KAT_CLASS_RJ45_CROSSOVER_GIG: return "gigabit crossover";
    case KAT_CLASS_RJ45_ROLLOVER: return "rollover (console)";
    case KAT_CLASS_NONE: break;
    }
    return "none";
}

static void add_fault(kat_continuity_t *r, const char *fmt, ...)
{
    if (r->fault_count >= KAT_MAX_FAULTS) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->faults[r->fault_count++], KAT_FAULT_LEN, fmt, ap);
    va_end(ap);
}

static int pin_at(kat_port_t port, int pin, int flip)
{
    return kat_port(port)->type == KAT_PTYPE_USBC && flip ? kat_usbc_mirror(pin) : pin;
}

static kat_kind_t detect(const kat_scan_t *s)
{
    int pairs = 0;
    kat_port_t pa = KAT_PORT_COUNT, pb = KAT_PORT_COUNT;
    for (int a = 0; a < KAT_PORT_COUNT; a++) {
        const kat_port_info_t *A = kat_port((kat_port_t)a);
        if (A->end != KAT_END_A) continue;
        for (int b = 0; b < KAT_PORT_COUNT; b++) {
            const kat_port_info_t *B = kat_port((kat_port_t)b);
            if (B->end != KAT_END_B) continue;
            bool joined = false;
            for (int i = 0; i < A->line_count && !joined; i++)
                for (int j = 0; j < B->line_count && !joined; j++)
                    joined = kat_scan_joined(s, A->first_line + i, B->first_line + j);
            if (joined) {
                pairs++;
                pa = (kat_port_t)a;
                pb = (kat_port_t)b;
            }
        }
    }
    if (pairs == 0) return KAT_KIND_NONE;
    if (pairs > 1) return KAT_KIND_MULTIPLE;
    for (unsigned i = 0; i < N(KINDS); i++)
        if (KINDS[i].a == pa && KINDS[i].b == pb) return KINDS[i].kind;
    return KAT_KIND_MULTIPLE;
}

/* ---- USB ---------------------------------------------------------------------------------- */

enum { NET_VBUS = 0, NET_GND = 1 };   /* every USB table starts VBUS, GND */

typedef struct {
    int8_t role[KAT_LINE_COUNT];     /* net index, or -1 */
    int8_t partner[KAT_LINE_COUNT];  /* for a line outside every net: a net it may sit on (Ra, Rp, Rd) */
} roles_t;

static void usb_roles(const kind_spec_t *k, int fa, int fb, roles_t *ro)
{
    memset(ro->role, -1, sizeof ro->role);
    memset(ro->partner, -1, sizeof ro->partner);
    for (int n = 0; n < k->count; n++) {
        ro->role[kat_line(k->a, pin_at(k->a, k->nets[n].a_pin, fa))] = (int8_t)n;
        ro->role[kat_line(k->b, pin_at(k->b, k->nets[n].b_pin, fb))] = (int8_t)n;
    }
    /* Each USB-C plug's VCONN contact may sit on GND through Ra (1 kΩ) or an e-marker. */
    if (kat_port(k->a)->type == KAT_PTYPE_USBC)
        ro->partner[kat_line(k->a, pin_at(k->a, KAT_C_CC2, fa))] = NET_GND;
    if (kat_port(k->b)->type == KAT_PTYPE_USBC)
        ro->partner[kat_line(k->b, pin_at(k->b, KAT_C_CC2, fb))] = NET_GND;
    /* A legacy A-to-C cable has Rp (56 kΩ) from CC to VBUS; a C-to-micro-B one has Rd (5.1 kΩ) to GND. */
    if (k->kind == KAT_KIND_A_C) ro->partner[kat_line(k->b, pin_at(k->b, KAT_C_CC1, fb))] = NET_VBUS;
    if (k->kind == KAT_KIND_C_MICRO) ro->partner[kat_line(k->a, pin_at(k->a, KAT_C_CC1, fa))] = NET_GND;
    if (kat_port(k->b)->type == KAT_PTYPE_MICROB) ro->partner[kat_line(k->b, KAT_M_ID)] = NET_GND;
}

static const char *line_desc(int line, char *buf, size_t n)
{
    kat_port_t p = kat_line_port(line);
    const kat_port_info_t *pi = kat_port(p);
    snprintf(buf, n, "%c-end %s", pi->end == KAT_END_A ? 'A' : 'B', kat_pin_name(pi->type, kat_line_pin(line)));
    return buf;
}

static void usb_eval(const kat_scan_t *s, const kind_spec_t *k, int fa, int fb, kat_continuity_t *r)
{
    roles_t ro;
    usb_roles(k, fa, fb, &ro);
    memset(r, 0, sizeof *r);
    r->kind = k->kind;
    r->flip_a = (uint8_t)fa;
    r->flip_b = (uint8_t)fb;
    r->net_count = k->count;

    const kat_port_info_t *P[2] = {kat_port(k->a), kat_port(k->b)};
    int lines[KAT_LINE_COUNT], nl = 0;
    for (int e = 0; e < 2; e++)
        for (int i = 0; i < P[e]->line_count; i++) lines[nl++] = P[e]->first_line + i;

    /* Shorts and crossed wires: a connected group should hold one net (plus anything allowed to sit on it). */
    bool done[KAT_LINE_COUNT] = {false};
    for (int i = 0; i < nl; i++) {
        int root = kat_scan_root(s, lines[i]);
        if (done[root]) continue;
        done[root] = true;
        int keys[KAT_LINE_COUNT], nk = 0, unused[KAT_LINE_COUNT], nu = 0, members[KAT_LINE_COUNT], nm = 0;
        for (int j = 0; j < nl; j++) {
            int l = lines[j];
            if (kat_scan_root(s, l) != root) continue;
            members[nm++] = l;
            int key;
            if (ro.role[l] >= 0) key = k->nets[ro.role[l]].group == KAT_G_SHIELD ? NET_GND : ro.role[l];
            else if (ro.partner[l] >= 0) key = ro.partner[l];
            else {
                unused[nu++] = l;
                continue;
            }
            bool seen = false;
            for (int q = 0; q < nk; q++) seen |= keys[q] == key;
            if (!seen) keys[nk++] = key;
        }
        char b1[48], b2[48];
        if (nk >= 2) {
            if (nm == 2 && nu == 0 && ro.role[members[0]] >= 0 && ro.role[members[1]] >= 0 &&
                kat_port(kat_line_port(members[0]))->end != kat_port(kat_line_port(members[1]))->end) {
                int la = kat_port(kat_line_port(members[0]))->end == KAT_END_A ? members[0] : members[1];
                int lb = la == members[0] ? members[1] : members[0];
                add_fault(r, "crossed: A-end %s reaches B-end %s", k->nets[ro.role[la]].name,
                          k->nets[ro.role[lb]].name);
            } else if (nk == 2) {
                add_fault(r, "%s shorted to %s", k->nets[keys[0]].name, k->nets[keys[1]].name);
            } else {
                add_fault(r, "%s, %s and %d more shorted together", k->nets[keys[0]].name,
                          k->nets[keys[1]].name, nk - 2);
            }
        }
        if (nu > 0 && (nk > 0 || nu > 1)) {
            if (nk > 0)
                add_fault(r, "%s shorted to %s", k->nets[keys[0]].name, line_desc(unused[0], b1, sizeof b1));
            else
                add_fault(r, "%s shorted to %s", line_desc(unused[0], b1, sizeof b1),
                          line_desc(unused[1], b2, sizeof b2));
        }
    }

    int d = 0, dn = 0, cc = 0, cc_n = 0, vbus = 0, gnd = 0;
    for (int n = 0; n < k->count; n++) {
        const kat_net_t *net = &k->nets[n];
        int la = kat_line(k->a, pin_at(k->a, net->a_pin, fa));
        int lb = kat_line(k->b, pin_at(k->b, net->b_pin, fb));
        bool on = kat_scan_joined(s, la, lb);
        if (on) r->present |= 1u << n;
        switch (net->group) {
        case KAT_G_VBUS: vbus = on; break;
        case KAT_G_GND: gnd = on; break;
        case KAT_G_CC: cc_n++; cc += on; break;
        case KAT_G_D: dn++; d += on; break;
        case KAT_G_SS: r->ss_total++; r->ss_wires += on; break;
        case KAT_G_SHIELD: r->shield = on; break;
        default: break;
        }
    }
    if (!vbus) add_fault(r, "VBUS open");
    if (!gnd) add_fault(r, "GND open");
    if (cc_n && !cc) add_fault(r, "CC open: chargers won't switch VBUS on");
    if (d == 1) {
        for (int n = 0; n < k->count; n++)
            if (k->nets[n].group == KAT_G_D && !(r->present & (1u << n)))
                add_fault(r, "%s open: data needs both D+ and D" MINUS, k->nets[n].name);
    }
    if (r->ss_wires > 0 && r->ss_wires < r->ss_total)
        add_fault(r, "only %d of %d SuperSpeed wires", r->ss_wires, r->ss_total);

    for (int e = 0; e < 2; e++) {
        if (P[e]->type != KAT_PTYPE_USBC) continue;
        kat_port_t port = e ? k->b : k->a;
        int vconn = kat_line(port, pin_at(port, KAT_C_CC2, e ? fb : fa));
        r->ra |= kat_scan_joined(s, vconn, kat_line(port, KAT_C_GND));
    }
    if (P[1]->type == KAT_PTYPE_MICROB)
        r->otg_id = kat_scan_joined(s, kat_line(k->b, KAT_M_ID), kat_line(k->b, KAT_M_GND));

    if (r->fault_count) r->cls = KAT_CLASS_FAULTY;
    else if (r->ss_total && r->ss_wires == r->ss_total) r->cls = KAT_CLASS_SUPERSPEED;
    else if (dn && d == dn) r->cls = KAT_CLASS_USB2;
    else r->cls = KAT_CLASS_POWER_ONLY;
}

static int popcount(uint32_t v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

static void classify_usb(const kat_scan_t *s, const kind_spec_t *k, kat_continuity_t *out)
{
    int fa_max = kat_port(k->a)->type == KAT_PTYPE_USBC ? 2 : 1;
    int fb_max = kat_port(k->b)->type == KAT_PTYPE_USBC ? 2 : 1;
    int best = -1000000;
    kat_continuity_t r;
    for (int fa = 0; fa < fa_max; fa++)
        for (int fb = 0; fb < fb_max; fb++) {
            usb_eval(s, k, fa, fb, &r);
            int score = popcount(r.present) * 16 - r.fault_count;
            if (score > best) {
                best = score;
                *out = r;
            }
        }
}

/* ---- RJ45 --------------------------------------------------------------------------------- */

static bool map_is(const uint8_t *m, const uint8_t *want)
{
    return memcmp(m, want, 8) == 0;
}

static void classify_rj45(const kat_scan_t *s, kat_continuity_t *r)
{
    memset(r, 0, sizeof *r);
    r->kind = KAT_KIND_RJ45;
    r->net_count = N(NETS_RJ);
    int la0 = kat_line(KAT_PORT_A_RJ45, 0), lb0 = kat_line(KAT_PORT_B_RJ45, 0);
    int ash = kat_line(KAT_PORT_A_RJ45, KAT_R_SHIELD), bsh = kat_line(KAT_PORT_B_RJ45, KAT_R_SHIELD);

    bool done[KAT_LINE_COUNT] = {false};
    for (int i = 0; i < 2 * KAT_R_COUNT; i++) {
        int l = i < KAT_R_COUNT ? la0 + i : lb0 + i - KAT_R_COUNT;
        int root = kat_scan_root(s, l);
        if (done[root]) continue;
        done[root] = true;
        int ap[8], na = 0, bp[8], nb = 0;
        bool sh = false;
        for (int p = 0; p < 8; p++) {
            if (kat_scan_root(s, la0 + p) == root) ap[na++] = p;
            if (kat_scan_root(s, lb0 + p) == root) bp[nb++] = p;
        }
        sh = kat_scan_root(s, ash) == root || kat_scan_root(s, bsh) == root;
        if (na > 1 || nb > 1) {
            int x = na > 1 ? ap[0] : bp[0], y = na > 1 ? ap[1] : bp[1];
            add_fault(r, "pins %d and %d shorted (%c end)", x + 1, y + 1, na > 1 ? 'A' : 'B');
        } else if (na == 1 && nb == 1) {
            r->rj45_map[ap[0]] = (uint8_t)(bp[0] + 1);
        }
        if (sh && (na || nb)) add_fault(r, "shield shorted to pin %d", (na ? ap[0] : bp[0]) + 1);
    }
    r->shield = kat_scan_joined(s, ash, bsh);
    for (int p = 0; p < 8 && r->shield; p++)
        r->shield = !kat_scan_joined(s, ash, la0 + p) && !kat_scan_joined(s, ash, lb0 + p);
    for (int p = 0; p < 8; p++)
        if (r->rj45_map[p] == p + 1) r->present |= 1u << p;
    if (r->shield) r->present |= 1u << 8;

    static const uint8_t STRAIGHT[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const uint8_t FOUR[8] = {1, 2, 3, 0, 0, 6, 0, 0};
    static const uint8_t CROSS[8] = {3, 6, 1, 4, 5, 2, 7, 8};
    static const uint8_t CROSS4[8] = {3, 6, 1, 0, 0, 2, 0, 0};
    static const uint8_t CROSSG[8] = {3, 6, 1, 7, 8, 2, 4, 5};
    static const uint8_t ROLL[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    const uint8_t *m = r->rj45_map;

    if (r->fault_count) r->cls = KAT_CLASS_FAULTY;
    else if (map_is(m, STRAIGHT)) r->cls = KAT_CLASS_RJ45_8WIRE;
    else if (map_is(m, FOUR)) r->cls = KAT_CLASS_RJ45_4WIRE;
    else if (map_is(m, CROSS) || map_is(m, CROSS4)) r->cls = KAT_CLASS_RJ45_CROSSOVER;
    else if (map_is(m, CROSSG)) r->cls = KAT_CLASS_RJ45_CROSSOVER_GIG;
    else if (map_is(m, ROLL)) r->cls = KAT_CLASS_RJ45_ROLLOVER;
    else {
        static const int PAIRS[4][2] = {{0, 1}, {2, 5}, {3, 4}, {6, 7}};
        bool reported[8] = {false};
        for (int q = 0; q < 4; q++) {
            int a = PAIRS[q][0], b = PAIRS[q][1];
            if (m[a] == b + 1 && m[b] == a + 1) {
                add_fault(r, "pair %d/%d reversed", a + 1, b + 1);
                reported[a] = reported[b] = true;
            }
        }
        for (int p = 0; p < 8; p++) {
            if (reported[p] || m[p] == p + 1) continue;
            if (!m[p]) add_fault(r, "pin %d open", p + 1);
            else add_fault(r, "pin %d goes to pin %d", p + 1, m[p]);
        }
        r->cls = KAT_CLASS_FAULTY;
    }
}

void kat_classify(const kat_scan_t *scan, kat_continuity_t *out)
{
    memset(out, 0, sizeof *out);
    out->kind = detect(scan);
    if (out->kind == KAT_KIND_NONE) return;
    if (out->kind == KAT_KIND_MULTIPLE) {
        add_fault(out, "more than one cable plugged in");
        return;
    }
    if (out->kind == KAT_KIND_RJ45) classify_rj45(scan, out);
    else classify_usb(scan, spec_for(out->kind), out);
}

#include "katooni/simcable.h"

static int at(kat_port_t port, int pin, int flip)
{
    if (kat_port(port)->type == KAT_PTYPE_USBC && flip) pin = kat_usbc_mirror(pin);
    return kat_line(port, pin);
}

void kat_sim_usb(kat_scan_t *s, const kat_sim_usb_t *c)
{
    static const struct {
        kat_kind_t kind;
        kat_port_t a, b;
    } PORTS[] = {
        {KAT_KIND_C_C, KAT_PORT_A_USBC, KAT_PORT_B_USBC},
        {KAT_KIND_A_C, KAT_PORT_A_USBA, KAT_PORT_B_USBC},
        {KAT_KIND_C_MICRO, KAT_PORT_A_USBC, KAT_PORT_B_MICROB},
        {KAT_KIND_A_MICRO, KAT_PORT_A_USBA, KAT_PORT_B_MICROB},
    };
    kat_scan_clear(s);
    kat_port_t pa = KAT_PORT_COUNT, pb = KAT_PORT_COUNT;
    for (unsigned i = 0; i < sizeof PORTS / sizeof PORTS[0]; i++)
        if (PORTS[i].kind == c->kind) {
            pa = PORTS[i].a;
            pb = PORTS[i].b;
        }
    if (pa == KAT_PORT_COUNT) return;
    uint8_t n;
    const kat_net_t *nets = kat_kind_nets(c->kind, &n);
    for (int i = 0; i < n; i++) {
        kat_group_t g = nets[i].group;
        if ((g == KAT_G_SS && c->no_ss) || (g == KAT_G_D && c->no_d) || (g == KAT_G_SBU && c->no_sbu) ||
            (g == KAT_G_SHIELD && c->no_shield) || (g == KAT_G_CC && c->no_cc))
            continue;
        kat_scan_join(s, at(pa, nets[i].a_pin, c->flip_a), at(pb, nets[i].b_pin, c->flip_b));
    }
    if (c->ra) {
        if (kat_port(pa)->type == KAT_PTYPE_USBC)
            kat_scan_join(s, at(pa, KAT_C_CC2, c->flip_a), kat_line(pa, KAT_C_GND));
        if (kat_port(pb)->type == KAT_PTYPE_USBC)
            kat_scan_join(s, at(pb, KAT_C_CC2, c->flip_b), kat_line(pb, KAT_C_GND));
    }
    if (c->rp && c->kind == KAT_KIND_A_C) kat_scan_join(s, at(pb, KAT_C_CC1, c->flip_b), kat_line(pb, KAT_C_VBUS));
    if (c->rd && c->kind == KAT_KIND_C_MICRO) kat_scan_join(s, at(pa, KAT_C_CC1, c->flip_a), kat_line(pa, KAT_C_GND));
    if (c->otg && kat_port(pb)->type == KAT_PTYPE_MICROB) kat_scan_join(s, kat_line(pb, KAT_M_ID), kat_line(pb, KAT_M_GND));
}

void kat_sim_rj45(kat_scan_t *s, const uint8_t map[8], bool shield)
{
    kat_scan_clear(s);
    for (int i = 0; i < 8; i++)
        if (map[i]) kat_scan_join(s, kat_line(KAT_PORT_A_RJ45, i), kat_line(KAT_PORT_B_RJ45, map[i] - 1));
    if (shield) kat_scan_join(s, kat_line(KAT_PORT_A_RJ45, KAT_R_SHIELD), kat_line(KAT_PORT_B_RJ45, KAT_R_SHIELD));
}

int kat_sim_emarker(const kat_sim_em_t *e, uint32_t out[6])
{
    /* VDM header: PD SID, structured, version 2.1 (1.0 for PD 2.0), ACK, Discover Identity. */
    uint32_t ver = e->pd2 ? 0 : (1u << 13) | (1u << 11);
    out[0] = 0xFF000000u | (1u << 15) | ver | (1u << 6) | 1u;
    out[1] = ((e->active ? 4u : 3u) << 27) | (3u << 21) | e->vid;    /* SOP' type; Type-C plug */
    out[2] = 0x00001234u;                                               /* XID */
    out[3] = ((uint32_t)e->pid << 16) | 0x0100u;                        /* bcdDevice 1.00 */
    out[4] = (1u << 28) | (2u << 24) | ((e->active ? 3u : 0u) << 21) | (2u << 18) |
             ((uint32_t)e->epr << 17) | ((uint32_t)(e->latency & 0xF) << 13) |
             ((e->active ? 2u : 0u) << 11) | ((uint32_t)(e->vbus_code & 3) << 9) |
             ((uint32_t)(e->current_code & 3) << 5) | ((e->active ? 1u : 0u) << 4) | (e->speed & 7u);
    if (!e->active) return 5;
    /* Active VDO2: copper, redriver, USB4 supported (0), USB 2 and 3.2 supported (0), two lanes. */
    out[5] = (70u << 24) | (85u << 16) | (1u << 3) | 1u;
    return 6;
}

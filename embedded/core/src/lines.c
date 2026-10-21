#include "katooni/lines.h"

#include <stddef.h>

#define C0 0
#define A0 (C0 + KAT_C_COUNT)
#define RA0 (A0 + KAT_A_COUNT)
#define CB0 (RA0 + KAT_R_COUNT)
#define M0 (CB0 + KAT_C_COUNT)
#define RB0 (M0 + KAT_M_COUNT)

static const kat_port_info_t PORTS[KAT_PORT_COUNT] = {
    [KAT_PORT_A_USBC] = {KAT_END_A, KAT_PTYPE_USBC, "A-end USB-C", "C", C0, KAT_C_COUNT},
    [KAT_PORT_A_USBA] = {KAT_END_A, KAT_PTYPE_USBA, "A-end USB-A", "A", A0, KAT_A_COUNT},
    [KAT_PORT_A_RJ45] = {KAT_END_A, KAT_PTYPE_RJ45, "A-end RJ45", "RJ45", RA0, KAT_R_COUNT},
    [KAT_PORT_B_USBC] = {KAT_END_B, KAT_PTYPE_USBC, "B-end USB-C", "C", CB0, KAT_C_COUNT},
    [KAT_PORT_B_MICROB] = {KAT_END_B, KAT_PTYPE_MICROB, "B-end micro-B", "\xC2\xB5" "B", M0, KAT_M_COUNT},
    [KAT_PORT_B_RJ45] = {KAT_END_B, KAT_PTYPE_RJ45, "B-end RJ45", "RJ45", RB0, KAT_R_COUNT},
};

_Static_assert(RB0 + KAT_R_COUNT == KAT_LINE_COUNT, "line numbering");

const kat_port_info_t *kat_port(kat_port_t port)
{
    return (unsigned)port < KAT_PORT_COUNT ? &PORTS[port] : NULL;
}

int kat_line(kat_port_t port, int pin)
{
    const kat_port_info_t *p = kat_port(port);
    if (!p || pin < 0 || pin >= p->line_count) return -1;
    return p->first_line + pin;
}

kat_port_t kat_line_port(int line)
{
    for (int i = 0; i < KAT_PORT_COUNT; i++)
        if (line >= PORTS[i].first_line && line < PORTS[i].first_line + PORTS[i].line_count)
            return (kat_port_t)i;
    return KAT_PORT_COUNT;
}

int kat_line_pin(int line)
{
    kat_port_t p = kat_line_port(line);
    return p == KAT_PORT_COUNT ? -1 : line - PORTS[p].first_line;
}

static const char *const C_NAMES[KAT_C_COUNT] = {
    "VBUS", "GND", "CC1 (A5)", "CC2 (B5)", "D+ (A6)", "D\xE2\x88\x92 (A7)", "D+ (B6)",
    "D\xE2\x88\x92 (B7)", "TX1+ (A2)", "TX1\xE2\x88\x92 (A3)", "RX1+ (B11)", "RX1\xE2\x88\x92 (B10)",
    "TX2+ (B2)", "TX2\xE2\x88\x92 (B3)", "RX2+ (A11)", "RX2\xE2\x88\x92 (A10)", "SBU1 (A8)",
    "SBU2 (B8)", "shield",
};
static const char *const A_NAMES[KAT_A_COUNT] = {
    "VBUS", "D\xE2\x88\x92", "D+", "GND", "SSRX\xE2\x88\x92", "SSRX+", "SSTX\xE2\x88\x92", "SSTX+", "shield",
};
static const char *const M_NAMES[KAT_M_COUNT] = {"VBUS", "D\xE2\x88\x92", "D+", "ID", "GND", "shield"};
static const char *const R_NAMES[KAT_R_COUNT] = {"1", "2", "3", "4", "5", "6", "7", "8", "shield"};

const char *kat_pin_name(kat_port_type_t type, int pin)
{
    switch (type) {
    case KAT_PTYPE_USBC: return pin >= 0 && pin < KAT_C_COUNT ? C_NAMES[pin] : "?";
    case KAT_PTYPE_USBA: return pin >= 0 && pin < KAT_A_COUNT ? A_NAMES[pin] : "?";
    case KAT_PTYPE_MICROB: return pin >= 0 && pin < KAT_M_COUNT ? M_NAMES[pin] : "?";
    case KAT_PTYPE_RJ45: return pin >= 0 && pin < KAT_R_COUNT ? R_NAMES[pin] : "?";
    }
    return "?";
}

int kat_usbc_mirror(int pin)
{
    static const int8_t M[KAT_C_COUNT] = {
        [KAT_C_VBUS] = KAT_C_VBUS, [KAT_C_GND] = KAT_C_GND,
        [KAT_C_CC1] = KAT_C_CC2, [KAT_C_CC2] = KAT_C_CC1,
        [KAT_C_DP1] = KAT_C_DP2, [KAT_C_DN1] = KAT_C_DN2,
        [KAT_C_DP2] = KAT_C_DP1, [KAT_C_DN2] = KAT_C_DN1,
        [KAT_C_TX1P] = KAT_C_TX2P, [KAT_C_TX1N] = KAT_C_TX2N,
        [KAT_C_TX2P] = KAT_C_TX1P, [KAT_C_TX2N] = KAT_C_TX1N,
        [KAT_C_RX1P] = KAT_C_RX2P, [KAT_C_RX1N] = KAT_C_RX2N,
        [KAT_C_RX2P] = KAT_C_RX1P, [KAT_C_RX2N] = KAT_C_RX1N,
        [KAT_C_SBU1] = KAT_C_SBU2, [KAT_C_SBU2] = KAT_C_SBU1,
        [KAT_C_SHIELD] = KAT_C_SHIELD,
    };
    return pin >= 0 && pin < KAT_C_COUNT ? M[pin] : -1;
}

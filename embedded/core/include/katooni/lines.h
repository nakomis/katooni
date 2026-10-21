/* Katooni's ports and the conductor lines behind them.
 *
 * Every conductor of every port is its own line, driven and sensed by the I²C expanders: no two
 * connector pins share a line, and none is tied to board ground (the PD and load tests switch the
 * A-end USB-C's GND to board ground only while they run). A line is "connected" to another when the
 * scan sees them joined through the cable.
 *
 * Lines are numbered globally: A-end ports first, then B-end ports, in kat_port_t order. */
#ifndef KATOONI_LINES_H
#define KATOONI_LINES_H

#include <stdint.h>

typedef enum { KAT_END_A = 0, KAT_END_B = 1 } kat_end_t;

typedef enum {
    KAT_PTYPE_USBC,
    KAT_PTYPE_USBA,     /* USB 3 Standard-A receptacle (9 pins) */
    KAT_PTYPE_MICROB,   /* USB 2 micro-B (5 pins) */
    KAT_PTYPE_RJ45,     /* shielded 8P8C */
} kat_port_type_t;

typedef enum {
    KAT_PORT_A_USBC,
    KAT_PORT_A_USBA,
    KAT_PORT_A_RJ45,
    KAT_PORT_B_USBC,
    KAT_PORT_B_MICROB,
    KAT_PORT_B_RJ45,
    KAT_PORT_COUNT,
} kat_port_t;

/* USB-C receptacle lines. GND (A1/A12/B1/B12) and VBUS (A4/A9/B4/B9) are one line each; everything
 * else that the plug can land on is kept separate, so the scan sees which way up each plug is. */
enum {
    KAT_C_VBUS, KAT_C_GND,
    KAT_C_CC1,  /* A5 */
    KAT_C_CC2,  /* B5 */
    KAT_C_DP1,  /* A6 */
    KAT_C_DN1,  /* A7 */
    KAT_C_DP2,  /* B6 */
    KAT_C_DN2,  /* B7 */
    KAT_C_TX1P, /* A2 */
    KAT_C_TX1N, /* A3 */
    KAT_C_RX1P, /* B11 */
    KAT_C_RX1N, /* B10 */
    KAT_C_TX2P, /* B2 */
    KAT_C_TX2N, /* B3 */
    KAT_C_RX2P, /* A11 */
    KAT_C_RX2N, /* A10 */
    KAT_C_SBU1, /* A8 */
    KAT_C_SBU2, /* B8 */
    KAT_C_SHIELD,
    KAT_C_COUNT,
};

/* USB 3 Standard-A receptacle; GND_DRAIN (pin 7) is joined to GND (pin 4) on the board. */
enum {
    KAT_A_VBUS, KAT_A_DN, KAT_A_DP, KAT_A_GND,
    KAT_A_SSRXN, KAT_A_SSRXP, KAT_A_SSTXN, KAT_A_SSTXP,
    KAT_A_SHIELD,
    KAT_A_COUNT,
};

enum { KAT_M_VBUS, KAT_M_DN, KAT_M_DP, KAT_M_ID, KAT_M_GND, KAT_M_SHIELD, KAT_M_COUNT };

/* RJ45 pins 1-8 are lines 0-7. */
enum { KAT_R_SHIELD = 8, KAT_R_COUNT = 9 };

#define KAT_LINE_COUNT (2 * KAT_C_COUNT + KAT_A_COUNT + KAT_M_COUNT + 2 * KAT_R_COUNT)

typedef struct {
    kat_end_t end;
    kat_port_type_t type;
    const char *name;   /* "A-end USB-C" */
    const char *short_name; /* "C", "A", "µB", "RJ45" for labels */
    uint8_t first_line;
    uint8_t line_count;
} kat_port_info_t;

const kat_port_info_t *kat_port(kat_port_t port);

/* Global line number of a port's pin (one of the per-type enums above). */
int kat_line(kat_port_t port, int pin);

/* Which port a global line belongs to, and its pin within that port. */
kat_port_t kat_line_port(int line);
int kat_line_pin(int line);

/* "A-end USB-C CC1 (A5)" etc. Returns a static string. */
const char *kat_pin_name(kat_port_type_t type, int pin);

/* USB-C: the receptacle pin that a plug pin lands on when the plug is upside down. */
int kat_usbc_mirror(int pin);

#endif

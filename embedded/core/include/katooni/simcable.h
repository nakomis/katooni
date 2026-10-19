/* Synthetic cables for the tests and the Mac simulator: the scan a given cable would produce, and the
 * e-marker reply it would send. None of this is captured from real cables. */
#ifndef KATOONI_SIMCABLE_H
#define KATOONI_SIMCABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "katooni/classify.h"
#include "katooni/scan.h"

typedef struct {
    kat_kind_t kind;            /* a USB kind */
    uint8_t flip_a, flip_b;     /* plug upside down at that end (USB-C ends only) */
    bool no_ss, no_d, no_sbu, no_shield, no_cc;
    bool ra;                    /* VCONN pulled to GND (Ra) at each USB-C plug */
    bool rp, rd;                /* legacy A-to-C Rp to VBUS; C-to-micro-B Rd to GND, read as joined */
    bool otg;                   /* micro-B ID tied to GND */
} kat_sim_usb_t;

void kat_sim_usb(kat_scan_t *s, const kat_sim_usb_t *spec);

/* RJ45: map[i] is the B-end pin (1-8) that A-end pin i+1 reaches, 0 for none. */
void kat_sim_rj45(kat_scan_t *s, const uint8_t map[8], bool shield);

typedef struct {
    uint16_t vid, pid;
    uint8_t vbus_code;      /* 0 = 20 V, 3 = 50 V */
    uint8_t current_code;   /* 1 = 3 A, 2 = 5 A */
    uint8_t speed;          /* kat_usb_speed_t */
    uint8_t latency;        /* 1 = <10 ns */
    bool epr, active, pd2;
} kat_sim_em_t;

/* Writes the Discover Identity ACK (VDM header first); returns the number of objects (5 or 6). */
int kat_sim_emarker(const kat_sim_em_t *e, uint32_t out[6]);

#endif

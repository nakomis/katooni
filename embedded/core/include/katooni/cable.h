/* One tested cable: continuity, e-marker and resistance together, and the words for it. */
#ifndef KATOONI_CABLE_H
#define KATOONI_CABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "katooni/classify.h"
#include "katooni/emarker.h"
#include "katooni/resistance.h"

typedef struct {
    kat_continuity_t cont;
    kat_emarker_t em;
    kat_resistance_t res;
} kat_cable_t;

/* Everything that decides which bag a cable goes in. Plain data, so it can be stored as it is. */
typedef struct {
    uint8_t kind, cls;
    uint8_t speed, latency;
    uint32_t present;
    uint8_t rj45_map[8];
    uint8_t shield, otg_id, em_present, em_active, em_epr, em_captive, res_measured, claims_too_much;
    uint16_t vid, pid;
    uint16_t max_vbus_mv, max_current_ma;
    uint32_t loop_mohm;
} kat_fingerprint_t;

void kat_fingerprint(const kat_cable_t *c, kat_fingerprint_t *fp);

/* Same bag? Everything must be equal except the loop resistance, which may differ by the larger of
 * tol_mohm and tol_pct per cent of the bag's first cable. */
bool kat_fingerprint_match(const kat_fingerprint_t *bag, const kat_fingerprint_t *c, uint16_t tol_mohm,
                           uint8_t tol_pct);

/* A cable that is faulty is never bagged. */
bool kat_cable_faulty(const kat_cable_t *c);

/* The Dymo label text, e.g. "C–C 240W 40G ≤1m" or "RJ45 8-wire STP". */
void kat_label(const kat_fingerprint_t *fp, char *buf, size_t n);

/* One line for the screen: the first fault, the copper warning, or what the cable is. */
void kat_reason(const kat_cable_t *c, char *buf, size_t n);

#endif

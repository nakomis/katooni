/* Continuity scan -> what sort of cable this is, and what's wrong with it (KATOO-2). */
#ifndef KATOONI_CLASSIFY_H
#define KATOONI_CLASSIFY_H

#include <stdbool.h>
#include <stdint.h>

#include "katooni/scan.h"

typedef enum {
    KAT_KIND_NONE,      /* nothing joins the two ends */
    KAT_KIND_C_C,
    KAT_KIND_A_C,
    KAT_KIND_C_MICRO,
    KAT_KIND_A_MICRO,
    KAT_KIND_RJ45,
    KAT_KIND_MULTIPLE,  /* more than one cable in, or a pairing of ports no cable can make */
} kat_kind_t;

typedef enum {
    KAT_CLASS_NONE,
    KAT_CLASS_FAULTY,
    KAT_CLASS_POWER_ONLY,
    KAT_CLASS_USB2,
    KAT_CLASS_SUPERSPEED,          /* every SuperSpeed wire the cable type can carry */
    KAT_CLASS_RJ45_8WIRE,          /* straight, all eight */
    KAT_CLASS_RJ45_4WIRE,          /* straight, 1/2/3/6 only: 100 Mb at most */
    KAT_CLASS_RJ45_CROSSOVER,      /* 1-3, 2-6 swapped */
    KAT_CLASS_RJ45_CROSSOVER_GIG,  /* and 4-7, 5-8 */
    KAT_CLASS_RJ45_ROLLOVER,       /* 1-8, 2-7...: a console cable */
} kat_class_t;

typedef enum { KAT_G_VBUS, KAT_G_GND, KAT_G_CC, KAT_G_D, KAT_G_SS, KAT_G_SBU, KAT_G_SHIELD, KAT_G_PIN } kat_group_t;

/* One conductor a cable of a given kind should carry, from the A end's point of view. For USB-C
 * ends the pins are where the plug's contacts land when it's the right way up. */
typedef struct {
    const char *name;
    kat_group_t group;
    int8_t a_pin;
    int8_t b_pin;
} kat_net_t;

#define KAT_MAX_FAULTS 8
#define KAT_FAULT_LEN 56

typedef struct {
    kat_kind_t kind;
    kat_class_t cls;
    uint8_t flip_a, flip_b;    /* USB-C plug upside down at that end */
    uint8_t net_count;
    uint32_t present;          /* bit n: net n of kat_kind_nets(kind) is continuous */
    uint8_t ss_wires, ss_total;
    uint8_t rj45_map[8];       /* RJ45: B-end pin (1-8) each A-end pin reaches, 0 if none */
    bool shield;               /* shield continuous end to end */
    bool ra;                   /* a USB-C VCONN pin sits on GND through Ra: e-marker likely */
    bool otg_id;               /* micro-B ID tied to GND (an OTG lead) */
    uint8_t fault_count;
    char faults[KAT_MAX_FAULTS][KAT_FAULT_LEN];
} kat_continuity_t;

void kat_classify(const kat_scan_t *scan, kat_continuity_t *out);

const kat_net_t *kat_kind_nets(kat_kind_t kind, uint8_t *count);
const char *kat_kind_name(kat_kind_t kind);    /* "C–C", "A–C", "C–µB", "A–µB", "RJ45" */
const char *kat_class_name(kat_class_t cls);   /* "USB 2", "SuperSpeed", ... */

#endif

/* A continuity scan: which lines the cable joins.
 *
 * The firmware drives one line at a time and records every other line that follows it; the core only
 * needs the resulting pairs. kat_scan_join() is symmetric and the core works on the connected groups,
 * so a scan that only records each pair once is fine. */
#ifndef KATOONI_SCAN_H
#define KATOONI_SCAN_H

#include <stdbool.h>
#include <stdint.h>

#include "katooni/lines.h"

typedef struct {
    uint8_t parent[KAT_LINE_COUNT];   /* union-find; build with the functions below */
} kat_scan_t;

void kat_scan_clear(kat_scan_t *s);
void kat_scan_join(kat_scan_t *s, int line_a, int line_b);
int kat_scan_root(const kat_scan_t *s, int line);
bool kat_scan_joined(const kat_scan_t *s, int line_a, int line_b);

/* True when any line at the A end is joined to any line at the B end, i.e. a cable is in. */
bool kat_scan_any_cable(const kat_scan_t *s);

/* Two scans see the same connections (used to debounce cable arrival). */
bool kat_scan_equal(const kat_scan_t *a, const kat_scan_t *b);

#endif

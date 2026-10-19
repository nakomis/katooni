/* The resistance check (KATOO-4): a known current round the VBUS/GND loop, the drop sensed at the
 * far end, and whether the copper could carry what the cable claims.
 *
 * The USB Type-C spec allows a cable 500 mV of IR drop on VBUS and 250 mV on GND at its rated
 * current, so the loop may be at most 750 mV / 3 A = 250 mΩ for a 3 A cable and 750 mV / 5 A =
 * 150 mΩ for a 5 A one. No length is needed for that test. */
#ifndef KATOONI_RESISTANCE_H
#define KATOONI_RESISTANCE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t test_ma;          /* load current; 500 mA keeps inside the house connector's 1 A */
    uint16_t fixture_mohm;     /* Katooni's own share of the loop (sockets, traces), from calibration */
    uint16_t limit_3a_mohm;    /* 250 */
    uint16_t limit_5a_mohm;    /* 150 */
} kat_res_config_t;

typedef struct {
    bool measured;
    uint32_t loop_mohm;        /* cable only: VBUS out plus GND back, fixture removed */
    uint8_t copper_amps;       /* 5, 3, or 0 (doesn't meet even the 3 A drop limit) */
    bool claims_too_much;      /* rated (e-marker, or the 3 A default) above what the copper meets */
    uint8_t awg;               /* rough conductor gauge when the length is known, else 0 */
} kat_resistance_t;

void kat_res_config_default(kat_res_config_t *cfg);

/* drop_uv: loop voltage drop in µV at current_ua; claimed_ma: the cable's rating; metres: length if
 * known (from the e-marker's latency), else 0. A non-positive current means no measurement. */
void kat_resistance_compute(int32_t drop_uv, int32_t current_ua, uint16_t claimed_ma, int metres,
                            const kat_res_config_t *cfg, kat_resistance_t *out);

/* AWG whose copper resistance per metre is closest to mohm_per_m, or 0 if out of range. */
uint8_t kat_awg_for(uint32_t mohm_per_m);

#endif

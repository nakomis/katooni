#include "katooni/resistance.h"

#include <string.h>

void kat_res_config_default(kat_res_config_t *cfg)
{
    cfg->test_ma = 500;
    cfg->fixture_mohm = 0;
    cfg->limit_3a_mohm = 250;
    cfg->limit_5a_mohm = 150;
}

/* Annealed copper at 20 °C, mΩ per metre. */
static const struct {
    uint8_t awg;
    uint16_t mohm_per_m;
} COPPER[] = {
    {16, 13}, {18, 21}, {20, 33}, {22, 53}, {24, 84}, {26, 134}, {28, 213}, {30, 339}, {32, 538}, {34, 856},
};

uint8_t kat_awg_for(uint32_t mohm_per_m)
{
    const unsigned n = sizeof COPPER / sizeof COPPER[0];
    if (mohm_per_m < COPPER[0].mohm_per_m / 2 || mohm_per_m > COPPER[n - 1].mohm_per_m * 2) return 0;
    unsigned best = 0;
    for (unsigned i = 1; i < n; i++) {
        /* Compare on a log scale: gauges are a geometric series. */
        uint64_t r = mohm_per_m, a = COPPER[i].mohm_per_m, b = COPPER[best].mohm_per_m;
        uint64_t da = r > a ? r * 1000 / a : a * 1000 / r;
        uint64_t db = r > b ? r * 1000 / b : b * 1000 / r;
        if (da < db) best = i;
    }
    return COPPER[best].awg;
}

void kat_resistance_compute(int32_t drop_uv, int32_t current_ua, uint16_t claimed_ma, int metres,
                            const kat_res_config_t *cfg, kat_resistance_t *out)
{
    memset(out, 0, sizeof *out);
    if (current_ua <= 0 || drop_uv < 0) return;
    out->measured = true;
    int64_t mohm = (int64_t)drop_uv * 1000 / current_ua - cfg->fixture_mohm;
    out->loop_mohm = mohm < 0 ? 0 : (uint32_t)mohm;
    out->copper_amps = out->loop_mohm <= cfg->limit_5a_mohm ? 5 : out->loop_mohm <= cfg->limit_3a_mohm ? 3 : 0;
    out->claims_too_much = claimed_ma > out->copper_amps * 1000u;
    if (metres > 0) out->awg = kat_awg_for(out->loop_mohm / 2 / (uint32_t)metres);
}

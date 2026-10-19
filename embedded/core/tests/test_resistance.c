#include "check.h"
#include "katooni/resistance.h"

int main(void)
{
    kat_res_config_t cfg;
    kat_res_config_default(&cfg);
    CHECK_INT(cfg.test_ma, 500);
    kat_resistance_t r;

    /* 50 mV at 500 mA is 100 mΩ: fine for 5 A. */
    kat_resistance_compute(50000, 500000, 5000, 0, &cfg, &r);
    CHECK(r.measured);
    CHECK_INT(r.loop_mohm, 100);
    CHECK_INT(r.copper_amps, 5);
    CHECK(!r.claims_too_much);
    CHECK_INT(r.awg, 0);

    /* 155 mΩ: a 5 A claim is a lie, 3 A would be fine. */
    kat_resistance_compute(77500, 500000, 5000, 0, &cfg, &r);
    CHECK_INT(r.loop_mohm, 155);
    CHECK_INT(r.copper_amps, 3);
    CHECK(r.claims_too_much);
    kat_resistance_compute(77500, 500000, 3000, 0, &cfg, &r);
    CHECK(!r.claims_too_much);

    /* 300 mΩ fails even 3 A. */
    kat_resistance_compute(150000, 500000, 3000, 0, &cfg, &r);
    CHECK_INT(r.copper_amps, 0);
    CHECK(r.claims_too_much);

    /* The fixture's share comes off, and can't take the cable below zero. */
    cfg.fixture_mohm = 40;
    kat_resistance_compute(50000, 500000, 5000, 0, &cfg, &r);
    CHECK_INT(r.loop_mohm, 60);
    kat_resistance_compute(5000, 500000, 5000, 0, &cfg, &r);
    CHECK_INT(r.loop_mohm, 0);
    cfg.fixture_mohm = 0;

    /* No current, no measurement. */
    kat_resistance_compute(50000, 0, 5000, 0, &cfg, &r);
    CHECK(!r.measured);
    kat_resistance_compute(-1, 500000, 5000, 0, &cfg, &r);
    CHECK(!r.measured);

    /* 1 m, 168 mΩ loop: 84 mΩ per conductor per metre is 24 AWG. 2 m at 212 mΩ is 26 AWG. */
    kat_resistance_compute(84000, 500000, 3000, 1, &cfg, &r);
    CHECK_INT(r.awg, 24);
    kat_resistance_compute(268000, 500000, 3000, 2, &cfg, &r);
    CHECK_INT(r.awg, 26);
    CHECK_INT(kat_awg_for(21), 18);
    CHECK_INT(kat_awg_for(500), 32);
    CHECK_INT(kat_awg_for(2), 0);
    CHECK_INT(kat_awg_for(5000), 0);

    CHECK_DONE();
}

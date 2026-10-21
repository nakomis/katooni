#include <stdlib.h>

#include "check.h"
#include "katooni/session.h"
#include "katooni/simcable.h"

static kat_config_t cfg;

/* A whole cable: continuity from a sim spec, an e-marker (or none), and a loop resistance. */
static void make(kat_cable_t *c, const kat_sim_usb_t *u, const kat_sim_em_t *e, uint32_t loop_mohm)
{
    kat_scan_t s;
    kat_sim_usb(&s, u);
    kat_classify(&s, &c->cont);
    if (e) {
        uint32_t o[6];
        kat_emarker_decode(o, kat_sim_emarker(e, o), &c->em);
    } else {
        kat_emarker_none(&c->em);
    }
    kat_resistance_compute((int32_t)loop_mohm * 500, 500000, c->em.max_current_ma,
                           kat_latency_metres(c->em.latency), &cfg.res, &c->res);
}

static void make_rj(kat_cable_t *c, const uint8_t m[8], bool shield)
{
    kat_scan_t s;
    kat_sim_rj45(&s, m, shield);
    kat_classify(&s, &c->cont);
    kat_emarker_none(&c->em);
    memset(&c->res, 0, sizeof c->res);
}

static const kat_sim_usb_t CC_FULL = {.kind = KAT_KIND_C_C, .ra = true};
static const kat_sim_em_t EM_240 = {.vid = 0x05AC, .pid = 1, .vbus_code = 3, .current_code = 2,
                                    .speed = KAT_SPEED_GEN3, .latency = 1, .epr = true};

static void test_labels(void)
{
    kat_cable_t c;
    kat_fingerprint_t fp;
    char buf[KAT_LABEL_LEN], why[KAT_REASON_LEN];

    make(&c, &CC_FULL, &EM_240, 90);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "C\xE2\x80\x93" "C 240W 40G \xE2\x89\xA4" "1m");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "SuperSpeed: all 8 wires");

    kat_sim_usb_t usb2 = {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true};
    make(&c, &usb2, NULL, 200);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "C\xE2\x80\x93" "C 60W USB2");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "USB 2 data, no SuperSpeed");

    kat_sim_usb_t power = {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true, .no_d = true};
    make(&c, &power, NULL, 200);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "C\xE2\x80\x93" "C 60W power only");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "No data wires: power only");

    /* No e-marker but SuperSpeed wires: the speed isn't known. */
    kat_sim_usb_t ac = {.kind = KAT_KIND_A_C, .rp = true};
    make(&c, &ac, NULL, 200);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "A\xE2\x80\x93" "C SS");

    kat_sim_usb_t otg = {.kind = KAT_KIND_A_MICRO, .otg = true};
    make(&c, &otg, NULL, 200);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "A\xE2\x80\x93\xC2\xB5" "B USB2 OTG");

    /* A 5 A claim on thin copper. */
    make(&c, &CC_FULL, &EM_240, 310);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_HAS(buf, " thin");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "e-marker claims 5 A but the loop is 310 m\xCE\xA9: too thin even for 3 A");
    make(&c, &usb2, NULL, 260);
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "Loop is 260 m\xCE\xA9: too thin even for 3 A");
    kat_sim_em_t em5 = EM_240;
    em5.speed = KAT_SPEED_USB2;
    make(&c, &usb2, &em5, 200);
    kat_reason(&c, why, sizeof why);
    CHECK_HAS(why, "copper fits 3 A");

    /* An e-marker that claims SuperSpeed on a USB 2 cable. */
    make(&c, &usb2, &EM_240, 90);
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "e-marker claims 40G but there are no SuperSpeed wires");
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "C\xE2\x80\x93" "C 240W USB2 \xE2\x89\xA4" "1m");

    /* Active cable. */
    kat_sim_em_t act = {.current_code = 1, .speed = KAT_SPEED_GEN2, .latency = 3, .active = true};
    make(&c, &CC_FULL, &act, 120);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "C\xE2\x80\x93" "C 60W 20G 3m active");

    /* RJ45. */
    make_rj(&c, (const uint8_t[8]){1, 2, 3, 4, 5, 6, 7, 8}, true);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "RJ45 8-wire STP");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "All 8 wires straight, shielded");
    make_rj(&c, (const uint8_t[8]){1, 2, 3, 0, 0, 6, 0, 0}, false);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "RJ45 4-wire UTP");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "Only 4 wires: 100 Mb max");
    make_rj(&c, (const uint8_t[8]){8, 7, 6, 5, 4, 3, 2, 1}, false);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "RJ45 rollover (console)");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "Rollover (console) cable");
    make_rj(&c, (const uint8_t[8]){3, 6, 1, 4, 5, 2, 7, 8}, false);
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "Crossover cable");
    make_rj(&c, (const uint8_t[8]){3, 6, 1, 7, 8, 2, 4, 5}, false);
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "Gigabit crossover cable");
    make_rj(&c, (const uint8_t[8]){2, 1, 3, 4, 5, 6, 7, 8}, false);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "RJ45 faulty");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "pair 1/2 reversed");

    /* Faulty USB, several faults; and no cable at all. */
    kat_sim_usb_t nocc = {.kind = KAT_KIND_C_C, .no_cc = true, .no_d = true, .no_ss = true, .no_sbu = true};
    make(&c, &nocc, NULL, 100);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, buf, sizeof buf);
    CHECK_STR(buf, "C\xE2\x80\x93" "C faulty");
    memset(&c, 0, sizeof c);
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "No cable");
    c.cont.kind = KAT_KIND_C_C;
    c.cont.fault_count = 2;
    strcpy(c.cont.faults[0], "VBUS open");
    kat_reason(&c, why, sizeof why);
    CHECK_STR(why, "VBUS open (+1 more)");

    /* A tiny buffer truncates safely. */
    char tiny[6];
    make(&c, &CC_FULL, &EM_240, 90);
    kat_fingerprint(&c, &fp);
    kat_label(&fp, tiny, sizeof tiny);
    CHECK_INT(strlen(tiny) < sizeof tiny, 1);
}

static void test_bagging(void)
{
    static kat_session_t s;
    kat_session_clear(&s);
    kat_cable_t c;

    make(&c, &CC_FULL, &EM_240, 100);
    kat_add_result_t r = kat_session_add(&s, &c, &cfg);
    CHECK_INT(r.bag, 0);
    CHECK(r.new_bag);
    CHECK(!r.faulty);

    /* Same cable, other way up, slightly different resistance: same bag. */
    kat_sim_usb_t flipped = CC_FULL;
    flipped.flip_a = 1;
    make(&c, &flipped, &EM_240, 112);
    r = kat_session_add(&s, &c, &cfg);
    CHECK_INT(r.bag, 0);
    CHECK(!r.new_bag);
    CHECK_INT(s.bags[0].count, 2);
    CHECK_INT(s.bags[0].loop_sum_mohm, 212);

    /* Outside the band (25 mΩ or 15 %): a new bag. */
    make(&c, &CC_FULL, &EM_240, 140);
    r = kat_session_add(&s, &c, &cfg);
    CHECK_INT(r.bag, 1);
    CHECK(r.new_bag);

    /* A different e-marker product ID: a new bag. */
    kat_sim_em_t other = EM_240;
    other.pid = 2;
    make(&c, &CC_FULL, &other, 100);
    r = kat_session_add(&s, &c, &cfg);
    CHECK_INT(r.bag, 2);

    /* A large-resistance bag uses the percentage band. */
    kat_sim_usb_t usb2 = {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true};
    make(&c, &usb2, NULL, 400);
    CHECK_INT(kat_session_add(&s, &c, &cfg).bag, 3);
    make(&c, &usb2, NULL, 455);
    CHECK_INT(kat_session_add(&s, &c, &cfg).bag, 3);

    /* RJ45 (no resistance) bags on wiring alone. */
    make_rj(&c, (const uint8_t[8]){1, 2, 3, 4, 5, 6, 7, 8}, false);
    CHECK_INT(kat_session_add(&s, &c, &cfg).bag, 4);
    CHECK_INT(kat_session_add(&s, &c, &cfg).bag, 4);
    CHECK_STR(s.bags[4].label, "RJ45 8-wire UTP");

    /* Faulty: counted, never bagged. */
    make_rj(&c, (const uint8_t[8]){2, 1, 3, 4, 5, 6, 7, 8}, false);
    r = kat_session_add(&s, &c, &cfg);
    CHECK(r.faulty);
    CHECK_INT(r.bag, -1);
    CHECK_INT(s.faulty_total, 1);
    CHECK_STR(s.faulty[0].reason, "pair 1/2 reversed");
    CHECK_INT(s.bag_count, 5);
    CHECK_INT(s.tested, 9);

    /* The faulty ring wraps. */
    for (int i = 0; i < KAT_MAX_FAULTY + 2; i++) kat_session_add(&s, &c, &cfg);
    CHECK_INT(s.faulty_total, KAT_MAX_FAULTY + 3);
    CHECK_INT(s.faulty_next, 3);

    /* Full: no more room for a new bag. */
    kat_session_clear(&s);
    for (int i = 0; i < KAT_MAX_BAGS + 1; i++) {
        kat_sim_em_t e = EM_240;
        e.pid = (uint16_t)(100 + i);
        make(&c, &CC_FULL, &e, 100);
        r = kat_session_add(&s, &c, &cfg);
    }
    CHECK(r.full);
    CHECK_INT(r.bag, -1);
    CHECK_INT(s.bag_count, KAT_MAX_BAGS);
}

static void test_letters_and_colours(void)
{
    char l[4];
    kat_bag_letter(0, l);
    CHECK_STR(l, "A");
    kat_bag_letter(25, l);
    CHECK_STR(l, "Z");
    kat_bag_letter(26, l);
    CHECK_STR(l, "AA");
    kat_bag_letter(27, l);
    CHECK_STR(l, "AB");
    kat_bag_letter(52, l);
    CHECK_STR(l, "BA");
    kat_bag_letter(-1, l);
    CHECK_STR(l, "?");
    CHECK(kat_bag_colour(0) == kat_bag_colour(KAT_PALETTE_SIZE));
    CHECK(kat_bag_colour(0) != kat_bag_colour(1));
    CHECK(kat_bag_colour(-5) == kat_bag_colour(0));
    CHECK_STR(kat_bag_colour(2)->name, "green");
    CHECK_INT(kat_rgb565(0xFFFFFF), 0xFFFF);
    CHECK_INT(kat_rgb565(0xFF0000), 0xF800);
    CHECK_INT(kat_rgb565(0x00FF00), 0x07E0);
    CHECK_INT(kat_rgb565(0x0000FF), 0x001F);
}

static void test_persistence(void)
{
    static kat_session_t s, t;
    kat_session_clear(&s);
    kat_cable_t c;
    make(&c, &CC_FULL, &EM_240, 100);
    kat_session_add(&s, &c, &cfg);
    kat_session_add(&s, &c, &cfg);

    size_t n = kat_session_blob_size();
    uint8_t *buf = malloc(n);
    CHECK_INT(kat_session_save(&s, buf, n - 1), 0);
    CHECK_INT(kat_session_save(&s, buf, n), n);
    CHECK(kat_session_load(&t, buf, n));
    CHECK_INT(t.bag_count, 1);
    CHECK_INT(t.bags[0].count, 2);
    CHECK_STR(t.bags[0].label, s.bags[0].label);
    CHECK(memcmp(&s, &t, sizeof s) == 0);

    CHECK(!kat_session_load(&t, buf, n - 1));
    CHECK(!kat_session_load(&t, NULL, n));
    buf[n - 1] ^= 1;                      /* corrupt the body: CRC fails */
    CHECK(!kat_session_load(&t, buf, n));
    buf[n - 1] ^= 1;
    buf[0] ^= 1;                          /* corrupt the magic */
    CHECK(!kat_session_load(&t, buf, n));
    buf[0] ^= 1;

    /* A body that passes its CRC but makes no sense is refused and leaves a clear session. */
    kat_session_t *bogus = calloc(1, sizeof *bogus);
    bogus->bag_count = KAT_MAX_BAGS + 1;
    kat_session_save(bogus, buf, n);
    CHECK(!kat_session_load(&t, buf, n));
    CHECK_INT(t.bag_count, 0);
    free(bogus);
    free(buf);

    CHECK_INT(kat_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
}

int main(void)
{
    kat_config_default(&cfg);
    test_labels();
    test_bagging();
    test_letters_and_colours();
    test_persistence();
    CHECK_DONE();
}

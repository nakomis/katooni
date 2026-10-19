#include "check.h"
#include "katooni/classify.h"
#include "katooni/simcable.h"

static kat_continuity_t r;

static void classify(const kat_scan_t *s)
{
    kat_classify(s, &r);
}

static int has_fault(const char *needle)
{
    for (int i = 0; i < r.fault_count; i++)
        if (strstr(r.faults[i], needle)) return 1;
    return 0;
}

static void join(kat_scan_t *s, kat_port_t pa, int a, kat_port_t pb, int b)
{
    kat_scan_join(s, kat_line(pa, a), kat_line(pb, b));
}

/* Hand-written, so these don't lean on the same tables as the classifier. */
static void test_hand_written_usb2_flipped(void)
{
    kat_scan_t s;
    kat_scan_clear(&s);
    /* A USB 2 C-to-C cable, A end right way up, B end upside down: A5 lands on B5, A6/A7 on B6/B7. */
    join(&s, KAT_PORT_A_USBC, KAT_C_VBUS, KAT_PORT_B_USBC, KAT_C_VBUS);
    join(&s, KAT_PORT_A_USBC, KAT_C_GND, KAT_PORT_B_USBC, KAT_C_GND);
    join(&s, KAT_PORT_A_USBC, KAT_C_CC1, KAT_PORT_B_USBC, KAT_C_CC2);
    join(&s, KAT_PORT_A_USBC, KAT_C_DP1, KAT_PORT_B_USBC, KAT_C_DP2);
    join(&s, KAT_PORT_A_USBC, KAT_C_DN1, KAT_PORT_B_USBC, KAT_C_DN2);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_C_C);
    CHECK_INT(r.cls, KAT_CLASS_USB2);
    CHECK_INT(r.flip_a, 0);
    CHECK_INT(r.flip_b, 1);
    CHECK_INT(r.fault_count, 0);
    CHECK(!r.shield);
    CHECK_INT(r.ss_wires, 0);
    CHECK_INT(r.ss_total, 8);
}

static void test_hand_written_superspeed(void)
{
    kat_scan_t s;
    kat_scan_clear(&s);
    /* Both ends right way up: TX at one end lands on RX at the other, SBU1 meets SBU2. */
    join(&s, KAT_PORT_A_USBC, KAT_C_VBUS, KAT_PORT_B_USBC, KAT_C_VBUS);
    join(&s, KAT_PORT_A_USBC, KAT_C_GND, KAT_PORT_B_USBC, KAT_C_GND);
    join(&s, KAT_PORT_A_USBC, KAT_C_CC1, KAT_PORT_B_USBC, KAT_C_CC1);
    join(&s, KAT_PORT_A_USBC, KAT_C_DP1, KAT_PORT_B_USBC, KAT_C_DP1);
    join(&s, KAT_PORT_A_USBC, KAT_C_DN1, KAT_PORT_B_USBC, KAT_C_DN1);
    join(&s, KAT_PORT_A_USBC, KAT_C_TX1P, KAT_PORT_B_USBC, KAT_C_RX1P);
    join(&s, KAT_PORT_A_USBC, KAT_C_TX1N, KAT_PORT_B_USBC, KAT_C_RX1N);
    join(&s, KAT_PORT_A_USBC, KAT_C_RX1P, KAT_PORT_B_USBC, KAT_C_TX1P);
    join(&s, KAT_PORT_A_USBC, KAT_C_RX1N, KAT_PORT_B_USBC, KAT_C_TX1N);
    join(&s, KAT_PORT_A_USBC, KAT_C_TX2P, KAT_PORT_B_USBC, KAT_C_RX2P);
    join(&s, KAT_PORT_A_USBC, KAT_C_TX2N, KAT_PORT_B_USBC, KAT_C_RX2N);
    join(&s, KAT_PORT_A_USBC, KAT_C_RX2P, KAT_PORT_B_USBC, KAT_C_TX2P);
    join(&s, KAT_PORT_A_USBC, KAT_C_RX2N, KAT_PORT_B_USBC, KAT_C_TX2N);
    join(&s, KAT_PORT_A_USBC, KAT_C_SBU1, KAT_PORT_B_USBC, KAT_C_SBU2);
    join(&s, KAT_PORT_A_USBC, KAT_C_SBU2, KAT_PORT_B_USBC, KAT_C_SBU1);
    join(&s, KAT_PORT_A_USBC, KAT_C_SHIELD, KAT_PORT_B_USBC, KAT_C_SHIELD);
    /* An e-marker at the A end: VCONN (the plug's B5) to GND through Ra. */
    join(&s, KAT_PORT_A_USBC, KAT_C_CC2, KAT_PORT_A_USBC, KAT_C_GND);
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_SUPERSPEED);
    CHECK_INT(r.ss_wires, 8);
    CHECK_INT(r.fault_count, 0);
    CHECK(r.shield);
    CHECK(r.ra);
}

static void test_cc_every_orientation(void)
{
    for (int fa = 0; fa < 2; fa++)
        for (int fb = 0; fb < 2; fb++) {
            kat_scan_t s;
            kat_sim_usb_t c = {.kind = KAT_KIND_C_C, .flip_a = (uint8_t)fa, .flip_b = (uint8_t)fb, .ra = true};
            kat_sim_usb(&s, &c);
            classify(&s);
            CHECK_INT(r.kind, KAT_KIND_C_C);
            CHECK_INT(r.cls, KAT_CLASS_SUPERSPEED);
            CHECK_INT(r.fault_count, 0);
            CHECK_INT(r.flip_a ^ r.flip_b, fa ^ fb);   /* only the relative flip is observable */
            CHECK(r.ra);
            CHECK(r.shield);
        }
}

static void test_cc_variants(void)
{
    kat_scan_t s;
    kat_sim_usb_t usb2 = {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true};
    kat_sim_usb(&s, &usb2);
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_USB2);
    CHECK(!r.ra);

    kat_sim_usb_t power = {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true, .no_d = true, .no_shield = true};
    kat_sim_usb(&s, &power);
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_POWER_ONLY);
    CHECK_INT(r.fault_count, 0);

    kat_sim_usb_t nocc = {.kind = KAT_KIND_C_C, .no_cc = true, .no_ss = true};
    kat_sim_usb(&s, &nocc);
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_FAULTY);
    CHECK(has_fault("CC open"));
}

static void test_cc_faults(void)
{
    kat_scan_t s;
    kat_sim_usb_t good = {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true};

    /* D+ and D− swapped inside the cable. */
    kat_scan_clear(&s);
    join(&s, KAT_PORT_A_USBC, KAT_C_VBUS, KAT_PORT_B_USBC, KAT_C_VBUS);
    join(&s, KAT_PORT_A_USBC, KAT_C_GND, KAT_PORT_B_USBC, KAT_C_GND);
    join(&s, KAT_PORT_A_USBC, KAT_C_CC1, KAT_PORT_B_USBC, KAT_C_CC1);
    join(&s, KAT_PORT_A_USBC, KAT_C_DP1, KAT_PORT_B_USBC, KAT_C_DN1);
    join(&s, KAT_PORT_A_USBC, KAT_C_DN1, KAT_PORT_B_USBC, KAT_C_DP1);
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_FAULTY);
    CHECK(has_fault("crossed"));

    /* D+ shorted to GND. */
    kat_sim_usb(&s, &good);
    kat_scan_join(&s, kat_line(KAT_PORT_A_USBC, KAT_C_DP1), kat_line(KAT_PORT_A_USBC, KAT_C_GND));
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_FAULTY);
    CHECK(has_fault("shorted"));

    /* VBUS shorted to GND: the dangerous one. */
    kat_sim_usb(&s, &good);
    kat_scan_join(&s, kat_line(KAT_PORT_B_USBC, KAT_C_VBUS), kat_line(KAT_PORT_B_USBC, KAT_C_GND));
    classify(&s);
    CHECK(has_fault("VBUS shorted to GND") || has_fault("GND shorted to VBUS"));

    /* Three nets in one lump. */
    kat_sim_usb(&s, &good);
    kat_scan_join(&s, kat_line(KAT_PORT_B_USBC, KAT_C_VBUS), kat_line(KAT_PORT_B_USBC, KAT_C_GND));
    kat_scan_join(&s, kat_line(KAT_PORT_B_USBC, KAT_C_VBUS), kat_line(KAT_PORT_B_USBC, KAT_C_CC1));
    classify(&s);
    CHECK(has_fault("more shorted together"));

    /* A pin the plug doesn't use, shorted to a wire. */
    kat_sim_usb(&s, &good);
    kat_scan_join(&s, kat_line(KAT_PORT_B_USBC, KAT_C_DP2), kat_line(KAT_PORT_B_USBC, KAT_C_GND));
    classify(&s);
    CHECK(has_fault("GND shorted to B-end D+ (B6)"));

    /* Two unused pins shorted to each other. */
    kat_sim_usb(&s, &good);
    kat_scan_join(&s, kat_line(KAT_PORT_B_USBC, KAT_C_DP2), kat_line(KAT_PORT_B_USBC, KAT_C_DN2));
    classify(&s);
    CHECK(has_fault("B-end D+ (B6) shorted to B-end D"));

    /* SBU wires shorted together. */
    kat_sim_usb(&s, &good);
    kat_scan_join(&s, kat_line(KAT_PORT_B_USBC, KAT_C_SBU1), kat_line(KAT_PORT_B_USBC, KAT_C_SBU2));
    classify(&s);
    CHECK(has_fault("SBU"));

    /* One SuperSpeed wire missing. */
    kat_sim_usb_t full = {.kind = KAT_KIND_C_C};
    kat_sim_usb(&s, &full);
    kat_scan_t t;
    kat_scan_clear(&t);
    for (int l = 0; l < KAT_LINE_COUNT; l++) {
        if (l == kat_line(KAT_PORT_A_USBC, KAT_C_TX2N)) continue;
        int root = kat_scan_root(&s, l);
        if (root != l && root != kat_line(KAT_PORT_A_USBC, KAT_C_TX2N)) kat_scan_join(&t, root, l);
    }
    classify(&t);
    CHECK_INT(r.cls, KAT_CLASS_FAULTY);
    CHECK(has_fault("only 7 of 8 SuperSpeed wires"));

    /* VBUS and GND missing, D− missing. */
    kat_scan_clear(&s);
    join(&s, KAT_PORT_A_USBC, KAT_C_CC1, KAT_PORT_B_USBC, KAT_C_CC1);
    join(&s, KAT_PORT_A_USBC, KAT_C_DP1, KAT_PORT_B_USBC, KAT_C_DP1);
    classify(&s);
    CHECK(has_fault("VBUS open"));
    CHECK(has_fault("GND open"));
    CHECK(has_fault("open: data needs both"));
}

static void test_other_usb_kinds(void)
{
    kat_scan_t s;
    kat_sim_usb_t ac = {.kind = KAT_KIND_A_C, .flip_b = 1, .rp = true, .ra = true};
    kat_sim_usb(&s, &ac);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_A_C);
    CHECK_INT(r.cls, KAT_CLASS_SUPERSPEED);
    CHECK_INT(r.ss_total, 4);
    CHECK_INT(r.fault_count, 0);
    CHECK_INT(r.flip_b, 1);

    kat_sim_usb_t ac2 = {.kind = KAT_KIND_A_C, .no_ss = true, .rp = true};
    kat_sim_usb(&s, &ac2);
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_USB2);

    kat_sim_usb_t cm = {.kind = KAT_KIND_C_MICRO, .flip_a = 1, .rd = true};
    kat_sim_usb(&s, &cm);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_C_MICRO);
    CHECK_INT(r.cls, KAT_CLASS_USB2);
    CHECK_INT(r.fault_count, 0);
    CHECK(!r.otg_id);

    kat_sim_usb_t am = {.kind = KAT_KIND_A_MICRO, .otg = true, .no_shield = true};
    kat_sim_usb(&s, &am);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_A_MICRO);
    CHECK(r.otg_id);
    CHECK(!r.shield);
    CHECK_INT(r.fault_count, 0);

    kat_sim_usb_t amp = {.kind = KAT_KIND_A_MICRO, .no_d = true};
    kat_sim_usb(&s, &amp);
    classify(&s);
    CHECK_INT(r.cls, KAT_CLASS_POWER_ONLY);

    kat_sim_usb_t none = {.kind = KAT_KIND_RJ45};    /* not a USB kind: leaves the scan empty */
    kat_sim_usb(&s, &none);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_NONE);
}

static void test_detection(void)
{
    kat_scan_t s;
    kat_scan_clear(&s);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_NONE);

    /* Two cables at once. */
    kat_sim_usb_t c = {.kind = KAT_KIND_C_C};
    kat_sim_usb(&s, &c);
    join(&s, KAT_PORT_A_RJ45, 0, KAT_PORT_B_RJ45, 0);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_MULTIPLE);
    CHECK(has_fault("more than one cable"));

    /* A pairing no cable makes: A-end RJ45 to B-end USB-C. */
    kat_scan_clear(&s);
    join(&s, KAT_PORT_A_RJ45, 0, KAT_PORT_B_USBC, KAT_C_VBUS);
    classify(&s);
    CHECK_INT(r.kind, KAT_KIND_MULTIPLE);
}

static void rj(const uint8_t m[8], bool shield)
{
    kat_scan_t s;
    kat_sim_rj45(&s, m, shield);
    classify(&s);
}

static void test_rj45(void)
{
    static const uint8_t STRAIGHT[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    rj(STRAIGHT, true);
    CHECK_INT(r.kind, KAT_KIND_RJ45);
    CHECK_INT(r.cls, KAT_CLASS_RJ45_8WIRE);
    CHECK(r.shield);
    CHECK_INT(r.present, 0x1FF);
    rj(STRAIGHT, false);
    CHECK(!r.shield);

    rj((const uint8_t[8]){1, 2, 3, 0, 0, 6, 0, 0}, false);
    CHECK_INT(r.cls, KAT_CLASS_RJ45_4WIRE);
    rj((const uint8_t[8]){3, 6, 1, 4, 5, 2, 7, 8}, false);
    CHECK_INT(r.cls, KAT_CLASS_RJ45_CROSSOVER);
    rj((const uint8_t[8]){3, 6, 1, 0, 0, 2, 0, 0}, false);
    CHECK_INT(r.cls, KAT_CLASS_RJ45_CROSSOVER);
    rj((const uint8_t[8]){3, 6, 1, 7, 8, 2, 4, 5}, false);
    CHECK_INT(r.cls, KAT_CLASS_RJ45_CROSSOVER_GIG);
    rj((const uint8_t[8]){8, 7, 6, 5, 4, 3, 2, 1}, false);
    CHECK_INT(r.cls, KAT_CLASS_RJ45_ROLLOVER);

    rj((const uint8_t[8]){2, 1, 3, 4, 5, 6, 7, 8}, true);
    CHECK_INT(r.cls, KAT_CLASS_FAULTY);
    CHECK(has_fault("pair 1/2 reversed"));
    rj((const uint8_t[8]){1, 2, 3, 4, 5, 6, 0, 8}, true);
    CHECK(has_fault("pin 7 open"));
    rj((const uint8_t[8]){1, 2, 3, 4, 5, 6, 8, 0}, true);
    CHECK(has_fault("pin 7 goes to pin 8"));

    kat_scan_t s;
    kat_sim_rj45(&s, STRAIGHT, false);
    kat_scan_join(&s, kat_line(KAT_PORT_A_RJ45, 0), kat_line(KAT_PORT_A_RJ45, 1));
    classify(&s);
    CHECK(has_fault("pins 1 and 2 shorted"));

    kat_sim_rj45(&s, STRAIGHT, true);
    kat_scan_join(&s, kat_line(KAT_PORT_B_RJ45, KAT_R_SHIELD), kat_line(KAT_PORT_B_RJ45, 3));
    classify(&s);
    CHECK(has_fault("shield shorted to pin 4"));
    CHECK(!r.shield);
}

static void test_names(void)
{
    CHECK_STR(kat_kind_name(KAT_KIND_C_C), "C\xE2\x80\x93" "C");
    CHECK_STR(kat_kind_name(KAT_KIND_A_MICRO), "A\xE2\x80\x93\xC2\xB5" "B");
    CHECK_STR(kat_kind_name(KAT_KIND_C_MICRO), "C\xE2\x80\x93\xC2\xB5" "B");
    CHECK_STR(kat_kind_name(KAT_KIND_A_C), "A\xE2\x80\x93" "C");
    CHECK_STR(kat_kind_name(KAT_KIND_RJ45), "RJ45");
    CHECK_STR(kat_kind_name(KAT_KIND_MULTIPLE), "several cables");
    CHECK_STR(kat_kind_name(KAT_KIND_NONE), "none");
    CHECK_STR(kat_class_name(KAT_CLASS_RJ45_ROLLOVER), "rollover (console)");
    CHECK_STR(kat_class_name(KAT_CLASS_NONE), "none");
    uint8_t n;
    CHECK(kat_kind_nets(KAT_KIND_C_C, &n) != NULL);
    CHECK_INT(n, 16);
    CHECK(kat_kind_nets(KAT_KIND_NONE, &n) == NULL);
    CHECK_INT(n, 0);
}

int main(void)
{
    test_hand_written_usb2_flipped();
    test_hand_written_superspeed();
    test_cc_every_orientation();
    test_cc_variants();
    test_cc_faults();
    test_other_usb_kinds();
    test_detection();
    test_rj45();
    test_names();
    CHECK_DONE();
}

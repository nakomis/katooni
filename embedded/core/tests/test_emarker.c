/* E-marker replies here are synthetic, built to the PD 3.1 field layouts (kat_sim_emarker) or by
 * hand. None is captured from a real cable yet: add real captures to this file once the board runs. */
#include "check.h"
#include "katooni/emarker.h"
#include "katooni/simcable.h"

static kat_emarker_t em;

static void test_none(void)
{
    CHECK_INT(kat_emarker_decode(NULL, 0, &em), KAT_EM_NO_REPLY);
    CHECK(!em.present);
    CHECK_INT(em.max_vbus_mv, 20000);
    CHECK_INT(em.max_current_ma, 3000);
    CHECK_INT(kat_emarker_watts(&em), 60);
    CHECK_INT(em.speed, KAT_SPEED_UNKNOWN);
}

static void test_passive_240w(void)
{
    uint32_t o[6];
    kat_sim_em_t e = {.vid = 0x05AC, .pid = 0x7E57, .vbus_code = 3, .current_code = 2,
                      .speed = KAT_SPEED_GEN3, .latency = 1, .epr = true};
    int n = kat_sim_emarker(&e, o);
    CHECK_INT(n, 5);
    CHECK_INT(kat_emarker_decode(o, n, &em), KAT_EM_OK);
    CHECK(em.present);
    CHECK(!em.active);
    CHECK(em.epr);
    CHECK_INT(em.vid, 0x05AC);
    CHECK_INT(em.pid, 0x7E57);
    CHECK_INT(em.bcd_device, 0x0100);
    CHECK_INT(em.xid, 0x1234);
    CHECK_INT(em.max_vbus_mv, 50000);
    CHECK_INT(em.max_current_ma, 5000);
    CHECK_INT(kat_emarker_watts(&em), 240);
    CHECK_INT(em.speed, KAT_SPEED_GEN3);
    CHECK_STR(kat_speed_label(em.speed, true), "40G");
    CHECK_INT(em.latency, 1);
    CHECK_STR(kat_latency_label(em.latency), "\xE2\x89\xA4" "1m");
    CHECK_INT(em.svdm_major, 1);
    CHECK_INT(em.vdo_count, 5);
    CHECK_INT(em.vdo[0], o[0]);
    CHECK_INT(em.hw_ver, 1);
    CHECK_INT(em.fw_ver, 2);
    CHECK(!em.captive);
}

static void test_hand_built_100w(void)
{
    /* Built by hand from the spec: VDM header 0xFF00A841 (SVID FF00, structured, v2.1, ACK,
     * Discover Identity); ID header: passive cable (011 in B29..27), VID 0x1234; Passive Cable VDO:
     * latency 0010 (~2 m), 20 V (00), 5 A (10 in B6..5), USB 3.2 Gen2 (010). */
    uint32_t o[5] = {
        0xFF00A841u,
        (3u << 27) | 0x1234u,
        0,
        0xABCD0000u,
        (2u << 13) | (0u << 9) | (2u << 5) | 2u,
    };
    CHECK_INT(kat_emarker_decode(o, 5, &em), KAT_EM_OK);
    CHECK_INT(em.vid, 0x1234);
    CHECK_INT(em.pid, 0xABCD);
    CHECK_INT(em.max_vbus_mv, 20000);
    CHECK_INT(em.max_current_ma, 5000);
    CHECK_INT(kat_emarker_watts(&em), 100);
    CHECK_INT(em.speed, KAT_SPEED_GEN2);
    CHECK_STR(kat_speed_label(em.speed, true), "20G");
    CHECK_STR(kat_speed_label(em.speed, false), "10G");
    CHECK_STR(kat_latency_label(em.latency), "2m");
    CHECK_INT(kat_latency_metres(em.latency), 2);
}

static void test_pd2(void)
{
    /* A PD 2.0 cable: VDM version 1.0, where B10..9 are lane directionality, not voltage. */
    uint32_t o[6];
    kat_sim_em_t e = {.vbus_code = 3, .current_code = 1, .speed = KAT_SPEED_USB2, .pd2 = true};
    int n = kat_sim_emarker(&e, o);
    CHECK_INT(kat_emarker_decode(o, n, &em), KAT_EM_OK);
    CHECK_INT(em.svdm_major, 0);
    CHECK_INT(em.max_vbus_mv, 20000);
    CHECK_INT(em.max_current_ma, 3000);
    CHECK(!em.epr);
}

static void test_active(void)
{
    uint32_t o[6];
    kat_sim_em_t e = {.vbus_code = 0, .current_code = 1, .speed = KAT_SPEED_GEN4, .latency = 3, .active = true};
    int n = kat_sim_emarker(&e, o);
    CHECK_INT(n, 6);
    CHECK_INT(kat_emarker_decode(o, n, &em), KAT_EM_OK);
    CHECK(em.active);
    CHECK(em.vbus_through);
    CHECK(em.usb4);
    CHECK(!em.optical);
    CHECK(!em.retimer);
    CHECK_INT(em.max_current_ma, 3000);
    CHECK_STR(kat_speed_label(em.speed, true), "80G");

    o[4] &= ~(1u << 4);       /* no VBUS through the cable */
    o[5] |= (1u << 10) | (1u << 9) | (1u << 8);   /* optical, retimer, no USB4 */
    kat_emarker_decode(o, n, &em);
    CHECK(!em.vbus_through);
    CHECK_INT(em.max_current_ma, 0);
    CHECK(em.optical);
    CHECK(em.retimer);
    CHECK(!em.usb4);
    kat_emarker_decode(o, 5, &em);      /* VDO2 missing: still decodes */
    CHECK(em.active);
}

static void test_bad_replies(void)
{
    uint32_t o[6];
    kat_sim_em_t e = {.current_code = 1};
    int n = kat_sim_emarker(&e, o);

    uint32_t nak[1] = {(o[0] & ~(3u << 6)) | (2u << 6)};
    CHECK_INT(kat_emarker_decode(nak, 1, &em), KAT_EM_NAK);
    CHECK(!em.present);
    CHECK_INT(em.max_current_ma, 3000);

    uint32_t bad[5];
    memcpy(bad, o, sizeof bad);
    bad[0] = (bad[0] & 0xFFFF) | (0xFF01u << 16);     /* not the PD SID */
    CHECK_INT(kat_emarker_decode(bad, 5, &em), KAT_EM_BAD);
    memcpy(bad, o, sizeof bad);
    bad[0] &= ~(1u << 15);                             /* unstructured */
    CHECK_INT(kat_emarker_decode(bad, 5, &em), KAT_EM_BAD);
    memcpy(bad, o, sizeof bad);
    bad[0] = (bad[0] & ~0x1Fu) | 2u;                   /* Discover SVIDs */
    CHECK_INT(kat_emarker_decode(bad, 5, &em), KAT_EM_BAD);
    CHECK_INT(kat_emarker_decode(o, 4, &em), KAT_EM_BAD);
    memcpy(bad, o, sizeof bad);
    bad[1] = (bad[1] & ~(7u << 27)) | (6u << 27);      /* a VCONN-powered device, not a cable */
    CHECK_INT(kat_emarker_decode(bad, 5, &em), KAT_EM_NOT_CABLE);
    CHECK(!em.present);

    /* More objects than we keep: the extras are ignored. */
    uint32_t many[9] = {0};
    memcpy(many, o, (size_t)n * sizeof o[0]);
    CHECK_INT(kat_emarker_decode(many, 9, &em), KAT_EM_OK);
    CHECK_INT(em.vdo_count, KAT_EM_MAX_VDOS);
}

static void test_labels(void)
{
    CHECK_STR(kat_speed_label(KAT_SPEED_USB2, true), "USB2");
    CHECK_STR(kat_speed_label(KAT_SPEED_GEN1, true), "5G");
    CHECK_STR(kat_speed_label(KAT_SPEED_UNKNOWN, true), "?");
    CHECK_STR(kat_latency_label(8), ">7m");
    CHECK_STR(kat_latency_label(9), "~200m");
    CHECK_STR(kat_latency_label(15), "?");
    CHECK_INT(kat_latency_metres(0), 0);
    CHECK_INT(kat_latency_metres(8), 0);
    CHECK_INT(kat_latency_metres(7), 7);

    /* Speed codes 5-7 are reserved. */
    uint32_t o[6];
    kat_sim_em_t e = {.current_code = 1, .speed = 6};
    kat_emarker_decode(o, kat_sim_emarker(&e, o), &em);
    CHECK_INT(em.speed, KAT_SPEED_UNKNOWN);

    kat_emarker_t x = {.max_vbus_mv = 30000, .max_current_ma = 3000};
    CHECK_INT(kat_emarker_watts(&x), 90);
}

int main(void)
{
    test_none();
    test_passive_240w();
    test_hand_built_100w();
    test_pd2();
    test_active();
    test_bad_replies();
    test_labels();
    CHECK_DONE();
}

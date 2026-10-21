/* Shape checks only: the simulator's tests (embedded/sim/test_sim.py) parse the same JSON for real. */
#include "check.h"
#include "katooni/json.h"
#include "katooni/simcable.h"

static char text[32768];

static const char *render_session(const kat_session_t *s)
{
    kat_buf_t b = {text, sizeof text, 0, false};
    kat_json_session(s, kat_buf_sink(&b));
    CHECK(!b.overflow);
    return text;
}

static void add_usb(kat_session_t *s, const kat_config_t *cfg, kat_cable_t *c, kat_add_result_t *r, bool em, uint32_t loop)
{
    kat_scan_t scan;
    kat_sim_usb_t u = {.kind = KAT_KIND_C_C, .ra = em};
    kat_sim_usb(&scan, &u);
    kat_classify(&scan, &c->cont);
    if (em) {
        uint32_t o[6];
        kat_sim_em_t e = {.vid = 0x05AC, .pid = 0x1234, .vbus_code = 3, .current_code = 2, .speed = KAT_SPEED_GEN3,
                          .latency = 1, .epr = true};
        kat_emarker_decode(o, kat_sim_emarker(&e, o), &c->em);
    } else {
        kat_emarker_none(&c->em);
    }
    kat_resistance_compute((int32_t)loop * 500, 500000, c->em.max_current_ma, 1, &cfg->res, &c->res);
    *r = kat_session_add(s, c, cfg);
}

int main(void)
{
    static kat_session_t s;
    kat_config_t cfg;
    kat_config_default(&cfg);
    kat_session_clear(&s);

    CHECK_STR(render_session(&s), "{\"tested\":0,\"faulty_total\":0,\"bags\":[],\"faulty\":[]}");

    kat_cable_t c;
    kat_add_result_t r;
    add_usb(&s, &cfg, &c, &r, true, 90);
    add_usb(&s, &cfg, &c, &r, true, 95);
    add_usb(&s, &cfg, &c, &r, false, 200);
    const char *j = render_session(&s);
    CHECK_HAS(j, "\"tested\":3");
    CHECK_HAS(j, "\"letter\":\"A\",\"colour\":\"#1F77B4\",\"colour_name\":\"blue\",\"count\":2,\"singleton\":false");
    CHECK_HAS(j, "\"label\":\"C\xE2\x80\x93" "C 240W 40G \xE2\x89\xA4" "1m\"");
    CHECK_HAS(j, "\"mean_loop_mohm\":92");
    CHECK_HAS(j, "\"emarker\":{\"vid\":\"05AC\",\"pid\":\"1234\",\"max_v\":50,\"max_a\":5,\"watts\":240");
    CHECK_HAS(j, "\"letter\":\"B\"");
    CHECK_HAS(j, "\"singleton\":true");
    CHECK_HAS(j, "\"emarker\":null");

    /* Faulty cables, newest first; a reason with a quote in it is escaped. */
    kat_scan_t scan;
    kat_sim_rj45(&scan, (const uint8_t[8]){2, 1, 3, 4, 5, 6, 7, 8}, false);
    kat_classify(&scan, &c.cont);
    kat_emarker_none(&c.em);
    memset(&c.res, 0, sizeof c.res);
    kat_session_add(&s, &c, &cfg);
    strcpy(c.cont.faults[0], "odd \"quote\"\tand tab");
    kat_session_add(&s, &c, &cfg);
    j = render_session(&s);
    CHECK_HAS(j, "\"faulty_total\":2");
    CHECK_HAS(j, "\"faulty\":[{\"label\":\"RJ45 faulty\",\"reason\":\"odd \\\"quote\\\"\\u0009and tab\"},{\"label\":\"RJ45 faulty\",\"reason\":\"pair 1/2 reversed\"}]");

    /* The last cable. */
    kat_buf_t b = {text, sizeof text, 0, false};
    kat_json_last(&s, &c, &r, false, 7, kat_buf_sink(&b));
    CHECK_STR(text, "{\"seq\":7,\"have\":false}");

    add_usb(&s, &cfg, &c, &r, true, 90);
    b.len = 0;
    kat_json_last(&s, &c, &r, true, 8, kat_buf_sink(&b));
    CHECK(!b.overflow);
    CHECK_HAS(text, "\"seq\":8,\"have\":true,\"result\":{\"bag\":0,\"letter\":\"A\",\"colour\":\"#1F77B4\",\"count\":3,\"new_bag\":false");
    CHECK_HAS(text, "\"nets\":[{\"name\":\"VBUS\",\"present\":true}");
    CHECK_HAS(text, "\"emarker_reply\":{\"status\":\"ok\",\"vdos\":[\"FF00A841\"");
    CHECK_HAS(text, "\"resistance\":{\"measured\":true,\"loop_mohm\":90,\"copper_amps\":5");
    CHECK_HAS(text, "\"faults\":[]");

    kat_sim_rj45(&scan, (const uint8_t[8]){1, 2, 3, 4, 5, 6, 7, 8}, true);
    kat_classify(&scan, &c.cont);
    kat_emarker_none(&c.em);
    memset(&c.res, 0, sizeof c.res);
    r = kat_session_add(&s, &c, &cfg);
    b.len = 0;
    kat_json_last(&s, &c, &r, true, 9, kat_buf_sink(&b));
    CHECK_HAS(text, "\"rj45_map\":[1,2,3,4,5,6,7,8]");
    CHECK_HAS(text, "\"status\":\"no reply\",\"vdos\":[]");

    /* A buffer too small says so rather than truncating silently. */
    char tiny[16];
    kat_buf_t t = {tiny, sizeof tiny, 0, false};
    kat_json_session(&s, kat_buf_sink(&t));
    CHECK(t.overflow);
    CHECK(strlen(tiny) < sizeof tiny);

    CHECK_DONE();
}

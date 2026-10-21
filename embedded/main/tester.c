#include "tester.h"

#include <stdio.h>
#include <string.h>

#include "app_state.h"
#include "buzzer.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fusb302.h"
#include "lines_hw.h"
#include "loadtest.h"
#include "net.h"

#define POLL_MS 150
#define SETTLE_SCANS 3     /* identical scans in a row before testing: plugs fully seated */
#define GONE_SCANS 2       /* empty scans in a row before the cable counts as removed */

static const char *TAG = "tester";

static void show_ready(void)
{
    char where[96], body[160];
    net_where(where, sizeof where);
    snprintf(body, sizeof body, "Plug a cable into both ends. Results: %s", where);
    display_status("Ready", body);
}

/* Continuous VBUS and GND, not shorted together: safe to put current through. */
static bool loop_ok(const kat_scan_t *s)
{
    int av = kat_line(KAT_PORT_A_USBC, KAT_C_VBUS), ag = kat_line(KAT_PORT_A_USBC, KAT_C_GND);
    int bv = kat_line(KAT_PORT_B_USBC, KAT_C_VBUS), bg = kat_line(KAT_PORT_B_USBC, KAT_C_GND);
    return kat_scan_joined(s, av, bv) && kat_scan_joined(s, ag, bg) && !kat_scan_joined(s, av, ag);
}

static void test_cable(const kat_scan_t *scan)
{
    static kat_cable_t c;     /* big enough to keep off the task stack */
    memset(&c, 0, sizeof c);
    kat_classify(scan, &c.cont);
    kat_emarker_none(&c.em);

    /* The e-marker lives on the A-end USB-C: C-to-C and C-to-micro-B cables. */
    if (c.cont.kind == KAT_KIND_C_C || c.cont.kind == KAT_KIND_C_MICRO) {
        uint32_t objs[7];
        lines_quiet(true);
        loadtest_ground_a_end(true);
        int n = fusb302_discover_identity(c.cont.flip_a ? 2 : 1, objs);
        loadtest_ground_a_end(false);
        lines_quiet(false);
        kat_emarker_decode(objs, n > 0 ? n : 0, &c.em);
    }

    app_state_t *st = app_state_lock();
    kat_config_t cfg = st->config;
    app_state_unlock();

    /* The load path runs between the two USB-C sockets, so only C-to-C cables are measured. */
    if (c.cont.kind == KAT_KIND_C_C && loop_ok(scan)) {
        int32_t drop = 0, cur = 0;
        lines_quiet(true);
        esp_err_t err = loadtest_measure(&drop, &cur);
        lines_quiet(false);
        if (err == ESP_OK)
            kat_resistance_compute(drop, cur, c.em.max_current_ma, kat_latency_metres(c.em.latency), &cfg.res, &c.res);
    }

    st = app_state_lock();
    kat_add_result_t r = kat_session_add(&st->session, &c, &cfg);
    st->last = c;
    st->last_result = r;
    st->have_last = true;
    st->last_seq++;
    display_result_t d = {.faulty = r.faulty, .new_bag = r.new_bag, .cont = &c.cont};
    char label[KAT_LABEL_LEN], reason[KAT_REASON_LEN];
    kat_fingerprint_t fp;
    kat_fingerprint(&c, &fp);
    kat_label(&fp, label, sizeof label);
    kat_reason(&c, reason, sizeof reason);
    if (r.bag >= 0) {
        kat_bag_letter(r.bag, d.letter);
        d.rgb = kat_bag_colour(r.bag)->rgb;
        d.count = st->session.bags[r.bag].count;
    } else if (r.full) {
        strcpy(d.letter, "?");
        strlcpy(reason, "No room for another bag: start a new session", sizeof reason);
    }
    app_state_save();
    app_state_unlock();

    d.label = label;
    d.reason = reason;
    display_result(&d);
    buzzer_play(r.faulty ? BEEP_FAULTY : r.new_bag ? BEEP_NEW_BAG : BEEP_SAME_BAG);
    ESP_LOGI(TAG, "%s: %s (bag %d%s)", label, reason, r.bag, r.new_bag ? ", new" : "");
}

static void task(void *arg)
{
    static kat_scan_t scan, prev;
    int same = 0, gone = 0;
    bool showing = false;
    kat_scan_clear(&prev);
    show_ready();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        int stuck = 0;
        if (lines_scan(&scan, &stuck) != ESP_OK) continue;
        bool cable = kat_scan_any_cable(&scan);
        if (showing) {
            /* Leave the result up while it's bagged; a new cable replaces it. */
            gone = cable ? 0 : gone + 1;
            if (gone >= GONE_SCANS) {
                showing = false;
                same = 0;
            }
            continue;
        }
        if (!cable) {
            same = 0;
            continue;
        }
        same = kat_scan_equal(&scan, &prev) ? same + 1 : 0;
        prev = scan;
        if (same + 1 >= SETTLE_SCANS) {
            if (stuck) ESP_LOGW(TAG, "%d lines low with nothing driven", stuck);
            test_cable(&scan);
            showing = true;
            gone = 0;
        }
    }
}

static void fault_task(void *arg)
{
    display_status("Hardware fault", "The line expanders aren't answering on I2C, so Katooni can't test cables.");
    vTaskDelete(NULL);
}

void tester_start(bool hw_ok)
{
    xTaskCreate(hw_ok ? task : fault_task, "tester", 6144, NULL, 5, NULL);
}

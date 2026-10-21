#include "katooni/cable.h"

#include <stdio.h>
#include <string.h>

#define OHM "m\xCE\xA9"   /* mΩ */

void kat_fingerprint(const kat_cable_t *c, kat_fingerprint_t *fp)
{
    memset(fp, 0, sizeof *fp);
    fp->kind = (uint8_t)c->cont.kind;
    fp->cls = (uint8_t)c->cont.cls;
    fp->present = c->cont.present;
    memcpy(fp->rj45_map, c->cont.rj45_map, sizeof fp->rj45_map);
    fp->shield = c->cont.shield;
    fp->otg_id = c->cont.otg_id;
    if (c->em.present) {
        fp->em_present = 1;
        fp->em_active = c->em.active;
        fp->em_epr = c->em.epr;
        fp->em_captive = c->em.captive;
        fp->vid = c->em.vid;
        fp->pid = c->em.pid;
        fp->speed = (uint8_t)c->em.speed;
        fp->latency = c->em.latency;
    } else {
        fp->speed = KAT_SPEED_UNKNOWN;
    }
    fp->max_vbus_mv = c->em.max_vbus_mv;
    fp->max_current_ma = c->em.max_current_ma;
    fp->res_measured = c->res.measured;
    fp->loop_mohm = c->res.loop_mohm;
    fp->claims_too_much = c->res.claims_too_much;
}

bool kat_fingerprint_match(const kat_fingerprint_t *bag, const kat_fingerprint_t *c, uint16_t tol_mohm,
                           uint8_t tol_pct)
{
    kat_fingerprint_t a = *bag, b = *c;
    a.loop_mohm = b.loop_mohm = 0;
    if (memcmp(&a, &b, sizeof a) != 0) return false;
    if (!bag->res_measured) return true;
    uint32_t tol = (uint32_t)bag->loop_mohm * tol_pct / 100;
    if (tol < tol_mohm) tol = tol_mohm;
    uint32_t d = bag->loop_mohm > c->loop_mohm ? bag->loop_mohm - c->loop_mohm : c->loop_mohm - bag->loop_mohm;
    return d <= tol;
}

bool kat_cable_faulty(const kat_cable_t *c)
{
    return c->cont.kind == KAT_KIND_MULTIPLE || c->cont.cls == KAT_CLASS_FAULTY;
}

typedef struct {
    char *buf;
    size_t n, len;
} sb_t;

static void put(sb_t *s, const char *fmt, const char *arg)
{
    if (s->len >= s->n) return;
    int w = snprintf(s->buf + s->len, s->n - s->len, fmt, arg);
    if (w > 0) s->len += (size_t)w < s->n - s->len ? (size_t)w : s->n - s->len - 1;
}

static void put_u(sb_t *s, const char *fmt, unsigned v)
{
    if (s->len >= s->n) return;
    int w = snprintf(s->buf + s->len, s->n - s->len, fmt, v);
    if (w > 0) s->len += (size_t)w < s->n - s->len ? (size_t)w : s->n - s->len - 1;
}

void kat_label(const kat_fingerprint_t *fp, char *buf, size_t n)
{
    sb_t s = {buf, n, 0};
    if (n) buf[0] = 0;
    kat_kind_t kind = (kat_kind_t)fp->kind;
    kat_class_t cls = (kat_class_t)fp->cls;
    put(&s, "%s", kat_kind_name(kind));
    if (kind == KAT_KIND_RJ45) {
        put(&s, " %s", cls == KAT_CLASS_RJ45_4WIRE ? "4-wire" : kat_class_name(cls));
        if (cls != KAT_CLASS_FAULTY && cls != KAT_CLASS_RJ45_ROLLOVER) put(&s, " %s", fp->shield ? "STP" : "UTP");
        return;
    }
    if (cls == KAT_CLASS_FAULTY) {
        put(&s, " %s", "faulty");
        return;
    }
    if (kind == KAT_KIND_C_C) {
        kat_emarker_t em = {.max_vbus_mv = fp->max_vbus_mv, .max_current_ma = fp->max_current_ma};
        put_u(&s, " %uW", kat_emarker_watts(&em));
    }
    if (cls == KAT_CLASS_POWER_ONLY) put(&s, " %s", "power only");
    else if (cls == KAT_CLASS_USB2) put(&s, " %s", "USB2");
    else if (fp->em_present && fp->speed != KAT_SPEED_UNKNOWN && fp->speed != KAT_SPEED_USB2)
        put(&s, " %s", kat_speed_label((kat_usb_speed_t)fp->speed, kind == KAT_KIND_C_C));
    else put(&s, " %s", "SS");
    if (fp->em_present && fp->latency) put(&s, " %s", kat_latency_label(fp->latency));
    if (fp->em_active) put(&s, " %s", "active");
    if (fp->otg_id) put(&s, " %s", "OTG");
    if (fp->claims_too_much) put(&s, " %s", "thin");
}

void kat_reason(const kat_cable_t *c, char *buf, size_t n)
{
    sb_t s = {buf, n, 0};
    if (n) buf[0] = 0;
    const kat_continuity_t *k = &c->cont;
    if (k->kind == KAT_KIND_NONE) {
        put(&s, "%s", "No cable");
        return;
    }
    if (k->fault_count) {
        put(&s, "%s", k->faults[0]);
        if (k->fault_count > 1) put_u(&s, " (+%u more)", k->fault_count - 1u);
        return;
    }
    if (c->res.claims_too_much) {
        if (c->em.present) put_u(&s, "e-marker claims %u A but the loop is ", c->em.max_current_ma / 1000u);
        else put(&s, "%s", "Loop is ");
        put_u(&s, "%u " OHM, c->res.loop_mohm);
        put(&s, "%s", c->res.copper_amps ? ": copper fits 3 A" : ": too thin even for 3 A");
        return;
    }
    if (c->em.present && k->cls != KAT_CLASS_SUPERSPEED && c->em.speed != KAT_SPEED_USB2 &&
        c->em.speed != KAT_SPEED_UNKNOWN && k->kind != KAT_KIND_RJ45) {
        put(&s, "e-marker claims %s but there are no SuperSpeed wires",
            kat_speed_label(c->em.speed, k->kind == KAT_KIND_C_C));
        return;
    }
    switch (k->cls) {
    case KAT_CLASS_SUPERSPEED: put_u(&s, "SuperSpeed: all %u wires", k->ss_total); break;
    case KAT_CLASS_USB2: put(&s, "%s", "USB 2 data, no SuperSpeed"); break;
    case KAT_CLASS_POWER_ONLY: put(&s, "%s", "No data wires: power only"); break;
    case KAT_CLASS_RJ45_8WIRE: put(&s, "All 8 wires straight%s", k->shield ? ", shielded" : ""); break;
    case KAT_CLASS_RJ45_4WIRE: put(&s, "%s", "Only 4 wires: 100 Mb max"); break;
    case KAT_CLASS_RJ45_CROSSOVER: put(&s, "%s", "Crossover cable"); break;
    case KAT_CLASS_RJ45_CROSSOVER_GIG: put(&s, "%s", "Gigabit crossover cable"); break;
    case KAT_CLASS_RJ45_ROLLOVER: put(&s, "%s", "Rollover (console) cable"); break;
    default: put(&s, "%s", kat_class_name(k->cls)); break;
    }
}

#include "katooni/json.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void kat_buf_write(void *ctx, const char *s, size_t n)
{
    kat_buf_t *b = ctx;
    if (b->overflow || b->len + n + 1 > b->cap) {
        b->overflow = true;
        return;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = 0;
}

static void raw(kat_sink_t o, const char *s)
{
    o.write(o.ctx, s, strlen(s));
}

static void fmt(kat_sink_t o, const char *f, ...)
{
    char tmp[96];
    va_list ap;
    va_start(ap, f);
    int n = vsnprintf(tmp, sizeof tmp, f, ap);
    va_end(ap);
    if (n > 0) o.write(o.ctx, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

/* A JSON string: quotes, backslashes and control characters escaped; UTF-8 passes through. */
static void str(kat_sink_t o, const char *s)
{
    raw(o, "\"");
    const char *run = s;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c != '"' && c != '\\' && c >= 0x20) continue;
        o.write(o.ctx, run, (size_t)(s - run));
        if (c == '"') raw(o, "\\\"");
        else if (c == '\\') raw(o, "\\\\");
        else fmt(o, "\\u%04x", c);
        run = s + 1;
    }
    o.write(o.ctx, run, (size_t)(s - run));
    raw(o, "\"");
}

static void key(kat_sink_t o, const char *k)
{
    str(o, k);
    raw(o, ":");
}

static void kv_str(kat_sink_t o, const char *k, const char *v, bool comma)
{
    key(o, k);
    str(o, v);
    if (comma) raw(o, ",");
}

static void kv_int(kat_sink_t o, const char *k, long v, bool comma)
{
    key(o, k);
    fmt(o, "%ld", v);
    if (comma) raw(o, ",");
}

static void kv_bool(kat_sink_t o, const char *k, bool v, bool comma)
{
    key(o, k);
    raw(o, v ? "true" : "false");
    if (comma) raw(o, ",");
}

static const char *speed_name(uint8_t speed)
{
    static const char *const N[] = {"USB 2", "USB 3.2 Gen1", "USB 3.2 / USB4 Gen2", "USB4 Gen3", "USB4 Gen4"};
    return speed < sizeof N / sizeof N[0] ? N[speed] : "unknown";
}

/* The parts of a fingerprint a person reads: what it is, the e-marker's claims, the copper. */
static void spec(kat_sink_t o, const kat_fingerprint_t *fp)
{
    raw(o, "{");
    kv_str(o, "kind", kat_kind_name((kat_kind_t)fp->kind), true);
    kv_str(o, "class", kat_class_name((kat_class_t)fp->cls), true);
    kv_bool(o, "shield", fp->shield, true);
    kv_bool(o, "otg", fp->otg_id, true);
    key(o, "emarker");
    if (fp->em_present) {
        kat_emarker_t em = {.max_vbus_mv = fp->max_vbus_mv, .max_current_ma = fp->max_current_ma};
        raw(o, "{");
        fmt(o, "\"vid\":\"%04X\",\"pid\":\"%04X\",", fp->vid, fp->pid);
        kv_int(o, "max_v", fp->max_vbus_mv / 1000, true);
        kv_int(o, "max_a", fp->max_current_ma / 1000, true);
        kv_int(o, "watts", (long)kat_emarker_watts(&em), true);
        kv_str(o, "speed", speed_name(fp->speed), true);
        kv_str(o, "length", kat_latency_label(fp->latency), true);
        kv_bool(o, "active", fp->em_active, true);
        kv_bool(o, "epr", fp->em_epr, false);
        raw(o, "}");
    } else {
        raw(o, "null");
    }
    raw(o, ",");
    key(o, "resistance");
    if (fp->res_measured) {
        raw(o, "{");
        kv_int(o, "loop_mohm", (long)fp->loop_mohm, true);
        kv_bool(o, "too_thin", fp->claims_too_much, false);
        raw(o, "}");
    } else {
        raw(o, "null");
    }
    raw(o, "}");
}

void kat_json_session(const kat_session_t *s, kat_sink_t o)
{
    raw(o, "{");
    kv_int(o, "tested", (long)s->tested, true);
    kv_int(o, "faulty_total", s->faulty_total, true);
    key(o, "bags");
    raw(o, "[");
    for (int i = 0; i < s->bag_count; i++) {
        const kat_bag_t *b = &s->bags[i];
        const kat_colour_t *c = kat_bag_colour(i);
        char letter[4];
        kat_bag_letter(i, letter);
        if (i) raw(o, ",");
        raw(o, "{");
        kv_int(o, "index", i, true);
        kv_str(o, "letter", letter, true);
        fmt(o, "\"colour\":\"#%06lX\",", (unsigned long)c->rgb);
        kv_str(o, "colour_name", c->name, true);
        kv_int(o, "count", b->count, true);
        kv_bool(o, "singleton", b->count == 1, true);
        kv_str(o, "label", b->label, true);
        if (b->fp.res_measured) kv_int(o, "mean_loop_mohm", (long)(b->loop_sum_mohm / (b->count ? b->count : 1)), true);
        key(o, "spec");
        spec(o, &b->fp);
        raw(o, "}");
    }
    raw(o, "],");
    key(o, "faulty");
    raw(o, "[");
    int kept = s->faulty_total < KAT_MAX_FAULTY ? s->faulty_total : KAT_MAX_FAULTY;
    for (int k = 0; k < kept; k++) {   /* newest first */
        const kat_faulty_t *f = &s->faulty[(s->faulty_next + KAT_MAX_FAULTY - 1 - k) % KAT_MAX_FAULTY];
        if (k) raw(o, ",");
        raw(o, "{");
        kv_str(o, "label", f->label, true);
        kv_str(o, "reason", f->reason, false);
        raw(o, "}");
    }
    raw(o, "]}");
}

static const char *em_status(kat_em_status_t st)
{
    switch (st) {
    case KAT_EM_OK: return "ok";
    case KAT_EM_NO_REPLY: return "no reply";
    case KAT_EM_NAK: return "NAK";
    case KAT_EM_NOT_CABLE: return "not a cable";
    case KAT_EM_BAD: return "bad reply";
    }
    return "?";
}

void kat_json_last(const kat_session_t *s, const kat_cable_t *c, const kat_add_result_t *r, bool have,
                   uint32_t seq, kat_sink_t o)
{
    raw(o, "{");
    kv_int(o, "seq", (long)seq, true);
    kv_bool(o, "have", have, !!have);
    if (!have) {
        raw(o, "}");
        return;
    }
    kat_fingerprint_t fp;
    char label[KAT_LABEL_LEN], reason[KAT_REASON_LEN], letter[4] = "";
    kat_fingerprint(c, &fp);
    kat_label(&fp, label, sizeof label);
    kat_reason(c, reason, sizeof reason);
    if (r->bag >= 0) kat_bag_letter(r->bag, letter);

    key(o, "result");
    raw(o, "{");
    kv_int(o, "bag", r->bag, true);
    kv_str(o, "letter", letter, true);
    if (r->bag >= 0) fmt(o, "\"colour\":\"#%06lX\",", (unsigned long)kat_bag_colour(r->bag)->rgb);
    kv_int(o, "count", r->bag >= 0 && r->bag < s->bag_count ? s->bags[r->bag].count : 0, true);
    kv_bool(o, "new_bag", r->new_bag, true);
    kv_bool(o, "faulty", r->faulty, true);
    kv_bool(o, "full", r->full, false);
    raw(o, "},");
    kv_str(o, "label", label, true);
    kv_str(o, "reason", reason, true);
    key(o, "spec");
    spec(o, &fp);
    raw(o, ",");

    const kat_continuity_t *k = &c->cont;
    kv_int(o, "flip_a", k->flip_a, true);
    kv_int(o, "flip_b", k->flip_b, true);
    kv_bool(o, "ra", k->ra, true);
    key(o, "nets");
    raw(o, "[");
    uint8_t n = 0;
    const kat_net_t *nets = kat_kind_nets(k->kind, &n);
    for (int i = 0; i < n; i++) {
        if (i) raw(o, ",");
        raw(o, "{");
        kv_str(o, "name", nets[i].name, true);
        kv_bool(o, "present", k->present & (1u << i), false);
        raw(o, "}");
    }
    raw(o, "],");
    if (k->kind == KAT_KIND_RJ45) {
        key(o, "rj45_map");
        raw(o, "[");
        for (int i = 0; i < 8; i++) fmt(o, i ? ",%u" : "%u", k->rj45_map[i]);
        raw(o, "],");
    }
    key(o, "faults");
    raw(o, "[");
    for (int i = 0; i < k->fault_count; i++) {
        if (i) raw(o, ",");
        str(o, k->faults[i]);
    }
    raw(o, "],");

    key(o, "emarker_reply");
    raw(o, "{");
    kv_str(o, "status", em_status(c->em.status), true);
    key(o, "vdos");
    raw(o, "[");
    for (int i = 0; i < c->em.vdo_count; i++) fmt(o, i ? ",\"%08lX\"" : "\"%08lX\"", (unsigned long)c->em.vdo[i]);
    raw(o, "]},");

    key(o, "resistance");
    raw(o, "{");
    kv_bool(o, "measured", c->res.measured, true);
    kv_int(o, "loop_mohm", (long)c->res.loop_mohm, true);
    kv_int(o, "copper_amps", c->res.copper_amps, true);
    kv_int(o, "awg", c->res.awg, true);
    kv_bool(o, "too_thin", c->res.claims_too_much, false);
    raw(o, "}}");
}

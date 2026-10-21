#include "katooni/session.h"

#include <string.h>

#define MAGIC 0x494E544Bu   /* "KTNI" */
#define VERSION 1u

typedef struct {
    uint32_t magic, version, size, crc;
} blob_header_t;

void kat_config_default(kat_config_t *cfg)
{
    cfg->tol_mohm = 25;
    cfg->tol_pct = 15;
    kat_res_config_default(&cfg->res);
}

void kat_session_clear(kat_session_t *s)
{
    memset(s, 0, sizeof *s);
}

kat_add_result_t kat_session_add(kat_session_t *s, const kat_cable_t *c, const kat_config_t *cfg)
{
    kat_add_result_t r = {.bag = -1};
    s->tested++;
    if (kat_cable_faulty(c)) {
        kat_faulty_t *f = &s->faulty[s->faulty_next];
        kat_fingerprint_t fp;
        kat_fingerprint(c, &fp);
        kat_label(&fp, f->label, sizeof f->label);
        kat_reason(c, f->reason, sizeof f->reason);
        s->faulty_next = (uint8_t)((s->faulty_next + 1) % KAT_MAX_FAULTY);
        s->faulty_total++;
        r.faulty = true;
        return r;
    }
    kat_fingerprint_t fp;
    kat_fingerprint(c, &fp);
    for (int i = 0; i < s->bag_count; i++) {
        if (kat_fingerprint_match(&s->bags[i].fp, &fp, cfg->tol_mohm, cfg->tol_pct)) {
            s->bags[i].count++;
            s->bags[i].loop_sum_mohm += fp.loop_mohm;
            r.bag = i;
            return r;
        }
    }
    if (s->bag_count >= KAT_MAX_BAGS) {
        r.full = true;
        return r;
    }
    kat_bag_t *b = &s->bags[s->bag_count];
    memset(b, 0, sizeof *b);
    b->fp = fp;
    b->count = 1;
    b->loop_sum_mohm = fp.loop_mohm;
    kat_label(&fp, b->label, sizeof b->label);
    r.bag = s->bag_count++;
    r.new_bag = true;
    return r;
}

void kat_bag_letter(int index, char out[4])
{
    if (index < 0) {
        out[0] = '?';
        out[1] = 0;
    } else if (index < 26) {
        out[0] = (char)('A' + index);
        out[1] = 0;
    } else {
        out[0] = (char)('A' + (index / 26 - 1) % 26);
        out[1] = (char)('A' + index % 26);
        out[2] = 0;
    }
}

/* Twelve colours that stay apart on a TFT and for most colour-blind eyes; the letter is always shown
 * too, because colours repeat after twelve bags. */
static const kat_colour_t PALETTE[KAT_PALETTE_SIZE] = {
    {"blue", 0x1F77B4}, {"orange", 0xFF7F0E}, {"green", 0x2CA02C}, {"red", 0xD62728},
    {"purple", 0x9467BD}, {"brown", 0x8C564B}, {"pink", 0xE377C2}, {"grey", 0x7F7F7F},
    {"olive", 0xBCBD22}, {"cyan", 0x17BECF}, {"navy", 0x1A2A6C}, {"yellow", 0xFFD700},
};

const kat_colour_t *kat_bag_colour(int index)
{
    return &PALETTE[(index < 0 ? 0 : index) % KAT_PALETTE_SIZE];
}

uint16_t kat_rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

uint32_t kat_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}

size_t kat_session_blob_size(void)
{
    return sizeof(blob_header_t) + sizeof(kat_session_t);
}

size_t kat_session_save(const kat_session_t *s, uint8_t *buf, size_t cap)
{
    if (cap < kat_session_blob_size()) return 0;
    blob_header_t h = {MAGIC, VERSION, sizeof *s, kat_crc32((const uint8_t *)s, sizeof *s)};
    memcpy(buf, &h, sizeof h);
    memcpy(buf + sizeof h, s, sizeof *s);
    return kat_session_blob_size();
}

bool kat_session_load(kat_session_t *s, const uint8_t *buf, size_t len)
{
    blob_header_t h;
    if (!buf || len != kat_session_blob_size()) return false;
    memcpy(&h, buf, sizeof h);
    if (h.magic != MAGIC || h.version != VERSION || h.size != sizeof *s) return false;
    if (kat_crc32(buf + sizeof h, sizeof *s) != h.crc) return false;
    /* Straight into *s: the session is several kB, too big for a copy on a task's stack. */
    memcpy(s, buf + sizeof h, sizeof *s);
    if (s->bag_count > KAT_MAX_BAGS || s->faulty_next >= KAT_MAX_FAULTY) {
        kat_session_clear(s);
        return false;
    }
    return true;
}

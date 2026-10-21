/* A sorting session: bags of identical cables (KATOO-5).
 *
 * The first cable with a new fingerprint opens a bag; matching cables after it join that bag. Faulty
 * cables are counted, and the latest few kept for the web page, but never bagged. The session is
 * plain data so the firmware can keep it in NVS across a power blip. */
#ifndef KATOONI_SESSION_H
#define KATOONI_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "katooni/cable.h"

#define KAT_MAX_BAGS 48
#define KAT_MAX_FAULTY 16
#define KAT_LABEL_LEN 48
#define KAT_REASON_LEN 80
#define KAT_PALETTE_SIZE 12

typedef struct {
    uint16_t tol_mohm;     /* resistance band: at least this many mΩ either side... */
    uint8_t tol_pct;       /* ...or this per cent of the bag's first cable, whichever is larger */
    kat_res_config_t res;
} kat_config_t;

void kat_config_default(kat_config_t *cfg);

typedef struct {
    kat_fingerprint_t fp;      /* the first cable's; later cables are matched against it */
    uint16_t count;
    uint32_t loop_sum_mohm;    /* for the mean shown on the web page */
    char label[KAT_LABEL_LEN];
} kat_bag_t;

typedef struct {
    char label[KAT_LABEL_LEN];
    char reason[KAT_REASON_LEN];
} kat_faulty_t;

typedef struct {
    uint32_t tested;
    uint16_t bag_count;
    uint16_t faulty_total;
    uint8_t faulty_next;           /* ring buffer of the latest faulty cables */
    kat_faulty_t faulty[KAT_MAX_FAULTY];
    kat_bag_t bags[KAT_MAX_BAGS];
} kat_session_t;

typedef struct {
    int bag;           /* bag index, or -1 */
    bool new_bag;
    bool faulty;
    bool full;         /* would have opened a new bag, but there's no room */
} kat_add_result_t;

void kat_session_clear(kat_session_t *s);
kat_add_result_t kat_session_add(kat_session_t *s, const kat_cable_t *c, const kat_config_t *cfg);

/* A..Z, then AA, AB... */
void kat_bag_letter(int index, char out[4]);

typedef struct {
    const char *name;
    uint32_t rgb;      /* 0xRRGGBB */
} kat_colour_t;

const kat_colour_t *kat_bag_colour(int index);
uint16_t kat_rgb565(uint32_t rgb);

/* NVS blob: a small header (magic, version, size, CRC-32) then the session. Returns the bytes
 * written, or 0 if cap is too small. */
size_t kat_session_blob_size(void);
size_t kat_session_save(const kat_session_t *s, uint8_t *buf, size_t cap);
bool kat_session_load(kat_session_t *s, const uint8_t *buf, size_t len);

uint32_t kat_crc32(const uint8_t *p, size_t n);

#endif

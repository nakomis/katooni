/* The web page's JSON (KATOO-7), shared by the firmware's server and the Mac simulator. Written to a
 * sink, so the device can stream it in chunks rather than hold the whole session in RAM.
 *
 * GET /api/session -> kat_json_session: every bag (letter, colour, count, label, spec), the latest
 *                     faulty cables (newest first), and the tallies.
 * GET /api/last    -> kat_json_last: the last cable in full (nets, faults, e-marker, resistance).
 */
#ifndef KATOONI_JSON_H
#define KATOONI_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "katooni/session.h"

typedef struct {
    void (*write)(void *ctx, const char *s, size_t n);
    void *ctx;
} kat_sink_t;

void kat_json_session(const kat_session_t *s, kat_sink_t out);

/* have: whether there's a last cable at all; seq bumps with every cable, so a page can poll cheaply. */
void kat_json_last(const kat_session_t *s, const kat_cable_t *c, const kat_add_result_t *r, bool have,
                   uint32_t seq, kat_sink_t out);

/* A sink into a fixed buffer, always NUL-terminated; overflow is recorded rather than written. */
typedef struct {
    char *buf;
    size_t cap, len;
    bool overflow;
} kat_buf_t;

void kat_buf_write(void *ctx, const char *s, size_t n);
static inline kat_sink_t kat_buf_sink(kat_buf_t *b)
{
    kat_sink_t k = {kat_buf_write, b};
    return k;
}

#endif

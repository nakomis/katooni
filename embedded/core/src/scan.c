#include "katooni/scan.h"

void kat_scan_clear(kat_scan_t *s)
{
    for (int i = 0; i < KAT_LINE_COUNT; i++) s->parent[i] = (uint8_t)i;
}

int kat_scan_root(const kat_scan_t *s, int line)
{
    if (line < 0 || line >= KAT_LINE_COUNT) return -1;
    while (s->parent[line] != line) line = s->parent[line];
    return line;
}

void kat_scan_join(kat_scan_t *s, int line_a, int line_b)
{
    int a = kat_scan_root(s, line_a), b = kat_scan_root(s, line_b);
    if (a < 0 || b < 0 || a == b) return;
    /* The smaller number becomes the root, so equal scans give equal parent arrays. */
    if (a < b) s->parent[b] = (uint8_t)a;
    else s->parent[a] = (uint8_t)b;
}

bool kat_scan_joined(const kat_scan_t *s, int line_a, int line_b)
{
    int a = kat_scan_root(s, line_a);
    return a >= 0 && a == kat_scan_root(s, line_b);
}

bool kat_scan_any_cable(const kat_scan_t *s)
{
    for (int a = 0; a < KAT_LINE_COUNT; a++) {
        if (kat_port(kat_line_port(a))->end != KAT_END_A) continue;
        for (int b = 0; b < KAT_LINE_COUNT; b++)
            if (kat_port(kat_line_port(b))->end == KAT_END_B && kat_scan_joined(s, a, b)) return true;
    }
    return false;
}

bool kat_scan_equal(const kat_scan_t *x, const kat_scan_t *y)
{
    for (int i = 0; i < KAT_LINE_COUNT; i++)
        if (kat_scan_root(x, i) != kat_scan_root(y, i)) return false;
    return true;
}

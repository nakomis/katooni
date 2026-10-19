/* The 2.0" ST7789 (240×320, portrait): splash, status, and the bag for each cable. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "katooni/classify.h"

esp_err_t display_init(void);

/* The Katooni logo and the firmware's name and version. */
void display_splash(const char *version);

/* A title and a line or two of text on a plain background, e.g. "Ready" / "Plug a cable into both ends". */
void display_status(const char *title, const char *body);

typedef struct {
    char letter[4];         /* bag letter, "!" when faulty */
    uint32_t rgb;           /* panel colour */
    bool new_bag, faulty;
    uint16_t count;         /* cables in the bag so far */
    const char *label;      /* Dymo label text */
    const char *reason;     /* one line: why */
    const kat_continuity_t *cont;
} display_result_t;

void display_result(const display_result_t *r);

#include "display.h"

#include <string.h>

#include "assets/fonts.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define W 240
#define H 320
#define CHUNK_LINES 40
#define BUF_PIXELS (W * CHUNK_LINES)

extern const uint8_t logo_start[] asm("_binary_logo_240_rgb565_start");

static const char *TAG = "display";
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_buf;                 /* DMA-capable; one transfer in flight at a time */
static SemaphoreHandle_t s_done;

#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREY 0x8410
#define DARK 0x2104
#define GREEN 0x2E8B
#define RED 0xD8A4

static uint16_t be(uint16_t c)     /* the panel takes RGB565 big-endian */
{
    return (uint16_t)((c >> 8) | (c << 8));
}

static uint16_t rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

static bool light(uint32_t rgb)    /* pick black text on light panels */
{
    unsigned r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    return r * 299 + g * 587 + b * 114 > 150000;
}

static bool IRAM_ATTR on_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}

static void blit(int x, int y, int w, int h)
{
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, s_buf);
    xSemaphoreTake(s_done, portMAX_DELAY);
}

static void fill(int x, int y, int w, int h, uint16_t c)
{
    c = be(c);
    int lines = BUF_PIXELS / w;
    for (int i = 0; i < w * lines; i++) s_buf[i] = c;
    for (int row = y; row < y + h; row += lines) blit(x, row, w, row + lines > y + h ? y + h - row : lines);
}

esp_err_t display_init(void)
{
    s_done = xSemaphoreCreateBinary();
    s_buf = heap_caps_malloc(BUF_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(s_done && s_buf, ESP_ERR_NO_MEM, TAG, "no memory");

    gpio_config_t bl = {.pin_bit_mask = 1ULL << KAT_PIN_LCD_BL, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&bl);
    gpio_set_level(KAT_PIN_LCD_BL, 0);

    spi_bus_config_t bus = {
        .mosi_io_num = KAT_PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = KAT_PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUF_PIXELS * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(KAT_LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = KAT_PIN_LCD_CS,
        .dc_gpio_num = KAT_PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = KAT_LCD_HZ,
        .trans_queue_depth = 2,
        .on_color_trans_done = on_done,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)KAT_LCD_HOST, &io_cfg, &io), TAG, "io");
    esp_lcd_panel_dev_config_t p = {
        .reset_gpio_num = KAT_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io, &p, &s_panel), TAG, "panel");
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);   /* IPS ST7789 panels want inversion on */
    esp_lcd_panel_disp_on_off(s_panel, true);
    fill(0, 0, W, H, BLACK);
    gpio_set_level(KAT_PIN_LCD_BL, 1);
    return ESP_OK;
}

/* ---- text --------------------------------------------------------------------------------- */

static int next_cp(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    int cp, n;
    if (p[0] < 0x80) cp = p[0], n = 1;
    else if ((p[0] & 0xE0) == 0xC0 && p[1]) cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F), n = 2;
    else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F), n = 3;
    else cp = '?', n = 1;
    *s += n;
    return cp;
}

static int glyph_index(const uint32_t *cps, int count, int cp)
{
    for (int i = 0; i < count; i++)
        if (cps[i] == (uint32_t)cp) return i;
    for (int i = 0; i < count; i++)
        if (cps[i] == '?') return i;
    return 0;
}

static void draw_glyph(int x, int y, const uint8_t *bits, int w, int h, int stride, uint16_t fg, uint16_t bg)
{
    fg = be(fg);
    bg = be(bg);
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            s_buf[yy * w + xx] = bits[yy * stride + xx / 8] & (0x80 >> (xx % 8)) ? fg : bg;
    blit(x, y, w, h);
}

/* Small text from (x, y), wrapped at word boundaries within width w; returns the y after it. */
static int text(int x, int y, int w, const char *s, uint16_t fg, uint16_t bg, int max_lines)
{
    int cols = w / FONT_SMALL_W, line = 0;
    while (*s && line < max_lines) {
        /* Find how many characters (code points) fit, breaking after a space if we can. */
        const char *p = s, *brk = NULL, *brk_next = NULL;
        int n = 0;
        while (*p && n < cols) {
            const char *q = p;
            int cp = next_cp(&q);
            if (cp == ' ') brk = p, brk_next = q;
            p = q;
            n++;
        }
        const char *end = (*p && brk) ? brk : p, *resume = (*p && brk) ? brk_next : p;
        int cx = x;
        for (const char *q = s; q < end;) {
            int i = glyph_index(font_small_codepoints, FONT_SMALL_COUNT, next_cp(&q));
            draw_glyph(cx, y, font_small_bits[i], FONT_SMALL_W, FONT_SMALL_H, FONT_SMALL_STRIDE, fg, bg);
            cx += FONT_SMALL_W;
        }
        s = resume;
        y += FONT_SMALL_H + 2;
        line++;
    }
    return y;
}

static void big_letters(int cx, int y, const char *s, uint16_t fg, uint16_t bg)
{
    int n = (int)strlen(s), x = cx - n * FONT_BIG_W / 2;
    for (int i = 0; i < n; i++) {
        int g = glyph_index(font_big_codepoints, FONT_BIG_COUNT, (unsigned char)s[i]);
        draw_glyph(x + i * FONT_BIG_W, y, font_big_bits[g], FONT_BIG_W, FONT_BIG_H, FONT_BIG_STRIDE, fg, bg);
    }
}

/* ---- screens ------------------------------------------------------------------------------ */

void display_splash(const char *version)
{
    const uint16_t *logo = (const uint16_t *)logo_start;
    for (int row = 0; row < 240; row += CHUNK_LINES) {
        memcpy(s_buf, logo + row * W, W * CHUNK_LINES * sizeof(uint16_t));
        blit(0, row, W, CHUNK_LINES);
    }
    fill(0, 240, W, H - 240, BLACK);
    text(8, 252, W - 16, "Katooni", WHITE, BLACK, 1);
    text(8, 280, W - 16, version, GREY, BLACK, 1);
}

void display_status(const char *title, const char *body)
{
    fill(0, 0, W, H, DARK);
    int y = text(12, 120, W - 24, title, WHITE, DARK, 2);
    text(12, y + 8, W - 24, body, GREY, DARK, 6);
}

/* One square per conductor the cable type should have: green continuous, grey not. */
static void conductor_map(int y, const kat_continuity_t *c)
{
    uint8_t n = 0;
    kat_kind_nets(c->kind, &n);
    if (!n) return;
    int size = 12, gap = 2, x = (W - n * (size + gap)) / 2;
    for (int i = 0; i < n; i++) fill(x + i * (size + gap), y, size, size, (c->present & (1u << i)) ? GREEN : GREY);
}

void display_result(const display_result_t *r)
{
    uint32_t rgb = r->faulty ? 0xD62728 : r->rgb;
    uint16_t panel = rgb565(rgb), ink = light(rgb) ? BLACK : WHITE;
    fill(0, 0, W, 150, panel);
    big_letters(W / 2, 20, r->faulty ? "?" : r->letter, ink, panel);
    char tag[24];
    if (r->faulty) strcpy(tag, "FAULTY");
    else if (r->new_bag) strcpy(tag, "NEW BAG");
    else snprintf(tag, sizeof tag, "x%u", r->count);
    text(8, 4, W - 16, tag, ink, panel, 1);

    fill(0, 150, W, H - 150, BLACK);
    int y = text(8, 158, W - 16, r->label ? r->label : "", WHITE, BLACK, 2);
    text(8, y + 6, W - 16, r->reason ? r->reason : "", r->faulty ? RED : GREY, BLACK, 4);
    if (r->cont) conductor_map(H - 20, r->cont);
}

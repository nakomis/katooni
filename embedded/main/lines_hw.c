#include "lines_hw.h"

#include "board_pins.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "hw_i2c.h"

/* Registers as 16-bit pairs (low byte port A / port 0). A pull-up register of 0 means the part's
 * pull-ups can't be switched off. */
#if KAT_EXPANDER_TYPE == KAT_EXPANDER_MCP23017
/* IOCON.BANK = 0 (the reset default): A and B registers are adjacent. GPA7 and GPB7 are output-only
 * on current datasheets, so they're driven low and never carry a line (board_pins.h). */
#define REG_DIR 0x00    /* IODIR: 1 = input */
#define REG_PULLUP 0x0C /* GPPU */
#define REG_IN 0x12     /* GPIO */
#define REG_OUT 0x14    /* OLAT */
#define OUTPUT_ONLY 0x8080
#elif KAT_EXPANDER_TYPE == KAT_EXPANDER_TCA9555
/* The TCA9555's ~100 kΩ pull-ups are fixed (check the part on the board: the TCA9535 has none). */
#define REG_IN 0x00
#define REG_OUT 0x02
#define REG_DIR 0x06    /* configuration: 1 = input */
#define REG_PULLUP 0
#define OUTPUT_ONLY 0x0000
#else
#error "KAT_EXPANDER_TYPE must be KAT_EXPANDER_MCP23017 or KAT_EXPANDER_TCA9555"
#endif

/* A long cable's capacitance against ~100 kΩ pull-ups: give each line time to settle. */
#define SETTLE_US 200
#define NONE 0xFF

static const char *TAG = "lines";
static i2c_master_dev_handle_t s_dev[KAT_EXPANDER_COUNT];
static uint16_t s_dir[KAT_EXPANDER_COUNT];   /* idle direction: every line an input */
static struct {
    uint8_t exp, bit;
} s_map[KAT_LINE_COUNT];

static esp_err_t wr16(int e, uint8_t reg, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)(v & 0xFF), (uint8_t)(v >> 8)};
    return hw_i2c_write_reg(s_dev[e], reg, b, 2);
}

static esp_err_t rd16(int e, uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    esp_err_t err = hw_i2c_read_reg(s_dev[e], reg, b, 2);
    *v = (uint16_t)(b[0] | (b[1] << 8));
    return err;
}

/* The map must put each line on its own pin, on an expander that exists, never on an output-only pin. */
static esp_err_t load_map(void)
{
    uint16_t used[KAT_EXPANDER_COUNT] = {0};
    int mapped = 0;
    for (int n = 0; n < KAT_LINE_COUNT; n++) {
        uint8_t e, b;
        s_map[n].exp = s_map[n].bit = NONE;
        if (!kat_line_map(n, &e, &b)) continue;
        uint16_t m = (uint16_t)(1u << (b & 15));
        ESP_RETURN_ON_FALSE(e < KAT_EXPANDER_COUNT && b < 16 && !(m & OUTPUT_ONLY) && !(used[e] & m),
                            ESP_ERR_INVALID_ARG, TAG, "line %d: bad pin (expander %u bit %u)", n, e, b);
        used[e] |= m;
        s_map[n].exp = e;
        s_map[n].bit = b;
        mapped++;
    }
    ESP_LOGI(TAG, "%d of %d lines mapped", mapped, KAT_LINE_COUNT);
    return ESP_OK;
}

esp_err_t lines_init(void)
{
    static const uint8_t addrs[KAT_EXPANDER_COUNT] = KAT_EXPANDER_ADDRS;
    ESP_RETURN_ON_ERROR(load_map(), TAG, "line map");
    for (int e = 0; e < KAT_EXPANDER_COUNT; e++) {
        ESP_RETURN_ON_ERROR(hw_i2c_add(addrs[e], &s_dev[e]), TAG, "add expander %d", e);
        /* Outputs latch low; every line an input with its pull-up, so nothing is driven. */
        ESP_RETURN_ON_ERROR(wr16(e, REG_OUT, 0x0000), TAG, "expander 0x%02x not answering", addrs[e]);
        if (REG_PULLUP) ESP_RETURN_ON_ERROR(wr16(e, REG_PULLUP, 0xFFFF), TAG, "pull-ups");
        s_dir[e] = (uint16_t)~OUTPUT_ONLY;
        ESP_RETURN_ON_ERROR(wr16(e, REG_DIR, s_dir[e]), TAG, "direction");
    }
    return ESP_OK;
}

esp_err_t lines_quiet(bool quiet)
{
    if (!REG_PULLUP) return ESP_OK;
    for (int e = 0; e < KAT_EXPANDER_COUNT; e++)
        ESP_RETURN_ON_ERROR(wr16(e, REG_PULLUP, quiet ? 0x0000 : 0xFFFF), TAG, "pull-ups");
    return ESP_OK;
}

static esp_err_t read_all(uint16_t in[KAT_EXPANDER_COUNT])
{
    for (int e = 0; e < KAT_EXPANDER_COUNT; e++) ESP_RETURN_ON_ERROR(rd16(e, REG_IN, &in[e]), TAG, "read");
    return ESP_OK;
}

static bool low(const uint16_t in[KAT_EXPANDER_COUNT], int line)
{
    return s_map[line].exp != NONE && !(in[s_map[line].exp] & (1u << s_map[line].bit));
}

esp_err_t lines_scan(kat_scan_t *scan, int *stuck_low)
{
    uint16_t base[KAT_EXPANDER_COUNT], in[KAT_EXPANDER_COUNT];
    kat_scan_clear(scan);
    ESP_RETURN_ON_ERROR(read_all(base), TAG, "baseline");
    int stuck = 0;
    for (int l = 0; l < KAT_LINE_COUNT; l++) stuck += low(base, l);
    if (stuck_low) *stuck_low = stuck;

    for (int l = 0; l < KAT_LINE_COUNT; l++) {
        /* Unmapped, already joined to an earlier line (same group: driving it shows nothing new), or
         * already low with nothing driven. */
        if (s_map[l].exp == NONE || kat_scan_root(scan, l) != l || low(base, l)) continue;
        int e = s_map[l].exp;
        uint16_t dir = s_dir[e] & (uint16_t)~(1u << s_map[l].bit);
        ESP_RETURN_ON_ERROR(wr16(e, REG_DIR, dir), TAG, "drive");
        esp_rom_delay_us(SETTLE_US);
        esp_err_t err = read_all(in);
        ESP_RETURN_ON_ERROR(wr16(e, REG_DIR, s_dir[e]), TAG, "release");
        ESP_RETURN_ON_ERROR(err, TAG, "read");
        for (int j = 0; j < KAT_LINE_COUNT; j++)
            if (j != l && low(in, j) && !low(base, j)) kat_scan_join(scan, l, j);
    }
    return ESP_OK;
}

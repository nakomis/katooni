#include "lines_hw.h"

#include "board_pins.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "hw_i2c.h"

/* MCP23017 registers with IOCON.BANK = 0 (the reset default): A and B pairs are adjacent, so one
 * two-byte access covers all 16 pins, port A in the low byte. */
#define REG_IODIR 0x00
#define REG_GPPU 0x0C
#define REG_GPIO 0x12
#define REG_OLAT 0x14

/* A long cable's capacitance against ~100 kΩ pull-ups: give each line time to settle. */
#define SETTLE_US 200

static const char *TAG = "lines";
static i2c_master_dev_handle_t s_dev[KAT_EXPANDER_COUNT];
static uint16_t s_iodir[KAT_EXPANDER_COUNT];
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

esp_err_t lines_init(void)
{
    static const uint8_t addrs[KAT_EXPANDER_COUNT] = KAT_EXPANDER_ADDRS;
    for (int n = 0; n < KAT_LINE_COUNT; n++) {
        const uint8_t m[2] = KAT_LINE_MAP_ENTRY(n);
        s_map[n].exp = m[0];
        s_map[n].bit = m[1];
    }
    for (int e = 0; e < KAT_EXPANDER_COUNT; e++) {
        ESP_RETURN_ON_ERROR(hw_i2c_add(addrs[e], &s_dev[e]), TAG, "add expander %d", e);
        /* Outputs latch low; every pin an input with its pull-up, so nothing is driven. */
        ESP_RETURN_ON_ERROR(wr16(e, REG_OLAT, 0x0000), TAG, "expander 0x%02x not answering", addrs[e]);
        ESP_RETURN_ON_ERROR(wr16(e, REG_GPPU, 0xFFFF), TAG, "pull-ups");
        ESP_RETURN_ON_ERROR(wr16(e, REG_IODIR, 0xFFFF), TAG, "iodir");
        s_iodir[e] = 0xFFFF;
    }
    return ESP_OK;
}

esp_err_t lines_quiet(bool quiet)
{
    for (int e = 0; e < KAT_EXPANDER_COUNT; e++)
        ESP_RETURN_ON_ERROR(wr16(e, REG_GPPU, quiet ? 0x0000 : 0xFFFF), TAG, "pull-ups");
    return ESP_OK;
}

static esp_err_t read_all(uint16_t in[KAT_EXPANDER_COUNT])
{
    for (int e = 0; e < KAT_EXPANDER_COUNT; e++) ESP_RETURN_ON_ERROR(rd16(e, REG_GPIO, &in[e]), TAG, "read");
    return ESP_OK;
}

static bool low(const uint16_t in[KAT_EXPANDER_COUNT], int line)
{
    return !(in[s_map[line].exp] & (1u << s_map[line].bit));
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
        /* A line already joined to an earlier one is in the same group: driving it shows nothing new. */
        if (kat_scan_root(scan, l) != l || low(base, l)) continue;
        int e = s_map[l].exp;
        uint16_t dir = s_iodir[e] & (uint16_t)~(1u << s_map[l].bit);
        ESP_RETURN_ON_ERROR(wr16(e, REG_IODIR, dir), TAG, "drive");
        esp_rom_delay_us(SETTLE_US);
        esp_err_t err = read_all(in);
        ESP_RETURN_ON_ERROR(wr16(e, REG_IODIR, s_iodir[e]), TAG, "release");
        ESP_RETURN_ON_ERROR(err, TAG, "read");
        for (int j = 0; j < KAT_LINE_COUNT; j++)
            if (j != l && low(in, j) && !low(base, j)) kat_scan_join(scan, l, j);
    }
    return ESP_OK;
}

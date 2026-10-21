/* Written from the FUSB302B datasheet; not yet run against a chip (no board yet). */
#include "fusb302.h"

#include "board_pins.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hw_i2c.h"

#define REG_DEVICE_ID 0x01
#define REG_SWITCHES0 0x02
#define REG_SWITCHES1 0x03
#define REG_CONTROL0 0x06
#define REG_CONTROL1 0x07
#define REG_CONTROL3 0x09
#define REG_POWER 0x0B
#define REG_RESET 0x0C
#define REG_STATUS1 0x41
#define REG_INTERRUPTA 0x3E
#define REG_FIFOS 0x43

#define SW0_PU_EN2 0x80
#define SW0_PU_EN1 0x40
#define SW0_VCONN_CC2 0x20
#define SW0_VCONN_CC1 0x10
#define SW0_MEAS_CC2 0x08
#define SW0_MEAS_CC1 0x04
#define SW1_POWERROLE 0x80
#define SW1_SPECREV_30 0x40
#define SW1_DATAROLE 0x10
#define SW1_AUTO_CRC 0x04
#define SW1_TXCC2 0x02
#define SW1_TXCC1 0x01
#define CTL0_TX_FLUSH 0x40
#define CTL0_HOST_CUR_USB 0x04
#define CTL1_RX_FLUSH 0x04
#define CTL1_ENSOP1 0x01
#define CTL3_RETRIES_3 0x06
#define CTL3_AUTO_RETRY 0x01
#define STATUS1_RX_EMPTY 0x20
#define INTA_RETRYFAIL 0x10
#define INTA_TXSENT 0x04

/* TX FIFO tokens. SOP' is Sync-1, Sync-1, Sync-3, Sync-3. */
#define TK_SOP1 0x12
#define TK_SOP3 0x1B
#define TK_PACKSYM 0x80
#define TK_JAM_CRC 0xFF
#define TK_EOP 0x14
#define TK_TXOFF 0xFE
#define TK_TXON 0xA1
#define RX_TOKEN_SOP1 0xC0   /* RX token B7..5 = 110: SOP' */

#define MSG_VENDOR_DEFINED 0x0F
#define MSG_GOODCRC 0x01

static const char *TAG = "fusb302";
static i2c_master_dev_handle_t s_dev;

static esp_err_t wr(uint8_t reg, uint8_t v)
{
    return hw_i2c_write_reg(s_dev, reg, &v, 1);
}

static esp_err_t rd(uint8_t reg, uint8_t *v)
{
    return hw_i2c_read_reg(s_dev, reg, v, 1);
}

esp_err_t fusb302_init(void)
{
    ESP_RETURN_ON_ERROR(hw_i2c_add(KAT_FUSB302_ADDR, &s_dev), TAG, "add");
    ESP_RETURN_ON_ERROR(wr(REG_RESET, 0x01), TAG, "not answering");   /* SW_RES */
    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(rd(REG_DEVICE_ID, &id), TAG, "device id");
    ESP_LOGI(TAG, "device id 0x%02x", id);
    ESP_RETURN_ON_ERROR(wr(REG_SWITCHES0, 0), TAG, "switches");        /* VCONN and Rp off */
    return wr(REG_POWER, 0x01);                                        /* bandgap only until needed */
}

static void off(void)
{
    wr(REG_SWITCHES0, 0);
    wr(REG_SWITCHES1, 0);
    wr(REG_POWER, 0x01);
}

int fusb302_discover_identity(int cc, uint32_t objs[7])
{
    uint8_t sw0 = cc == 2 ? (SW0_PU_EN2 | SW0_MEAS_CC2 | SW0_VCONN_CC1) : (SW0_PU_EN1 | SW0_MEAS_CC1 | SW0_VCONN_CC2);
    uint8_t sw1 = SW1_POWERROLE | SW1_SPECREV_30 | SW1_DATAROLE | SW1_AUTO_CRC | (cc == 2 ? SW1_TXCC2 : SW1_TXCC1);
    if (wr(REG_POWER, 0x0F) || wr(REG_CONTROL0, CTL0_HOST_CUR_USB | CTL0_TX_FLUSH) ||
        wr(REG_CONTROL3, CTL3_RETRIES_3 | CTL3_AUTO_RETRY) || wr(REG_SWITCHES0, sw0) || wr(REG_SWITCHES1, sw1) ||
        wr(REG_CONTROL1, CTL1_ENSOP1 | CTL1_RX_FLUSH)) {
        off();
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(50));   /* tVCONNStable: give the e-marker time to wake */

    /* Header: Vendor_Defined, 1 data object, spec revision 3.0, message ID 0, from the port.
     * VDM header: PD SID, structured VDM version 2.0, REQ, Discover Identity. */
    const uint16_t hdr = MSG_VENDOR_DEFINED | (2u << 6) | (1u << 12);
    const uint32_t vdm = 0xFF00A001u;
    uint8_t tx[] = {
        TK_SOP1, TK_SOP1, TK_SOP3, TK_SOP3, TK_PACKSYM | 6,
        (uint8_t)hdr, (uint8_t)(hdr >> 8),
        (uint8_t)vdm, (uint8_t)(vdm >> 8), (uint8_t)(vdm >> 16), (uint8_t)(vdm >> 24),
        TK_JAM_CRC, TK_EOP, TK_TXOFF, TK_TXON,
    };
    uint8_t inta;
    rd(REG_INTERRUPTA, &inta);       /* clear */
    if (hw_i2c_write_reg(s_dev, REG_FIFOS, tx, sizeof tx)) {
        off();
        return -1;
    }

    int count = 0;
    int64_t until = esp_timer_get_time() + 30 * 1000;
    while (esp_timer_get_time() < until) {
        uint8_t st = 0;
        if (rd(REG_STATUS1, &st)) break;
        if (st & STATUS1_RX_EMPTY) {
            rd(REG_INTERRUPTA, &inta);
            if (inta & INTA_RETRYFAIL) break;     /* nobody GoodCRC'd our request: no e-marker */
            vTaskDelay(1);
            continue;
        }
        uint8_t tok, h[2];
        if (hw_i2c_read_reg(s_dev, REG_FIFOS, &tok, 1) || hw_i2c_read_reg(s_dev, REG_FIFOS, h, 2)) break;
        uint16_t rh = (uint16_t)(h[0] | (h[1] << 8));
        int ndo = (rh >> 12) & 7;
        uint8_t body[7 * 4 + 4];
        if (hw_i2c_read_reg(s_dev, REG_FIFOS, body, (size_t)ndo * 4 + 4)) break;   /* data + CRC */
        if ((tok & 0xE0) != RX_TOKEN_SOP1 || ndo == 0 || (rh & 0x1F) != MSG_VENDOR_DEFINED) continue;
        for (int i = 0; i < ndo; i++)
            objs[i] = (uint32_t)body[4 * i] | ((uint32_t)body[4 * i + 1] << 8) | ((uint32_t)body[4 * i + 2] << 16) |
                      ((uint32_t)body[4 * i + 3] << 24);
        count = ndo;
        break;
    }
    off();
    return count;
}

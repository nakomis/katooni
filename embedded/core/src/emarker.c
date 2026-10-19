#include "katooni/emarker.h"

#include <string.h>

#define PD_SID 0xFF00
#define CMD_DISCOVER_IDENTITY 1
#define CMDT_ACK 1

enum { PT_PASSIVE = 3, PT_ACTIVE = 4 };   /* ID Header VDO B29..27, SOP' product type */

void kat_emarker_none(kat_emarker_t *out)
{
    memset(out, 0, sizeof *out);
    out->status = KAT_EM_NO_REPLY;
    out->max_vbus_mv = 20000;
    out->max_current_ma = 3000;
    out->vbus_through = true;
    out->speed = KAT_SPEED_UNKNOWN;
}

static kat_usb_speed_t speed_of(uint32_t v)
{
    switch (v & 7) {
    case 0: return KAT_SPEED_USB2;
    case 1: return KAT_SPEED_GEN1;
    case 2: return KAT_SPEED_GEN2;
    case 3: return KAT_SPEED_GEN3;
    case 4: return KAT_SPEED_GEN4;
    default: return KAT_SPEED_UNKNOWN;
    }
}

kat_em_status_t kat_emarker_decode(const uint32_t *objs, int count, kat_emarker_t *out)
{
    kat_emarker_none(out);
    if (count <= 0 || !objs) return out->status = KAT_EM_NO_REPLY;
    out->vdo_count = (uint8_t)(count > KAT_EM_MAX_VDOS ? KAT_EM_MAX_VDOS : count);
    memcpy(out->vdo, objs, out->vdo_count * sizeof objs[0]);

    uint32_t hdr = objs[0];
    if ((hdr >> 16) != PD_SID || !(hdr & (1u << 15)) || (hdr & 0x1F) != CMD_DISCOVER_IDENTITY)
        return out->status = KAT_EM_BAD;
    if (((hdr >> 6) & 3) != CMDT_ACK) return out->status = KAT_EM_NAK;
    if (count < 5) return out->status = KAT_EM_BAD;
    out->svdm_major = (uint8_t)((hdr >> 13) & 3);
    out->svdm_minor = (uint8_t)((hdr >> 11) & 3);

    uint32_t id = objs[1];
    unsigned ptype = (id >> 27) & 7;
    if (ptype != PT_PASSIVE && ptype != PT_ACTIVE) return out->status = KAT_EM_NOT_CABLE;
    out->vid = (uint16_t)(id & 0xFFFF);
    out->xid = objs[2];
    out->pid = (uint16_t)(objs[3] >> 16);
    out->bcd_device = (uint16_t)(objs[3] & 0xFFFF);

    uint32_t v = objs[4];
    out->active = ptype == PT_ACTIVE;
    out->hw_ver = (uint8_t)((v >> 28) & 0xF);
    out->fw_ver = (uint8_t)((v >> 24) & 0xF);
    out->vdo_version = (uint8_t)((v >> 21) & 7);
    out->captive = ((v >> 18) & 3) == 3;
    out->latency = (uint8_t)((v >> 13) & 0xF);
    out->termination = (uint8_t)((v >> 11) & 3);
    out->speed = speed_of(v);

    bool pd3 = out->svdm_major >= 1;
    if (pd3) {
        static const uint16_t VBUS_MV[4] = {20000, 30000, 40000, 50000};
        out->epr = (v >> 17) & 1;
        out->max_vbus_mv = VBUS_MV[(v >> 9) & 3];
    }
    /* B6..5: 01 = 3 A, 10 = 5 A. Active cables only carry VBUS when B4 says so. */
    unsigned cur = (v >> 5) & 3;
    out->max_current_ma = cur == 2 ? 5000 : 3000;
    if (out->active) {
        out->vbus_through = (v >> 4) & 1;
        if (!out->vbus_through) out->max_current_ma = 0;
        if (count >= 6) {
            uint32_t v2 = objs[5];
            out->optical = (v2 >> 10) & 1;
            out->retimer = (v2 >> 9) & 1;
            out->usb4 = !((v2 >> 8) & 1);       /* 0 = USB4 supported */
        }
    }
    out->present = true;
    return out->status = KAT_EM_OK;
}

unsigned kat_emarker_watts(const kat_emarker_t *em)
{
    unsigned mv = em->max_vbus_mv >= 50000 ? 48000 : em->max_vbus_mv;
    return (unsigned)((unsigned long)mv * em->max_current_ma / 1000000UL);
}

const char *kat_speed_label(kat_usb_speed_t speed, bool c_to_c)
{
    switch (speed) {
    case KAT_SPEED_USB2: return "USB2";
    case KAT_SPEED_GEN1: return "5G";
    case KAT_SPEED_GEN2: return c_to_c ? "20G" : "10G";
    case KAT_SPEED_GEN3: return "40G";
    case KAT_SPEED_GEN4: return "80G";
    case KAT_SPEED_UNKNOWN: break;
    }
    return "?";
}

const char *kat_latency_label(uint8_t code)
{
    static const char *const L[] = {
        "?", ("\xE2\x89\xA4" "1m"), "2m", "3m", "4m", "5m", "6m", "7m", ">7m", "~200m", "~400m", "~600m",
    };
    return code < sizeof L / sizeof L[0] ? L[code] : "?";
}

int kat_latency_metres(uint8_t code)
{
    return code >= 1 && code <= 7 ? code : 0;
}

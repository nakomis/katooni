/* USB PD cable e-marker: decoding the reply to a Discover Identity sent on SOP' (KATOO-3).
 *
 * Field layouts follow USB PD 3.1/3.2 (structured VDM version 2.x): the VDM header, the ID Header
 * VDO, the Cert Stat VDO, the Product VDO, then one Passive Cable VDO or two Active Cable VDOs. A
 * cable that answers with a version 1.0 VDM (PD 2.0) has no voltage field, so it's read as 20 V. */
#ifndef KATOONI_EMARKER_H
#define KATOONI_EMARKER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    KAT_SPEED_USB2,
    KAT_SPEED_GEN1,     /* USB 3.2 Gen1 */
    KAT_SPEED_GEN2,     /* USB 3.2 / USB4 Gen2 */
    KAT_SPEED_GEN3,     /* USB4 Gen3 */
    KAT_SPEED_GEN4,     /* USB4 Gen4 (PD 3.2) */
    KAT_SPEED_UNKNOWN,
} kat_usb_speed_t;

typedef enum {
    KAT_EM_OK,
    KAT_EM_NO_REPLY,    /* nothing answered on SOP': no e-marker */
    KAT_EM_NAK,         /* NAK or BUSY */
    KAT_EM_NOT_CABLE,   /* answered, but not as a passive or active cable (a VPD, say) */
    KAT_EM_BAD,         /* too short, or not a Discover Identity ACK */
} kat_em_status_t;

#define KAT_EM_MAX_VDOS 7

typedef struct {
    bool present;              /* decoded from a real reply; false means the defaults below */
    kat_em_status_t status;
    bool active;
    bool epr;                  /* EPR mode capable */
    bool captive;              /* cable is captive at the far end */
    bool optical, retimer, usb4; /* active cables (VDO2) */
    bool vbus_through;         /* active cables may not carry VBUS */
    uint8_t svdm_major, svdm_minor;
    uint8_t vdo_version, hw_ver, fw_ver;
    uint8_t latency;           /* raw latency code */
    uint8_t termination;       /* raw cable termination type */
    uint16_t vid, pid, bcd_device;
    uint32_t xid;
    uint16_t max_vbus_mv;
    uint16_t max_current_ma;
    kat_usb_speed_t speed;
    uint8_t vdo_count;
    uint32_t vdo[KAT_EM_MAX_VDOS];  /* the raw reply, VDM header first */
} kat_emarker_t;

/* No e-marker: the Type-C spec's default for a cable without one, 3 A at up to 20 V. */
void kat_emarker_none(kat_emarker_t *out);

/* objs: the data objects of the reply (VDM header first); count: how many. A count of 0 means no
 * reply. Whatever the status, *out is filled in (with the defaults when nothing usable came back). */
kat_em_status_t kat_emarker_decode(const uint32_t *objs, int count, kat_emarker_t *out);

/* What the cable is rated for: EPR cables top out at 48 V, so 50 V × 5 A is 240 W. */
unsigned kat_emarker_watts(const kat_emarker_t *em);

/* "USB2", "5G", "10G"/"20G", "40G", "80G", as on USB-IF cable logos. Two-lane (C-to-C) cables
 * are certified at twice the per-lane Gen2 rate, so a Gen2 C-to-C cable is a "20G" cable. */
const char *kat_speed_label(kat_usb_speed_t speed, bool c_to_c);

/* Rough length from the latency code: "≤1m", "2m" ... ">7m", "~200m" for optical. */
const char *kat_latency_label(uint8_t code);
/* Upper-bound length in metres for a passive cable's latency code, 0 if unknown. */
int kat_latency_metres(uint8_t code);

#endif

#include "check.h"
#include "katooni/lines.h"
#include "katooni/scan.h"

int main(void)
{
    CHECK_INT(KAT_LINE_COUNT, 71);
    int total = 0;
    for (int p = 0; p < KAT_PORT_COUNT; p++) total += kat_port((kat_port_t)p)->line_count;
    CHECK_INT(total, KAT_LINE_COUNT);
    CHECK(kat_port(KAT_PORT_COUNT) == NULL);

    /* Every line maps back to its port and pin. */
    for (int l = 0; l < KAT_LINE_COUNT; l++) CHECK_INT(kat_line(kat_line_port(l), kat_line_pin(l)), l);
    CHECK_INT(kat_line(KAT_PORT_A_USBC, KAT_C_COUNT), -1);
    CHECK_INT(kat_line(KAT_PORT_COUNT, 0), -1);
    CHECK_INT(kat_line_port(KAT_LINE_COUNT), KAT_PORT_COUNT);
    CHECK_INT(kat_line_pin(-1), -1);
    CHECK_INT(kat_port(KAT_PORT_B_MICROB)->end, KAT_END_B);
    CHECK_INT(kat_port(KAT_PORT_A_RJ45)->end, KAT_END_A);

    /* Mirroring a USB-C pin twice gets it back, and swaps the A and B rows. */
    for (int p = 0; p < KAT_C_COUNT; p++) CHECK_INT(kat_usbc_mirror(kat_usbc_mirror(p)), p);
    CHECK_INT(kat_usbc_mirror(KAT_C_CC1), KAT_C_CC2);
    CHECK_INT(kat_usbc_mirror(KAT_C_TX1P), KAT_C_TX2P);   /* A2 <-> B2 */
    CHECK_INT(kat_usbc_mirror(KAT_C_RX1P), KAT_C_RX2P);   /* B11 <-> A11 */
    CHECK_INT(kat_usbc_mirror(KAT_C_VBUS), KAT_C_VBUS);
    CHECK_INT(kat_usbc_mirror(99), -1);

    CHECK_STR(kat_pin_name(KAT_PTYPE_USBC, KAT_C_CC1), "CC1 (A5)");
    CHECK_STR(kat_pin_name(KAT_PTYPE_USBA, KAT_A_SSTXP), "SSTX+");
    CHECK_STR(kat_pin_name(KAT_PTYPE_MICROB, KAT_M_ID), "ID");
    CHECK_STR(kat_pin_name(KAT_PTYPE_RJ45, 0), "1");
    CHECK_STR(kat_pin_name(KAT_PTYPE_RJ45, 42), "?");
    CHECK_STR(kat_pin_name(KAT_PTYPE_USBC, -1), "?");
    CHECK_STR(kat_pin_name(KAT_PTYPE_USBA, 9), "?");
    CHECK_STR(kat_pin_name(KAT_PTYPE_MICROB, 6), "?");

    /* The scan: joins are symmetric and transitive, and equal scans compare equal. */
    kat_scan_t a, b;
    kat_scan_clear(&a);
    kat_scan_clear(&b);
    CHECK(!kat_scan_any_cable(&a));
    kat_scan_join(&a, 3, 50);
    kat_scan_join(&a, 50, 60);
    CHECK(kat_scan_joined(&a, 3, 60));
    CHECK(kat_scan_joined(&a, 60, 3));
    CHECK(!kat_scan_joined(&a, 3, 4));
    CHECK(!kat_scan_joined(&a, 3, -1));
    CHECK(kat_scan_any_cable(&a));
    kat_scan_join(&b, 60, 50);
    kat_scan_join(&b, 3, 60);
    CHECK(kat_scan_equal(&a, &b));
    kat_scan_join(&b, 1, 2);
    CHECK(!kat_scan_equal(&a, &b));
    kat_scan_join(&a, 3, 3);
    kat_scan_join(&a, 3, 999);
    CHECK_INT(kat_scan_root(&a, 999), -1);

    CHECK_DONE();
}

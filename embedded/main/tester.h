/* Plug and go (KATOO-6): notice a cable, test it, show its bag, wait for it to go. */
#pragma once

#include <stdbool.h>

/* hw_ok: the expanders answered at boot. Without them there's nothing to scan, so the task just
 * shows the fault. */
void tester_start(bool hw_ok);

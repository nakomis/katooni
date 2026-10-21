/* What the tester task and the web server share: the session and the last cable, behind a mutex. */
#pragma once

#include <stdbool.h>

#include "katooni/session.h"

typedef struct {
    kat_session_t session;
    kat_config_t config;
    kat_cable_t last;
    kat_add_result_t last_result;
    bool have_last;
    uint32_t last_seq;      /* bumps every time a cable is tested */
} app_state_t;

void app_state_init(void);
app_state_t *app_state_lock(void);
void app_state_unlock(void);

/* Persist the session to NVS (call with the lock held), and load it at boot. */
void app_state_save(void);
bool app_state_load(void);

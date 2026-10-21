#include "web.h"

#include <stdlib.h>
#include <string.h>

#include "app_state.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "katooni/json.h"

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");

static const char *TAG = "web";

/* JSON goes out in ~1 KB chunks, so a whole session never has to sit in RAM as text. */
typedef struct {
    httpd_req_t *req;
    char buf[1024];
    size_t len;
} chunker_t;

static void chunk_write(void *ctx, const char *s, size_t n)
{
    chunker_t *c = ctx;
    while (n) {
        size_t take = n < sizeof c->buf - c->len ? n : sizeof c->buf - c->len;
        memcpy(c->buf + c->len, s, take);
        c->len += take;
        s += take;
        n -= take;
        if (c->len == sizeof c->buf) {
            httpd_resp_send_chunk(c->req, c->buf, (ssize_t)c->len);
            c->len = 0;
        }
    }
}

static void chunk_end(chunker_t *c)
{
    if (c->len) httpd_resp_send_chunk(c->req, c->buf, (ssize_t)c->len);
    httpd_resp_send_chunk(c->req, NULL, 0);
}

/* A copy of the state, taken under the lock and streamed without it: a slow phone mustn't hold up
 * the tester. Several kB, so on the heap. */
typedef struct {
    kat_session_t session;
    kat_cable_t last;
    kat_add_result_t result;
    bool have;
    uint32_t seq;
    chunker_t out;
} snapshot_t;

static snapshot_t *snapshot(httpd_req_t *req)
{
    snapshot_t *s = malloc(sizeof *s);
    if (!s) return NULL;
    app_state_t *st = app_state_lock();
    s->session = st->session;
    s->last = st->last;
    s->result = st->last_result;
    s->have = st->have_last;
    s->seq = st->last_seq;
    app_state_unlock();
    s->out.req = req;
    s->out.len = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return s;
}

static esp_err_t get_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, index_start, index_end - index_start);
}

static esp_err_t get_session(httpd_req_t *req)
{
    snapshot_t *s = snapshot(req);
    if (!s) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    kat_json_session(&s->session, (kat_sink_t){chunk_write, &s->out});
    chunk_end(&s->out);
    free(s);
    return ESP_OK;
}

static esp_err_t get_last(httpd_req_t *req)
{
    snapshot_t *s = snapshot(req);
    if (!s) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    kat_json_last(&s->session, &s->last, &s->result, s->have, s->seq, (kat_sink_t){chunk_write, &s->out});
    chunk_end(&s->out);
    free(s);
    return ESP_OK;
}

static esp_err_t post_new(httpd_req_t *req)
{
    app_state_t *st = app_state_lock();
    kat_session_clear(&st->session);
    st->have_last = false;
    st->last_seq++;
    app_state_save();
    app_state_unlock();
    ESP_LOGI(TAG, "new session");
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t web_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    httpd_handle_t server;
    esp_err_t err = httpd_start(&server, &cfg);
    if (err != ESP_OK) return err;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_index},
        {.uri = "/api/session", .method = HTTP_GET, .handler = get_session},
        {.uri = "/api/last", .method = HTTP_GET, .handler = get_last},
        {.uri = "/api/session/new", .method = HTTP_POST, .handler = post_new},
    };
    for (size_t i = 0; i < sizeof routes / sizeof routes[0]; i++) httpd_register_uri_handler(server, &routes[i]);
    ESP_LOGI(TAG, "serving on port %d", cfg.server_port);
    return ESP_OK;
}

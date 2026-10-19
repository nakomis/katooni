/* Katooni on the Mac (KATOO-7): the portable core, the firmware's web page and the same JSON API,
 * with canned cables instead of hardware. For developing and checking the page.
 *
 *   build/sim/katooni-sim [--port 8080] [--demo] [--page embedded/main/web/index.html]
 *
 * The device's API, plus two for the simulator:
 *   GET  /api/sim/cables              the canned cables' names
 *   POST /api/sim/plug?cable=<name>   test one, as if it had been plugged in
 * --demo plugs a drawer's worth of cables at start. All cables and e-marker replies are synthetic. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "katooni/json.h"
#include "katooni/simcable.h"

typedef struct {
    const char *name;
    enum { USB, RJ } type;
    kat_sim_usb_t usb;
    bool em;
    kat_sim_em_t e;
    uint32_t loop_mohm;
    uint8_t rj[8];
    bool shield;
} canned_t;

static const canned_t CABLES[] = {
    {"cc-240w", USB, {.kind = KAT_KIND_C_C, .ra = true}, true,
     {.vid = 0x05AC, .pid = 0x7E57, .vbus_code = 3, .current_code = 2, .speed = KAT_SPEED_GEN3, .latency = 1, .epr = true}, 92, {0}, 0},
    {"cc-100w-usb3", USB, {.kind = KAT_KIND_C_C, .flip_b = 1, .ra = true}, true,
     {.vid = 0x2109, .pid = 0x0101, .vbus_code = 0, .current_code = 2, .speed = KAT_SPEED_GEN2, .latency = 2}, 128, {0}, 0},
    {"cc-usb2", USB, {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true}, false, {0}, 180, {0}, 0},
    {"cc-power-only", USB, {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true, .no_d = true, .no_shield = true}, false, {0}, 210, {0}, 0},
    {"cc-thin-liar", USB, {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true, .ra = true}, true,
     {.vid = 0x1A86, .pid = 0x0BAD, .vbus_code = 0, .current_code = 2, .speed = KAT_SPEED_USB2, .latency = 1}, 310, {0}, 0},
    {"cc-dplus-open", USB, {.kind = KAT_KIND_C_C, .no_ss = true, .no_sbu = true}, false, {0}, 150, {0}, 0},
    {"ac-usb3", USB, {.kind = KAT_KIND_A_C, .rp = true}, false, {0}, 0, {0}, 0},
    {"ac-usb2", USB, {.kind = KAT_KIND_A_C, .no_ss = true, .rp = true, .flip_b = 1}, false, {0}, 0, {0}, 0},
    {"c-microb", USB, {.kind = KAT_KIND_C_MICRO, .rd = true}, false, {0}, 0, {0}, 0},
    {"a-microb-otg", USB, {.kind = KAT_KIND_A_MICRO, .otg = true}, false, {0}, 0, {0}, 0},
    {"rj45-8wire-stp", RJ, {0}, false, {0}, 0, {1, 2, 3, 4, 5, 6, 7, 8}, true},
    {"rj45-4wire", RJ, {0}, false, {0}, 0, {1, 2, 3, 0, 0, 6, 0, 0}, false},
    {"rj45-crossover", RJ, {0}, false, {0}, 0, {3, 6, 1, 4, 5, 2, 7, 8}, false},
    {"rj45-reversed", RJ, {0}, false, {0}, 0, {2, 1, 3, 4, 5, 6, 7, 8}, false},
};
#define NCABLES (sizeof CABLES / sizeof CABLES[0])

static kat_session_t s_session;
static kat_config_t s_cfg;
static kat_cable_t s_last;
static kat_add_result_t s_result;
static bool s_have;
static uint32_t s_seq;
static char *s_page;
static size_t s_page_len;

static const canned_t *find(const char *name)
{
    for (size_t i = 0; i < NCABLES; i++)
        if (strcmp(CABLES[i].name, name) == 0) return &CABLES[i];
    return NULL;
}

/* Test a canned cable as the firmware's tester would. */
static void plug(const canned_t *c)
{
    kat_scan_t scan;
    memset(&s_last, 0, sizeof s_last);
    if (c->type == RJ) {
        kat_sim_rj45(&scan, c->rj, c->shield);
    } else {
        kat_sim_usb(&scan, &c->usb);
        if (strcmp(c->name, "cc-dplus-open") == 0) {   /* the one broken USB cable: D+ cut */
            kat_scan_t t;
            kat_scan_clear(&t);
            int dp = kat_line(KAT_PORT_A_USBC, KAT_C_DP1);
            for (int l = 0; l < KAT_LINE_COUNT; l++) {
                int root = kat_scan_root(&scan, l);
                if (l != dp && root != dp && root != l) kat_scan_join(&t, root, l);
            }
            scan = t;
        }
    }
    kat_classify(&scan, &s_last.cont);
    if (c->em) {
        uint32_t o[6];
        kat_emarker_decode(o, kat_sim_emarker(&c->e, o), &s_last.em);
    } else {
        kat_emarker_none(&s_last.em);
    }
    /* Only C-to-C cables are measured on the real board; a few mΩ of jitter, like a real reading. */
    if (s_last.cont.kind == KAT_KIND_C_C && c->loop_mohm) {
        int32_t loop = (int32_t)c->loop_mohm + (rand() % 9) - 4;
        kat_resistance_compute(loop * 500, 500000, s_last.em.max_current_ma, kat_latency_metres(s_last.em.latency),
                               &s_cfg.res, &s_last.res);
    }
    s_result = kat_session_add(&s_session, &s_last, &s_cfg);
    s_have = true;
    s_seq++;
}

/* ---- HTTP ---------------------------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len, cap;
} grow_t;

static void grow_write(void *ctx, const char *p, size_t n)
{
    grow_t *g = ctx;
    if (g->len + n > g->cap) {
        g->cap = (g->len + n) * 2;
        g->buf = realloc(g->buf, g->cap);
    }
    memcpy(g->buf + g->len, p, n);
    g->len += n;
}

static void send_all(int fd, const char *p, size_t n)
{
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) return;
        p += w;
        n -= (size_t)w;
    }
}

static void respond(int fd, const char *status, const char *type, const char *body, size_t len)
{
    char head[256];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
                     "Connection: close\r\n\r\n",
                     status, type, len);
    send_all(fd, head, (size_t)n);
    if (len) send_all(fd, body, len);
}

static void respond_json(int fd, grow_t *g)
{
    respond(fd, "200 OK", "application/json", g->buf, g->len);
    free(g->buf);
}

static void handle(int fd)
{
    char req[4096];
    size_t got = 0;
    while (got < sizeof req - 1) {   /* the request line and headers; the API takes no bodies */
        ssize_t r = read(fd, req + got, sizeof req - 1 - got);
        if (r <= 0) break;
        got += (size_t)r;
        req[got] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }
    req[got] = 0;
    char method[8] = "", path[512] = "";
    if (sscanf(req, "%7s %511s", method, path) != 2) return respond(fd, "400 Bad Request", "text/plain", "", 0);

    grow_t g = {0};
    kat_sink_t sink = {grow_write, &g};
    if (!strcmp(method, "GET") && (!strcmp(path, "/") || !strcmp(path, "/index.html"))) {
        respond(fd, "200 OK", "text/html; charset=utf-8", s_page, s_page_len);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/session")) {
        kat_json_session(&s_session, sink);
        respond_json(fd, &g);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/last")) {
        kat_json_last(&s_session, &s_last, &s_result, s_have, s_seq, sink);
        respond_json(fd, &g);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/session/new")) {
        kat_session_clear(&s_session);
        s_have = false;
        s_seq++;
        respond(fd, "204 No Content", "text/plain", "", 0);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/sim/cables")) {
        grow_write(&g, "[", 1);
        for (size_t i = 0; i < NCABLES; i++) {
            char item[64];
            int n = snprintf(item, sizeof item, "%s\"%s\"", i ? "," : "", CABLES[i].name);
            grow_write(&g, item, (size_t)n);
        }
        grow_write(&g, "]", 1);
        respond_json(fd, &g);
    } else if (!strcmp(method, "POST") && !strncmp(path, "/api/sim/plug?cable=", 20)) {
        const canned_t *c = find(path + 20);
        if (!c) return respond(fd, "404 Not Found", "text/plain", "no such cable", 13);
        plug(c);
        kat_json_last(&s_session, &s_last, &s_result, s_have, s_seq, sink);
        respond_json(fd, &g);
    } else {
        respond(fd, "404 Not Found", "text/plain", "not found", 9);
    }
}

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    *len = fread(buf, 1, (size_t)n, f);
    buf[*len] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    int port = 8080;
    bool demo = false;
    const char *page = KATOONI_PAGE;   /* embedded/main/web/index.html, from CMake */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--demo")) demo = true;
        else if (!strcmp(argv[i], "--page") && i + 1 < argc) page = argv[++i];
        else {
            fprintf(stderr, "usage: %s [--port N] [--demo] [--page index.html]\n", argv[0]);
            return 2;
        }
    }
    s_page = read_file(page, &s_page_len);
    if (!s_page) {
        fprintf(stderr, "can't read %s: %s\n", page, strerror(errno));
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    srand(1);
    kat_config_default(&s_cfg);
    kat_session_clear(&s_session);
    if (demo) {
        static const char *const DRAWER[] = {
            "cc-240w", "cc-240w", "cc-usb2", "cc-100w-usb3", "cc-240w", "rj45-8wire-stp", "cc-usb2",
            "cc-thin-liar", "ac-usb3", "rj45-reversed", "c-microb", "cc-power-only", "rj45-8wire-stp",
        };
        for (size_t i = 0; i < sizeof DRAWER / sizeof DRAWER[0]; i++) plug(find(DRAWER[i]));
    }

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port),
                               .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) || listen(srv, 8)) {
        fprintf(stderr, "can't listen on 127.0.0.1:%d: %s\n", port, strerror(errno));
        return 1;
    }
    socklen_t alen = sizeof addr;
    getsockname(srv, (struct sockaddr *)&addr, &alen);
    printf("katooni-sim on http://127.0.0.1:%d/\n", ntohs(addr.sin_port));
    fflush(stdout);
    for (;;) {
        int fd = accept(srv, NULL, NULL);
        if (fd < 0) continue;
        handle(fd);
        close(fd);
    }
}

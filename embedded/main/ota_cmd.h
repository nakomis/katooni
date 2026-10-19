// Fleet's command protocol, as Katooni reads it (KATOO-10). Copied from tinycrypt (CRYPT-14), less the
// watch approval: Katooni takes no approval, so a signed, newer image is enough.
//
// Commands arrive as JSON on katooni/<thing>/commands:
//   {"command":"get_version"} | {"command":"ping"} | {"command":"ota_cancel"}
//   {"command":"ota_update","url","version","sha256"}
//   {"command":"url_chunk","chunk_index","total_chunks","chunk_data","version"[,"sha256"]}
// Chunks (0-based, at most OTA_MAX_CHUNKS, in any order) carry a URL too long for one MQTT message;
// sha256 rides on one of them (Fleet puts it on the last). Everything else (reboot, wipe, settings) is
// refused.
//
// The parser checks shape only. What Katooni will install is decided by the image itself: its
// signature, its SHA-256 against this one, its project and its version.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OTA_MAX_CHUNKS 10
#define OTA_URL_MAX 8192  // a presigned URL with an STS session token is ~1.5-3 KB
#define OTA_VERSION_MAX 16  // "x.y.z", each part at most 4 digits
#define OTA_SHA256_LEN 32
#define OTA_CHUNKS_MAX_AGE_MS 60000  // a set that hasn't completed by then is dropped (CRYPT-21)

typedef enum
{
    OTA_CMD_GET_VERSION,
    OTA_CMD_PING,
    OTA_CMD_CANCEL,
    OTA_CMD_UPDATE,  // url, version and sha256 are set
    OTA_CMD_CHUNK,   // a chunk was stored: chunk_index of total_chunks (the URL isn't whole yet)
    OTA_CMD_ERROR,   // message is set; ota_error says whether to reply ota_error or error
} ota_cmd_kind;

typedef struct
{
    ota_cmd_kind kind;
    char url[OTA_URL_MAX];
    char version[OTA_VERSION_MAX];
    uint8_t sha256[OTA_SHA256_LEN];
    int chunk_index, total_chunks;
    bool ota_error;
    char message[96];
} ota_cmd;

// Chunks collected so far. Zero-initialise, or call ota_cmd_reset().
typedef struct
{
    int total;
    char version[OTA_VERSION_MAX];
    bool have_sha256;
    uint8_t sha256[OTA_SHA256_LEN];
    char *chunk[OTA_MAX_CHUNKS];  // malloc'd
    size_t stored;  // the chunks' total length: never reaches OTA_URL_MAX (CRYPT-21)
    uint32_t started_ms;  // when the set's first chunk came
} ota_chunks;

void ota_cmd_reset(ota_chunks *chunks);

// Parses one command. `out` is large (the URL), so keep it static or on the heap. `now_ms` is any
// millisecond clock (it may wrap): a chunk set older than OTA_CHUNKS_MAX_AGE_MS is dropped by the
// next command of any kind.
void ota_cmd_parse(ota_chunks *chunks, const char *json, size_t len, uint32_t now_ms, ota_cmd *out);

// "x.y.z": three dot-separated runs of 1-4 decimal digits, nothing else.
bool ota_version_valid(const char *v);
// Compares two valid versions numerically: < 0, 0 or > 0.
int ota_version_cmp(const char *a, const char *b);


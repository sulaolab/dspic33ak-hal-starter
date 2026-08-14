#include "app_console_line.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "app_console.h"
#include "touch_console.h"

static bool hex_nibble(char c, uint8_t *out)
{
    if ((c >= '0') && (c <= '9')) { *out = (uint8_t)(c - '0');        return true; }
    if ((c >= 'a') && (c <= 'f')) { *out = (uint8_t)(10 + (c - 'a')); return true; }
    if ((c >= 'A') && (c <= 'F')) { *out = (uint8_t)(10 + (c - 'A')); return true; }
    return false;
}

bool app_console_line_dispatch(const char *line)
{
    app_console_msg_t msg;
    size_t            len;
    size_t            hex;
    size_t            i;

    if (line == NULL) {
        return false;
    }

    len = strlen(line);
    if (len < 3u) {
        return false;
    }
    if ((line[0] != '*') && (line[0] != '?')) {
        return false;
    }

    /* Only module 'k' exists here. Checked before the payload is decoded so a
     * malformed line for a module this build does not carry is reported as an
     * unknown command rather than as a bad payload. */
    if (line[1] != 'k') {
        return false;
    }

    hex = len - 3u;
    if ((hex & 1u) != 0u) {
        printf(" console: odd number of hex digits in \"%s\"\n", line);
        return true;
    }
    if ((hex / 2u) > (size_t)APP_CONSOLE_MAX_DATA_BYTES) {
        printf(" console: payload too long in \"%s\"\n", line);
        return true;
    }

    memset(&msg, 0, sizeof(msg));
    msg.kind    = (uint8_t)line[0];
    msg.module  = (uint8_t)line[1];
    msg.name    = (uint8_t)line[2];
    msg.raw_len = (uint16_t)len;

    for (i = 0u; i < (hex / 2u); i++) {
        uint8_t hi;
        uint8_t lo;

        if (!hex_nibble(line[3u + (2u * i)], &hi) ||
            !hex_nibble(line[4u + (2u * i)], &lo)) {
            printf(" console: bad hex digit in \"%s\"\n", line);
            return true;
        }
        msg.data[i] = (uint8_t)((hi << 4) | lo);
    }
    msg.data_len = (uint16_t)(hex / 2u);

    touch_console_onmsg(&msg);

    /* The module reports its own outcome in msg.status; it has already printed
     * whatever the human needs to see, so the status is not echoed a second time
     * -- sonora's dispatcher does the same, and the manual's transcripts show
     * exactly one line per command because of it. */
    return true;
}

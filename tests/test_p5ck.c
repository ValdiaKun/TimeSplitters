#include "p5ck.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const uint8_t gz[] = {
        0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0x0b,0xc9,0xcc,0x4d,0x0d,0x2e,
        0x8c,0x92,0xc2,0x94,0x92,0xd2,0xa5,0x60,0x83,0x07,0x5f,0x65,0x64,0x8a,0xfc,0xa2,
        0xc5,0x02,0x84,0xa4,0xd2,0xf4,0xa2,0xd2,0xec,0xec,0xcf,0x03,0x00,0x6b,0x29,0xce,
        0xe9,0x22,0x00,0x00,0x00
    };
    const char expected[] = "TimeSplitters P5CK gzip regression";
    FILE *fp = tmpfile();
    TsP5ckEntry e;
    uint8_t *out = NULL;
    size_t size = 0;
    assert(fp);
    memset(&e, 0, sizeof(e));
    e.offset = 0;
    e.length = (uint32_t)(sizeof(expected) - 1);
    e.compressed_length = (uint32_t)sizeof(gz);
    assert(fwrite(gz, 1, sizeof(gz), fp) == sizeof(gz));
    fflush(fp);
    assert(ts_p5ck_read_payload(fp, &e, &out, &size) == 0);
    assert(size == sizeof(expected) - 1);
    assert(memcmp(out, expected, size) == 0);
    free(out);
    fclose(fp);
    return 0;
}

#include "p5ck.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const uint8_t gz[] = {
        0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0x0b,0xc9,0xcc,0x4d,0x0d,0x2e,
        0xc8,0xc9,0x2c,0x29,0x49,0x2d,0x2a,0x56,0x08,0x30,0x75,0xf6,0x56,0x48,0xaf,0xca,
        0x2c,0x50,0x28,0x4a,0x4d,0x2f,0x4a,0x2d,0x2e,0xce,0xcc,0xcf,0x03,0x00,0x6b,0x29,
        0xce,0xe9,0x22,0x00,0x00,0x00
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
    e.length++;
    out=NULL; size=0;
    assert(ts_p5ck_read_payload(fp, &e, &out, &size) != 0);
    assert(out==NULL && size==0);
    {
        /* Reject a directory table that overlaps the P5CK header itself. */
        uint8_t bad_header[24]={'P','5','C','K',8,0,0,0,16,0,0,0};
        FILE *bad=tmpfile();
        TsP5ckInfo info;
        assert(bad);
        assert(fwrite(bad_header,1,sizeof(bad_header),bad)==sizeof(bad_header));
        fflush(bad);
        assert(ts_p5ck_read_info(bad,&info)==-4);
        fclose(bad);
    }
    {
        /* A validated one-entry table still resolves its directory record. */
        uint8_t valid_header[32]={
            'P','5','C','K', 12,0,0,0, 16,0,0,0,
            0x78,0x56,0x34,0x12, 28,0,0,0, 4,0,0,0, 0,0,0,0,
            0xde,0xad,0xbe,0xef
        };
        FILE *table=tmpfile();
        TsP5ckInfo info;
        TsP5ckEntry entry;
        assert(table);
        assert(fwrite(valid_header,1,sizeof(valid_header),table)==sizeof(valid_header));
        fflush(table);
        assert(ts_p5ck_read_info(table,&info)==0 && info.entry_count==1u);
        assert(ts_p5ck_read_entry(table,&info,0,&entry)==0);
        assert(entry.crc==0x12345678u && entry.offset==28u && entry.length==4u &&
               entry.compressed_length==0u);
        {
            const uint8_t wrapped_offset[4]={0xfc,0xff,0xff,0xff};
            assert(fseek(table,16,SEEK_SET)==0);
            assert(fwrite(wrapped_offset,1,sizeof(wrapped_offset),table)==sizeof(wrapped_offset));
            fflush(table);
            assert(ts_p5ck_read_entry(table,&info,0,&entry)==-2);
            assert(entry.offset==0xfffffffcu && entry.length==4u);
        }
        fclose(table);
    }
    {
        /* Revalidate caller-supplied table metadata before computing file offsets. */
        FILE *table=tmpfile();
        TsP5ckInfo info={12u,0xfffffff0u,0x0fffffffu};
        TsP5ckEntry entry;
        assert(table);
        assert(ts_p5ck_read_entry(table,&info,0x08000000u,&entry)==-1);
        assert(entry.crc==0u && entry.offset==0u && entry.length==0u &&
               entry.compressed_length==0u);
        fclose(table);
    }
    fclose(fp);
    return 0;
}

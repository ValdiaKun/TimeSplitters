#ifndef TS_P5CK_H
#define TS_P5CK_H
#include <stdint.h>
#include <stdio.h>
typedef struct { uint32_t crc, offset, length, compressed_length; } TsP5ckEntry;
typedef struct { uint32_t index_offset, index_length, entry_count; } TsP5ckInfo;
int ts_p5ck_read_info(FILE *fp, TsP5ckInfo *info);
int ts_p5ck_read_entry(FILE *fp, const TsP5ckInfo *info, uint32_t index, TsP5ckEntry *entry);
int ts_p5ck_read_payload(FILE *fp, const TsP5ckEntry *entry, uint8_t **data, size_t *size);
#endif

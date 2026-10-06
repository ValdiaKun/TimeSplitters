#include "p5ck.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <zlib.h>

static int u32(FILE *fp, uint32_t *v) {
    unsigned char b[4];
    if (fread(b,1,4,fp)!=4) return -1;
    *v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
    return 0;
}

int ts_p5ck_read_info(FILE *fp, TsP5ckInfo *info) {
    unsigned char magic[4]; long end;
    if (!fp || !info) return -1; rewind(fp);
    if (fread(magic,1,4,fp)!=4) return -1;
    if (memcmp(magic,"P5CK",4)!=0) return -2;
    if (u32(fp,&info->index_offset)<0 || u32(fp,&info->index_length)<0) return -1;
    if (info->index_length & 15u) return -3;
    if (fseek(fp,0,SEEK_END)!=0) return -1; end=ftell(fp);
    if (end < 0 || (uint64_t)info->index_offset + info->index_length > (uint64_t)end) return -4;
    info->entry_count=info->index_length/16u; return 0;
}

int ts_p5ck_read_entry(FILE *fp,const TsP5ckInfo *info,uint32_t index,TsP5ckEntry *e) {
    long off,file_end; uint64_t payload_end; uint32_t stored;
    if (!fp || !info || !e || index>=info->entry_count) return -1;
    off=(long)info->index_offset+(long)index*16L;
    if (fseek(fp,off,SEEK_SET)!=0)return -1;
    if (u32(fp,&e->crc)<0 || u32(fp,&e->offset)<0 || u32(fp,&e->length)<0 || u32(fp,&e->compressed_length)<0)return -1;
    stored=e->compressed_length?e->compressed_length:e->length;
    if (fseek(fp,0,SEEK_END)!=0)return -1; file_end=ftell(fp);
    if (file_end<0)return -1; payload_end=(uint64_t)e->offset+stored;
    return payload_end<=(uint64_t)file_end?0:-2;
}

int ts_p5ck_read_payload(FILE *fp,const TsP5ckEntry *e,uint8_t **data,size_t *size) {
    uint8_t *src=NULL,*dst=NULL;
    size_t stored,out_size;
    z_stream zs;
    uLongf zlen;
    if(!fp||!e||!data||!size)return -1;
    *data=NULL;*size=0;
    stored=e->compressed_length?e->compressed_length:e->length;
    if(!stored || e->offset>(uint32_t)LONG_MAX)return -2;
    if(fseek(fp,(long)e->offset,SEEK_SET)!=0)return -3;
    src=(uint8_t*)malloc(stored); if(!src)return -4;
    if(fread(src,1,stored,fp)!=stored){free(src);return -5;}
    if(e->compressed_length && stored>=2 && src[0]==0x1fu && src[1]==0x8bu){
        out_size=e->length;
        if(!out_size || out_size>64u*1024u*1024u || out_size>UINT_MAX){free(src);return -6;}
        dst=(uint8_t*)malloc(out_size); if(!dst){free(src);return -7;}
        memset(&zs,0,sizeof(zs));
        zs.next_in=src; zs.avail_in=(uInt)stored;
        zs.next_out=dst; zs.avail_out=(uInt)out_size;
        if(inflateInit2(&zs,16+MAX_WBITS)!=Z_OK){free(src);free(dst);return -8;}
        if(inflate(&zs,Z_FINISH)!=Z_STREAM_END){
            inflateEnd(&zs);free(src);free(dst);return -9;
        }
        zlen=zs.total_out;
        inflateEnd(&zs);
        if(zlen!=(uLongf)e->length){free(src);free(dst);return -10;}
        free(src);
        *data=dst;*size=(size_t)zlen;return 0;
    }
    *data=src;*size=stored;return 0;
}

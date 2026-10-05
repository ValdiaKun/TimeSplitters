#ifndef FP_VU_H
#define FP_VU_H
#include <stddef.h>
#include <stdint.h>
int tsfp_vu_find_vutext(const uint8_t *elf, size_t size, size_t *offset, size_t *length);
int tsfp_vu_probe(const uint8_t *micro, size_t size, uint32_t start, uint32_t *xgkick_pc);
#endif

#ifndef FP_VU_H
#define FP_VU_H
#include <stddef.h>
#include <stdint.h>
int tsfp_vu_find_vutext(const uint8_t *elf, size_t size, size_t *offset, size_t *length);
int tsfp_vu_probe(const uint8_t *micro, size_t size, uint32_t start, uint32_t *xgkick_pc);
typedef struct {
    uint32_t vf[32][4];
    uint32_t vi[32];
    uint32_t acc[4];
    uint32_t q;
    uint32_t top;
    uint32_t itop;
    uint32_t mac_flag;
    uint32_t status_flag;
    uint32_t clip_flag;
    uint8_t *memory;
    size_t memory_size;
    uint8_t *gif;
    size_t gif_size;
    size_t gif_used;
    uint32_t pc;
    uint32_t steps;
    uint32_t unsupported;
    uint32_t branch_pending;
    uint32_t branch_target;
    uint32_t end_pending;
    uint16_t mac_pipe[4];
    uint16_t status_pipe[4];
    uint32_t clip_pipe[4];
    uint8_t flag_pipe_valid[4];
    uint32_t flag_pipe_pos;
    uint32_t xgkick_pc;
} TsFpVuState;

void tsfp_vu_state_init(TsFpVuState *state, uint8_t *memory, size_t memory_size,
                        uint8_t *gif, size_t gif_size);
int tsfp_vu_execute(const uint8_t *micro, size_t size, uint32_t start,
                    TsFpVuState *state, uint32_t max_steps);

#endif

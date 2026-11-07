#ifndef AV_VRAM_GATE_H
#define AV_VRAM_GATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AvVramGate {
    uint32_t size;
    void    *self;
    void    *(*open)(void *self, const uint8_t *gpu_uuid);
    int      (*take)(void *account, uint64_t bytes);
    void     (*give)(void *account, uint64_t bytes);
    void     (*close)(void *self, void *account);
    uint64_t (*free_bytes)(void *account);
    uint64_t (*cap)(void *account);
    int      (*take_host)(void *account, uint64_t bytes);
    void     (*give_host)(void *account, uint64_t bytes);
};

#ifdef __cplusplus
}
#endif

#endif

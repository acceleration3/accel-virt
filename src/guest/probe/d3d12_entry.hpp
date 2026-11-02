#ifndef AV_D3D12_ENTRY_HPP
#define AV_D3D12_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d12Entry {
public:
    D3d12Entry(const D3d12Entry &) = delete;
    D3d12Entry &operator=(const D3d12Entry &) = delete;

    virtual int32_t create_device(uint32_t &feature_level) = 0;
    virtual int32_t adapter_luid(uint64_t &luid) = 0;
    virtual int32_t pipeline_library(uint32_t &serialized) = 0;
    virtual int32_t create_queue() = 0;
    virtual int32_t create_render_target(uint32_t width, uint32_t height) = 0;
    virtual int32_t clear_and_copy(const uint8_t *colour) = 0;
    virtual int32_t wait() = 0;
    virtual int32_t read_back(uint8_t *pixels, uint32_t bytes) = 0;
    virtual int32_t existing_heap(const uint8_t *pattern, uint8_t *copied, uint32_t bytes, uint32_t &aliased) = 0;
    virtual int32_t write_watch(uint32_t &pages) = 0;
    virtual void    release() = 0;

protected:
    D3d12Entry() = default;
    ~D3d12Entry() = default;
};

}

#endif

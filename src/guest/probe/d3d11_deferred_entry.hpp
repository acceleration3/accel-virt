#ifndef AV_D3D11_DEFERRED_ENTRY_HPP
#define AV_D3D11_DEFERRED_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11DeferredEntry {
public:
    D3d11DeferredEntry(const D3d11DeferredEntry &) = delete;
    D3d11DeferredEntry &operator=(const D3d11DeferredEntry &) = delete;

    virtual int32_t  create_device(void *window, uint32_t width, uint32_t height) = 0;
    virtual int32_t  create_buffers(const uint32_t *sizes, uint32_t count, uint32_t geometry_bytes,
                                    uint32_t readback_bytes) = 0;
    virtual int32_t  create_contexts(uint32_t count) = 0;
    virtual int32_t  write_constants(uint32_t context, uint32_t buffer, uint8_t value) = 0;
    virtual int32_t  discard_geometry(uint32_t context, uint32_t bytes, uint8_t value) = 0;
    virtual int32_t  append_geometry(uint32_t context, uint32_t offset, uint32_t bytes, uint8_t value) = 0;
    virtual int32_t  finish(uint32_t context) = 0;
    virtual int32_t  execute(uint32_t context) = 0;
    virtual int32_t  present() = 0;
    virtual int32_t  read_geometry(uint8_t *bytes, uint32_t count) = 0;
    virtual uint64_t committed_bytes() = 0;
    virtual void     release() = 0;

protected:
    D3d11DeferredEntry() = default;
    ~D3d11DeferredEntry() = default;
};

}

#endif

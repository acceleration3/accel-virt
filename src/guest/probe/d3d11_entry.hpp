#ifndef AV_D3D11_ENTRY_HPP
#define AV_D3D11_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11Entry {
public:
    D3d11Entry(const D3d11Entry &) = delete;
    D3d11Entry &operator=(const D3d11Entry &) = delete;

    virtual int32_t create_device(void *window, uint32_t width, uint32_t height,
                                  uint32_t &feature_level) = 0;
    virtual int32_t adapter_luid(uint64_t &luid) = 0;
    virtual int32_t create_render_target() = 0;
    virtual void    clear(const uint8_t *colour) = 0;
    virtual int32_t read_back(uint8_t *pixels, uint32_t bytes) = 0;
    virtual int32_t present() = 0;
    virtual void    release() = 0;

protected:
    D3d11Entry() = default;
    ~D3d11Entry() = default;
};

}

#endif

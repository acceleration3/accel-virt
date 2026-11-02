#ifndef AV_D3D11_SHARED_ENTRY_HPP
#define AV_D3D11_SHARED_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11SharedEntry {
public:
    D3d11SharedEntry(const D3d11SharedEntry &) = delete;
    D3d11SharedEntry &operator=(const D3d11SharedEntry &) = delete;

    virtual int32_t create_device() = 0;
    virtual int32_t create_target(uint32_t target, uint32_t width, uint32_t height) = 0;
    virtual void    clear(uint32_t target, const uint8_t *colour) = 0;
    virtual int32_t read_texel(uint32_t target, uint32_t x, uint32_t y, uint8_t *texel) = 0;
    virtual int32_t removed_reason() = 0;
    virtual bool    layered() = 0;
    virtual void    release() = 0;

protected:
    D3d11SharedEntry() = default;
    ~D3d11SharedEntry() = default;
};

}

#endif

#ifndef AV_D3D9EX_ENTRY_HPP
#define AV_D3D9EX_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d9ExEntry {
public:
    D3d9ExEntry(const D3d9ExEntry &) = delete;
    D3d9ExEntry &operator=(const D3d9ExEntry &) = delete;

    virtual int32_t create_device(void *window) = 0;
    virtual int32_t create_swap_chain(void *window, uint32_t width, uint32_t height) = 0;
    virtual int32_t draw(const uint8_t *colour) = 0;
    virtual int32_t present(uint32_t width, uint32_t height) = 0;
    virtual int32_t wait_for_blank() = 0;
    virtual void    release() = 0;

protected:
    D3d9ExEntry() = default;
    ~D3d9ExEntry() = default;
};

}

#endif

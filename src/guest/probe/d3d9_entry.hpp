#ifndef AV_D3D9_ENTRY_HPP
#define AV_D3D9_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d9Entry {
public:
    D3d9Entry(const D3d9Entry &) = delete;
    D3d9Entry &operator=(const D3d9Entry &) = delete;

    virtual int32_t create_device(void *window, uint32_t width, uint32_t height) = 0;
    virtual int32_t adapter_ids(uint32_t &vendor, uint32_t &device) = 0;
    virtual int32_t clear(const uint8_t *colour) = 0;
    virtual int32_t read_back(uint8_t *pixels, uint32_t bytes) = 0;
    virtual int32_t present() = 0;
    virtual void    release() = 0;

protected:
    D3d9Entry() = default;
    ~D3d9Entry() = default;
};

}

#endif

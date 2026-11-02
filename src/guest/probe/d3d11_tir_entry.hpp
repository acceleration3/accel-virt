#ifndef AV_D3D11_TIR_ENTRY_HPP
#define AV_D3D11_TIR_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11TirEntry {
public:
    D3d11TirEntry(const D3d11TirEntry &) = delete;
    D3d11TirEntry &operator=(const D3d11TirEntry &) = delete;

    virtual int32_t create_device() = 0;
    virtual int32_t create_pipeline(uint32_t extent, uint32_t samples) = 0;
    virtual int32_t draw(const float *positions, uint32_t count) = 0;
    virtual int32_t read_back(uint32_t *texels, uint32_t count) = 0;
    virtual int32_t removed_reason() = 0;
    virtual bool    layered() = 0;
    virtual void    release() = 0;

protected:
    D3d11TirEntry() = default;
    ~D3d11TirEntry() = default;
};

}

#endif

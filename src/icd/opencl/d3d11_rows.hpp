#ifndef AV_CLICD_D3D11_ROWS_HPP
#define AV_CLICD_D3D11_ROWS_HPP

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class D3d11Rows {
public:
    D3d11Rows(const D3d11Rows &) = delete;
    D3d11Rows &operator=(const D3d11Rows &) = delete;

    virtual cl_int visit(uint8_t *data, size_t row_pitch, size_t slice_pitch) = 0;

protected:
    D3d11Rows() = default;
    ~D3d11Rows() = default;
};

}

#endif

/* What is done with a Direct3D 11 subresource's rows while D3d11Bridge has
 * them mapped (docs/d3d11-sharing.md): written into the cl_mem over it as an
 * acquire copies them in, or filled from it as a release copies them back.
 *
 * Two implementations, one per direction (ClD3d11Upload, ClD3d11Download),
 * and the tests' bridge calls whichever it is handed. */
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

    /* The subresource at `data`, a row every `row_pitch` bytes and a slice
     * every `slice_pitch`, as Map lays it out. What it answers is what the
     * bridge's read or write answers. */
    virtual cl_int visit(uint8_t *data, size_t row_pitch, size_t slice_pitch) = 0;

protected:
    D3d11Rows() = default;
    ~D3d11Rows() = default;
};

}  // namespace av::clicd

#endif  // AV_CLICD_D3D11_ROWS_HPP

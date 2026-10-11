/* An acquire's copy in (docs/d3d11-sharing.md): a Direct3D 11 subresource's
 * mapped rows written into the cl_mem over it, through the ICD's own blocking
 * transfers, so the bytes are the host's before the acquire returns. */
#ifndef AV_CLICD_CL_D3D11_UPLOAD_HPP
#define AV_CLICD_CL_D3D11_UPLOAD_HPP

#include "icd/opencl/d3d11_rows.hpp"

#include <CL/cl.h>

namespace av::clicd {

class ClIcdRoot;
class D3d11Object;

class ClD3d11Upload final : public D3d11Rows {
public:
    ClD3d11Upload(ClIcdRoot &root, cl_command_queue queue, cl_mem mem, const D3d11Object &object);

    cl_int visit(uint8_t *data, size_t row_pitch, size_t slice_pitch) override;

private:
    ClIcdRoot         &root_;
    cl_command_queue   queue_;
    cl_mem             mem_;
    const D3d11Object &object_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_D3D11_UPLOAD_HPP

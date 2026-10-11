/* A release's copy back (docs/d3d11-sharing.md): the cl_mem over a Direct3D 11
 * subresource read into a staging resource's mapped rows, through the ICD's
 * own blocking transfers, for the bridge to copy into the resource. */
#ifndef AV_CLICD_CL_D3D11_DOWNLOAD_HPP
#define AV_CLICD_CL_D3D11_DOWNLOAD_HPP

#include "icd/opencl/d3d11_rows.hpp"

#include <CL/cl.h>

namespace av::clicd {

class ClIcdRoot;
class D3d11Object;

class ClD3d11Download final : public D3d11Rows {
public:
    ClD3d11Download(ClIcdRoot &root, cl_command_queue queue, cl_mem mem, const D3d11Object &object);

    cl_int visit(uint8_t *data, size_t row_pitch, size_t slice_pitch) override;

private:
    ClIcdRoot         &root_;
    cl_command_queue   queue_;
    cl_mem             mem_;
    const D3d11Object &object_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_D3D11_DOWNLOAD_HPP

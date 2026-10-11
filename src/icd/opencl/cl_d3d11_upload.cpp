#include "icd/opencl/cl_d3d11_upload.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/d3d11_object.hpp"

namespace av::clicd {

ClD3d11Upload::ClD3d11Upload(ClIcdRoot &root, cl_command_queue queue, cl_mem mem, const D3d11Object &object)
    : root_(root), queue_(queue), mem_(mem), object_(object)
{
    AV_SCOPE("ClD3d11Upload::ClD3d11Upload");
}

cl_int ClD3d11Upload::visit(uint8_t *data, size_t row_pitch, size_t slice_pitch)
{
    AV_SCOPE("ClD3d11Upload::visit");
    if (object_.buffer()) return root_.enqueue_write(queue_, mem_, CL_TRUE, 0u, object_.bytes(), data, nullptr);
    const size_t origin[3] = {0u, 0u, 0u};
    size_t       region[3] = {};
    object_.region(region);
    /* A 2D image's slice pitch must be 0; only a 3D one has slices. */
    return root_.enqueue_write_image(queue_, mem_, origin, region, row_pitch, region[2] > 1u ? slice_pitch : 0u,
                                     data, nullptr);
}

}  // namespace av::clicd

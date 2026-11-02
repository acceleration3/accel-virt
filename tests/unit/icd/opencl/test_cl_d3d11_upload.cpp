#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_d3d11_upload.hpp"
#include "icd/opencl/d3d11_object.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <cstdint>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClD3d11Upload;
using av::clicd::D3d11Object;
using av::clicd::D3d11Resource;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_mem = 0x6600u;

class Opened {
public:
    explicit Opened(ClIcdRig &rig) : rig_(rig)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().blob_import(0u, 0x77u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS) return;
        rig_.host().blob_import(0u, 0x78u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        context_ = rig_.loader().create_context(device_, nullptr);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_queue, CL_SUCCESS);
        queue_ = rig_.loader().create_queue(context_, device_, nullptr);
    }

    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }

    cl_mem image(cl_mem_object_type type, std::size_t depth)
    {
        const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
        cl_image_desc         desc = {};
        desc.image_type = type;
        desc.image_width = 4u;
        desc.image_height = 2u;
        desc.image_depth = depth;
        rig_.host().cl_handle(host_mem, CL_SUCCESS);
        return rig_.root().create_image(context_, CL_MEM_READ_WRITE, &format, &desc, nullptr, nullptr);
    }

private:
    ClIcdRig        &rig_;
    cl_device_id     device_ = nullptr;
    cl_context       context_ = nullptr;
    cl_command_queue queue_ = nullptr;
};

int resource = 0;

}

TEST(cl_d3d11_upload, a_buffer_s_bytes_cross_as_one_write)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.queue() != nullptr);
  if (opened.queue() == nullptr) return;
  rig.host().cl_handle(host_mem, CL_SUCCESS);
  const cl_mem buffer = rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 16u, nullptr);
  const D3d11Object object(buffer, &resource, 0u,
                           D3d11Resource(D3d11Resource::Kind::buffer, 0u, 16u, 0u, 0u, 0u, 1u, 1u, 1u),
                           CL_MEM_READ_WRITE);
  std::uint8_t        bytes[16] = {1u, 2u, 3u};
  ClD3d11Upload       upload(rig.root(), opened.queue(), buffer, object);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(upload.visit(bytes, 16u, 16u), CL_SUCCESS);
  rig.host().cl_bytes(CL_SUCCESS, bytes, 16u);
  CHECK_EQ(rig.root().enqueue_read(opened.queue(), buffer, CL_TRUE, 0u, 16u, bytes, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueWriteBuffer);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueReadBuffer); }

TEST(cl_d3d11_upload, an_image_s_rows_cross_with_the_mapping_s_pitches)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.queue() != nullptr);
  if (opened.queue() == nullptr) return;
  const std::size_t element = 4u;
  std::uint8_t      rows[256] = {};
  const cl_mem      flat = opened.image(CL_MEM_OBJECT_IMAGE2D, 0u);
  const D3d11Object flat_object(flat, &resource, 0u,
                                D3d11Resource(D3d11Resource::Kind::texture2d, 28u, 0u, 4u, 2u, 1u, 1u, 1u, 1u),
                                CL_MEM_READ_WRITE);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&element), sizeof element);
  rig.host().cl_result(CL_SUCCESS);
  ClD3d11Upload flat_upload(rig.root(), opened.queue(), flat, flat_object);
  CHECK_EQ(flat_upload.visit(rows, 32u, 64u), CL_SUCCESS);
  const cl_mem      volume = opened.image(CL_MEM_OBJECT_IMAGE3D, 2u);
  const D3d11Object volume_object(volume, &resource, 0u,
                                  D3d11Resource(D3d11Resource::Kind::texture3d, 28u, 0u, 4u, 2u, 2u, 1u, 1u, 1u),
                                  CL_MEM_READ_WRITE);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&element), sizeof element);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  ClD3d11Upload       volume_upload(rig.root(), opened.queue(), volume, volume_object);
  CHECK_EQ(volume_upload.visit(rows, 16u, 64u), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueWriteImage);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&element), sizeof element);
  CHECK_EQ(volume_upload.visit(rows, 4u, 64u), CL_INVALID_VALUE); }

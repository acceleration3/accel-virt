/* cl_khr_gl_sharing in the guest ICD: which OpenGL context a context is made
 * from, what reaches the miniport for it, and which entries refuse a context
 * that was made from none.
 *
 * What is asserted is what an application sees - the context or the error it
 * is handed, the device clGetGLContextInfoKHR names - and what the ICD decides
 * the miniport is told: the OpenGL ICD's share name for the context, in the
 * record of the virtual context the ICD makes, and nothing at all for a GL
 * context the OpenGL ICD cannot vouch for. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/context/av_context_record.hpp"
#include "proto/context/sub_api.hpp"

#include <CL/cl_gl.h>

#include <cstdint>

using av::ClIcdRig;
using av::FakeClLoader;

namespace {

constexpr std::uint64_t host_platform = 0x7200u;
constexpr std::uint64_t host_device = 0x7300u;
constexpr std::uint64_t host_context = 0x7400u;
constexpr std::uint64_t host_buffer = 0x7500u;
/* What the OpenGL ICD would answer for its context: a D3DKMT handle, so
 * nothing a pointer or a host number could be mistaken for. */
constexpr std::uint32_t gl_share_name = 0x40000C42u;

/* The one device, found as the loader finds it. */
class Found {
public:
    explicit Found(ClIcdRig &rig) : rig_(rig)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().blob_import(0u, 0x77u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS) device_ = nullptr;
    }

    cl_device_id device() const { return device_; }

    /* A context made through the table with `properties`, the host answering
     * it as `host_context`. */
    cl_context create(const cl_context_properties *properties, cl_int &error)
    {
        rig_.host().blob_import(0u, 0x78u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        return rig_.loader().dispatch_of(device_)->clCreateContext(properties, 1u, &device_, nullptr, nullptr,
                                                                   &error);
    }

private:
    ClIcdRig    &rig_;
    cl_device_id device_ = nullptr;
};

void *const current_gl = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x10030u));
void *const other_gl = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x10040u));
void *const window_dc = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x2B010u));

}  // namespace

TEST(cl_gl_sharing, a_context_made_from_the_current_gl_context_names_its_objects)
{ ClIcdRig rig;
  Found     found(rig);
  CHECK(found.device() != nullptr);
  rig.gl_share().make_current(current_gl, gl_share_name);
  const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                              CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  cl_int error = CL_INVALID_VALUE;
  const cl_context context = found.create(properties, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(context != nullptr);
  /* The miniport is asked for an OpenCL context sharing the GL context's
   * objects, by the name the OpenGL ICD gave. */
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), av::Status::ok);
  CHECK(record.sub_api() == av::SubApi::opencl);
  CHECK_EQ(record.options().share_context(), gl_share_name);
  CHECK(record.options().shares_only());
  /* And the GL name a buffer is made over reaches the host as it was given. */
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_int     made = CL_INVALID_VALUE;
  const cl_mem buffer = rig.loader().dispatch_of(context)->clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, 7u, &made);
  CHECK_EQ(made, CL_SUCCESS);
  CHECK(buffer != nullptr); }

TEST(cl_gl_sharing, a_gl_context_that_is_not_current_is_no_share_group_and_makes_nothing)
{ ClIcdRig rig;
  Found     found(rig);
  rig.gl_share().make_current(current_gl, gl_share_name);
  const std::uint32_t   made_before = rig.kmt().last_private_size();
  const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(other_gl),
                                              CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  cl_int             error = CL_SUCCESS;
  const cl_device_id device = found.device();
  CHECK(rig.loader().dispatch_of(device)->clCreateContext(properties, 1u, &device, nullptr, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR);
  CHECK_EQ(rig.gl_share().asked(), 1u);
  /* Refused before any virtual context was asked for. */
  CHECK_EQ(rig.kmt().last_private_size(), made_before); }

TEST(cl_gl_sharing, a_context_made_from_no_gl_context_opens_no_gl_object)
{ ClIcdRig rig;
  Found     found(rig);
  cl_int error = CL_INVALID_VALUE;
  const cl_context context = found.create(nullptr, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(context != nullptr);
  CHECK_EQ(rig.gl_share().asked(), 0u);
  const cl_icd_dispatch *const table = rig.loader().dispatch_of(context);
  cl_int refused = CL_SUCCESS;
  CHECK(table->clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, 7u, &refused) == nullptr);
  CHECK_EQ(refused, CL_INVALID_CONTEXT);
  refused = CL_SUCCESS;
  CHECK(table->clCreateFromGLTexture(context, CL_MEM_READ_ONLY, 0x0DE1u, 0, 9u, &refused) == nullptr);
  CHECK_EQ(refused, CL_INVALID_CONTEXT);
  refused = CL_SUCCESS;
  CHECK(table->clCreateFromGLRenderbuffer(context, CL_MEM_WRITE_ONLY, 3u, &refused) == nullptr);
  CHECK_EQ(refused, CL_INVALID_CONTEXT); }

TEST(cl_gl_sharing, the_device_of_the_current_gl_context_is_the_one_device)
{ ClIcdRig rig;
  Found     found(rig);
  rig.gl_share().make_current(current_gl, gl_share_name);
  const cl_icd_dispatch *const table = rig.loader().dispatch_of(found.device());
  const cl_context_properties current[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                           CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  cl_device_id named = nullptr;
  std::size_t  size = 0u;
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof named, &named, &size),
           CL_SUCCESS);
  CHECK(named == found.device());
  CHECK_EQ(size, sizeof(cl_device_id));
  named = nullptr;
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_DEVICES_FOR_GL_CONTEXT_KHR, sizeof named, &named, nullptr),
           CL_SUCCESS);
  CHECK(named == found.device());
  /* A context nobody vouches for has no device, and a question the extension
   * does not ask is refused, not answered. */
  const cl_context_properties other[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(other_gl),
                                         CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  CHECK_EQ(table->clGetGLContextInfoKHR(other, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof named, &named, nullptr),
           CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR);
  CHECK_EQ(table->clGetGLContextInfoKHR(current, 0x2005u, sizeof named, &named, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, 1u, &named, nullptr),
           CL_INVALID_VALUE); }

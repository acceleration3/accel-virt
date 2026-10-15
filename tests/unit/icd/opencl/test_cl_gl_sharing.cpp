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
#include "proto/escape/av_context_record.hpp"
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
  /* The device's extensions, asked once and kept: it reports the extension. */
  rig.host().cl_info(CL_SUCCESS, "cl_khr_icd cl_khr_gl_sharing", 29u);
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


/* A buffer over a GL buffer is the host's object over that name, and what it
 * says of itself is the host's answer: a buffer made, then asked which GL
 * object it is. A host that made nothing is CL_INVALID_GL_OBJECT whether or
 * not it said why, and a cl_mem the ICD never made is refused without asking. */
TEST(cl_gl_sharing, create_from_gl_buffer_makes_the_host_s_object_and_gl_object_info_answers_for_it)
{ ClIcdRig rig;
  Found     found(rig);
  rig.gl_share().make_current(current_gl, gl_share_name);
  const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                              CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  cl_int error = CL_INVALID_VALUE;
  const cl_context context = found.create(properties, error);
  CHECK_EQ(error, CL_SUCCESS);

  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_int made = CL_INVALID_VALUE;
  const cl_mem buffer = rig.root().create_from_gl_buffer(context, CL_MEM_READ_WRITE, 7u, &made);
  CHECK_EQ(made, CL_SUCCESS);
  CHECK(buffer != nullptr);

  rig.host().cl_gl_object(CL_SUCCESS, CL_GL_OBJECT_BUFFER, 7u);
  cl_gl_object_type type = 0u;
  cl_GLuint         name = 0u;
  CHECK_EQ(rig.root().gl_object_info(buffer, &type, &name), CL_SUCCESS);
  CHECK_EQ(type, static_cast<cl_gl_object_type>(CL_GL_OBJECT_BUFFER));
  CHECK_EQ(name, 7u);
  CHECK_EQ(rig.root().gl_object_info(nullptr, &type, &name), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().gl_object_info(reinterpret_cast<cl_mem>(context), &type, &name), CL_INVALID_MEM_OBJECT);

  rig.host().cl_handle(0u, CL_INVALID_GL_OBJECT);
  made = CL_SUCCESS;
  CHECK(rig.root().create_from_gl_buffer(context, CL_MEM_READ_WRITE, 8u, &made) == nullptr);
  CHECK_EQ(made, CL_INVALID_GL_OBJECT);
  rig.host().cl_handle(0u, CL_SUCCESS);
  made = CL_SUCCESS;
  CHECK(rig.root().create_from_gl_buffer(context, CL_MEM_READ_WRITE, 9u, &made) == nullptr);
  CHECK_EQ(made, CL_INVALID_GL_OBJECT);
  made = CL_SUCCESS;
  CHECK(rig.root().create_from_gl_buffer(nullptr, CL_MEM_READ_WRITE, 7u, &made) == nullptr);
  CHECK_EQ(made, CL_INVALID_CONTEXT); }

/* A host whose OpenCL makes no context against its GL - NVIDIA's refuses every
 * EGL context, and the executor strikes the extension - leaves the device
 * without cl_khr_gl_sharing, and the current GL context then has no device:
 * an application that asks is not led to a clCreateContext that must fail. */
TEST(cl_gl_sharing, a_device_without_the_extension_is_no_device_of_the_gl_context)
{ ClIcdRig rig;
  Found     found(rig);
  rig.gl_share().make_current(current_gl, gl_share_name);
  const cl_icd_dispatch *const table = rig.loader().dispatch_of(found.device());
  const cl_context_properties current[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                           CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  rig.host().cl_info(CL_SUCCESS, "cl_khr_icd", 11u);
  cl_device_id named = nullptr;
  std::size_t  size = 7u;
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof named, &named, &size),
           CL_SUCCESS);
  CHECK(named == nullptr);
  CHECK_EQ(size, 0u);
  size = 7u;
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_DEVICES_FOR_GL_CONTEXT_KHR, sizeof named, &named, &size),
           CL_SUCCESS);
  CHECK(named == nullptr);
  CHECK_EQ(size, 0u); }

/* A host that will not say which extensions its device has leaves the question
 * unanswered, with the host's own error. */
TEST(cl_gl_sharing, a_host_that_will_not_list_the_extensions_answers_its_error)
{ ClIcdRig rig;
  Found     found(rig);
  rig.gl_share().make_current(current_gl, gl_share_name);
  const cl_icd_dispatch *const table = rig.loader().dispatch_of(found.device());
  const cl_context_properties current[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                           CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  rig.host().cl_info(CL_OUT_OF_HOST_MEMORY, "", 0u);
  cl_device_id named = nullptr;
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof named, &named, nullptr),
           CL_OUT_OF_HOST_MEMORY);
  CHECK(named == nullptr); }

namespace {

std::uint32_t commands_sent(void *kmt) { return static_cast<const av::MockKmt *>(kmt)->commands(); }

}  // namespace

/* Our OpenGL ICD holds a context's calls and sends them a batch at a time, and
 * the host runs the GL context and the OpenCL one on workers of their own: a
 * texture glGenTextures named and a bind still held made is nothing on the
 * host yet. So each create from a GL object has the calling thread's GL
 * context's held calls run first, before anything of the create is sent -
 * DaVinci Resolve registers its viewer textures on the thread that made them,
 * and was refused every one (CL_INVALID_GL_OBJECT). A refused context asks
 * nothing of GL. */
TEST(cl_gl_sharing, each_create_from_a_gl_object_runs_the_held_gl_calls_before_it_is_sent)
{ ClIcdRig rig;
  Found     found(rig);
  rig.gl_share().make_current(current_gl, gl_share_name);
  rig.gl_share().count_commands_with(&commands_sent, &rig.kmt());
  const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                              CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  cl_int error = CL_INVALID_VALUE;
  const cl_context context = found.create(properties, error);
  CHECK_EQ(error, CL_SUCCESS);
  if (context == nullptr) return;
  const cl_icd_dispatch *const table = rig.loader().dispatch_of(context);
  CHECK_EQ(rig.gl_share().runs(), 0u);

  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_int made = CL_INVALID_VALUE;
  CHECK(table->clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, 7u, &made) != nullptr);
  CHECK_EQ(made, CL_SUCCESS);
  CHECK_EQ(rig.gl_share().runs(), 1u);
  CHECK_EQ(rig.gl_share().commands_at_run(), sent);

  sent = rig.kmt().commands();
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  CHECK(table->clCreateFromGLTexture(context, CL_MEM_READ_WRITE, 0x0DE1u, 0, 186u, &made) != nullptr);
  CHECK_EQ(rig.gl_share().runs(), 2u);
  CHECK_EQ(rig.gl_share().commands_at_run(), sent);

  sent = rig.kmt().commands();
  rig.host().cl_handle(host_buffer + 2u, CL_SUCCESS);
  CHECK(table->clCreateFromGLRenderbuffer(context, CL_MEM_WRITE_ONLY, 3u, &made) != nullptr);
  CHECK_EQ(rig.gl_share().runs(), 3u);
  CHECK_EQ(rig.gl_share().commands_at_run(), sent);

  /* A context made from no GL context is refused before GL is asked. */
  const cl_context plain = found.create(nullptr, error);
  CHECK_EQ(error, CL_SUCCESS);
  if (plain == nullptr) return;
  CHECK(table->clCreateFromGLTexture(plain, CL_MEM_READ_ONLY, 0x0DE1u, 0, 9u, &made) == nullptr);
  CHECK_EQ(made, CL_INVALID_CONTEXT);
  CHECK_EQ(rig.gl_share().runs(), 3u); }

#include "executor/opencl/direct_gl_sharing.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/gl_renderbuffer_texture.hpp"
#include "executor/shim/opencl_device.hpp"
#include "harness/executor/host_gl_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <CL/cl_gl.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libgallium\n"
           "leak:libLLVM\n"
           "leak:libz\n";
}

namespace av {
struct DirectGlSharingProbe {
    DirectGlSharing &sharing;

    cl_mem over_texture(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int *error) const
    { return sharing.over_texture(context, flags, renderbuffer, error); }
};

}

using av::DirectGlSharing;
using av::DirectGlSharingProbe;
using av::HostGlRig;

namespace {

constexpr std::uint32_t gl_rgba8 = 0x8058u;

class Shared {
public:
    explicit Shared(HostGlRig &rig)
        : platform_(), vehicle_(), textures_(), sharing_(platform_, vehicle_, textures_), context_(nullptr), queue_(nullptr)
    {
        const av::GlContextAttributes plain(nullptr);
        if (!rig.ready() || !vehicle_.open(rig.display(), plain, &rig.gl())) return;
        textures_.configure(vehicle_, rig.display());
        sharing_.configure(rig.display());
        cl_device_id const          device = static_cast<cl_device_id>(rig.device().device());
        const cl_context_properties properties[] = {
            CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(rig.device().platform()), 0};
        cl_int error = CL_SUCCESS;
        context_ = sharing_.make_context(properties, 1u, &device, &error);
        if (context_ != nullptr) queue_ = ::clCreateCommandQueueWithProperties(context_, device, nullptr, &error);
    }
    ~Shared()
    {
        sharing_.forget_all();
        if (queue_ != nullptr) ::clReleaseCommandQueue(queue_);
        if (context_ != nullptr) ::clReleaseContext(context_);
        textures_.give_back();
        vehicle_.close();
    }
    Shared(const Shared &) = delete;
    Shared &operator=(const Shared &) = delete;

    bool             ready() const { return queue_ != nullptr; }
    DirectGlSharing &sharing() { return sharing_; }
    av::MockPlatform &platform() { return platform_; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    const void      *vehicle() const { return vehicle_.handle(); }

private:
    av::MockPlatform          platform_;
    av::GlContext             vehicle_;
    av::GlRenderbufferTexture textures_;
    DirectGlSharing           sharing_;
    cl_context                context_;
    cl_command_queue          queue_;
};

}

TEST(direct_gl_sharing_device, the_guest_s_context_names_the_vehicle)
{ HostGlRig rig;
  Shared    shared(rig);
  if (!shared.ready()) { std::fprintf(stderr, "this host's OpenCL makes no context against the vehicle\n"); return; }
  cl_context_properties named[16] = {};
  std::size_t           bytes = 0u;
  CHECK_EQ(::clGetContextInfo(shared.context(), CL_CONTEXT_PROPERTIES, sizeof named, named, &bytes), CL_SUCCESS);
  bool vehicle = false;
  for (std::size_t at = 0u; at + 1u < bytes / sizeof named[0]; at += 2u)
      vehicle = vehicle || (named[at] == CL_GL_CONTEXT_KHR &&
                            named[at + 1u] == reinterpret_cast<cl_context_properties>(shared.vehicle()));
  CHECK(vehicle);

  cl_device_id const    device = static_cast<cl_device_id>(rig.device().device());
  cl_context_properties overlong[34] = {};
  for (std::uint32_t at = 0u; at + 1u < 32u; at += 2u) {
      overlong[at] = CL_CONTEXT_PLATFORM;
      overlong[at + 1u] = reinterpret_cast<cl_context_properties>(rig.device().platform());
  }
  cl_int error = CL_SUCCESS;
  CHECK(shared.sharing().make_context(overlong, 1u, &device, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK(shared.sharing().make_context(overlong, 1u, &device, nullptr) == nullptr);
  cl_context const bare = shared.sharing().make_context(nullptr, 1u, &device, &error);
  CHECK(bare != nullptr);
  ::clReleaseContext(bare); }

TEST(direct_gl_sharing_device, a_gl_object_is_the_host_s_own)
{ HostGlRig rig;
  Shared    shared(rig);
  if (!shared.ready()) return;
  std::uint8_t        bytes[256] = {};
  const std::uint32_t buffer = rig.buffer(bytes, sizeof bytes);
  cl_int              error = CL_INVALID_VALUE;
  cl_mem const        mem = shared.sharing().from_buffer(shared.context(), CL_MEM_READ_WRITE, buffer, &error);
  CHECK(mem != nullptr);
  CHECK_EQ(shared.sharing().acquire(shared.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  std::memset(bytes, 0x3C, sizeof bytes);
  CHECK_EQ(::clEnqueueWriteBuffer(shared.queue(), mem, CL_TRUE, 0u, sizeof bytes, bytes, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(shared.sharing().release(shared.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(::clFinish(shared.queue()), CL_SUCCESS);
  std::uint8_t seen[256] = {};
  rig.read_buffer(buffer, seen, sizeof seen);
  CHECK(std::memcmp(seen, bytes, sizeof bytes) == 0);
  cl_gl_object_type type = 0u;
  cl_GLuint         name = 0u;
  CHECK_EQ(shared.sharing().object_info(mem, &type, &name), CL_SUCCESS);
  CHECK_EQ(type, static_cast<cl_gl_object_type>(CL_GL_OBJECT_BUFFER));
  CHECK_EQ(name, buffer);

  const std::uint32_t texture = rig.texture(0x0DE1u, 0, gl_rgba8, 0x1908u, 0x1401u, 4u, 4u, 1u, nullptr);
  cl_mem const        image = shared.sharing().from_texture(shared.context(), CL_MEM_READ_ONLY, 0x0DE1u, 0, texture, &error);
  CHECK(image != nullptr);
  cl_GLenum target = 0u;
  CHECK_EQ(shared.sharing().texture_info(image, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr), CL_SUCCESS);
  CHECK_EQ(target, 0x0DE1u);
  shared.sharing().forget(mem);
  CHECK_EQ(shared.sharing().renderbuffer_images(), 0u);
  ::clReleaseMemObject(image);
  ::clReleaseMemObject(mem);
  rig.delete_texture(texture);
  rig.delete_buffer(buffer); }

TEST(direct_gl_sharing_device, a_renderbuffer_the_host_will_not_name_is_read_through_a_texture_of_its_storage)
{ HostGlRig rig;
  Shared    shared(rig);
  if (!shared.ready()) return;
  DirectGlSharingProbe probe{shared.sharing()};
  const float          red[4] = {0.8f, 0.1f, 0.3f, 1.0f};
  const std::uint32_t  renderbuffer = rig.renderbuffer(gl_rgba8, 16u, 16u, red);
  cl_int               error = CL_INVALID_VALUE;
  cl_mem const         over = probe.over_texture(shared.context(), CL_MEM_READ_ONLY, renderbuffer, &error);
  if (over == nullptr) {
      std::fprintf(stderr, "this host makes no texture of a renderbuffer (%d)\n", error);
      rig.delete_renderbuffer(renderbuffer);
      return;
  }
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(shared.sharing().renderbuffer_images(), 1u);
  cl_gl_object_type type = 0u;
  cl_GLuint         name = 0u;
  CHECK_EQ(shared.sharing().object_info(over, &type, &name), CL_SUCCESS);
  CHECK_EQ(type, static_cast<cl_gl_object_type>(CL_GL_OBJECT_RENDERBUFFER));
  CHECK_EQ(name, renderbuffer);
  CHECK_EQ(shared.sharing().object_info(over, nullptr, nullptr), CL_SUCCESS);
  cl_GLenum target = 0u;
  CHECK_EQ(shared.sharing().texture_info(over, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr),
           CL_INVALID_GL_OBJECT);
  const std::size_t at[3] = {5u, 7u, 0u}, one[3] = {1u, 1u, 1u};
  std::uint8_t      pixel[4] = {};
  CHECK_EQ(shared.sharing().acquire(shared.queue(), 1u, &over, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(::clEnqueueReadImage(shared.queue(), over, CL_TRUE, at, one, 0u, 0u, pixel, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(shared.sharing().release(shared.queue(), 1u, &over, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK(pixel[0] >= 203u && pixel[0] <= 205u);
  CHECK(pixel[1] >= 25u && pixel[1] <= 27u);
  shared.sharing().forget(over);
  CHECK_EQ(shared.sharing().renderbuffer_images(), 0u);
  ::clReleaseMemObject(over);

  error = CL_INVALID_GL_OBJECT;
  CHECK(probe.over_texture(shared.context(), CL_MEM_READ_ONLY, renderbuffer + 1000u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_GL_OBJECT);
  error = CL_INVALID_GL_OBJECT;
  CHECK(probe.over_texture(nullptr, CL_MEM_READ_ONLY, renderbuffer, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_GL_OBJECT);
  CHECK(probe.over_texture(nullptr, CL_MEM_READ_ONLY, renderbuffer, nullptr) == nullptr);
  CHECK_EQ(shared.sharing().renderbuffer_images(), 0u);
  constexpr std::uint32_t many = 100u;
  static cl_mem held[many + 64u];
  for (std::uint32_t index = 0u; index < many; index++)
      held[index] = probe.over_texture(shared.context(), CL_MEM_READ_ONLY, renderbuffer, nullptr);
  CHECK_EQ(shared.sharing().renderbuffer_images(), many);
  shared.platform().fail_allocation_at(1u);
  std::uint32_t made = many;
  error = CL_SUCCESS;
  while (made < many + 64u &&
         (held[made] = probe.over_texture(shared.context(), CL_MEM_READ_ONLY, renderbuffer, &error)) != nullptr)
      made++;
  CHECK(made < many + 64u);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(shared.sharing().renderbuffer_images(), made);
  shared.sharing().forget_all();
  CHECK_EQ(shared.sharing().renderbuffer_images(), 0u);
  for (std::uint32_t index = 0u; index < made; index++) ::clReleaseMemObject(held[index]);

  cl_mem const made = shared.sharing().from_renderbuffer(shared.context(), CL_MEM_READ_ONLY, renderbuffer, &error);
  CHECK(made != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(shared.sharing().from_renderbuffer(shared.context(), 0xFFu, renderbuffer, &error) == nullptr);
  CHECK(error != CL_SUCCESS);
  CHECK(shared.sharing().from_renderbuffer(shared.context(), CL_MEM_READ_ONLY, renderbuffer + 1000u, nullptr) ==
        nullptr);
  shared.sharing().forget(made);
  ::clReleaseMemObject(made);
  rig.delete_renderbuffer(renderbuffer); }

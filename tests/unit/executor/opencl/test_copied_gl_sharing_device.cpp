/* GL objects reaching OpenCL as copies (CopiedGlSharing), on this host's own
 * GPU. The copy path is what a host whose OpenCL makes no context against the
 * vehicle gets - NVIDIA's - and nothing in it asks the host's OpenCL to share:
 * the cl_mems are plain ones of a context that names no GL context. So it runs
 * here as it runs there, on any GPU with OpenCL and EGL, and what is judged is
 * the bytes: what the guest's GL holds is what OpenCL reads after an acquire,
 * and what OpenCL wrote is what GL reads after a release.
 *
 * The guest's GL is a context of its own (HostGlRig) and the vehicle another
 * in its share group, as on the host; a host with no OpenCL and EGL GPU
 * returns without judging. */
#include "executor/opencl/copied_gl_sharing.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_object_bytes.hpp"
#include "executor/shim/gl_renderbuffer_texture.hpp"
#include "harness/executor/host_gl_rig.hpp"
#include "harness/framework/test_registry.hpp"

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

using av::CopiedGlSharing;
using av::HostGlRig;

namespace {

constexpr std::uint32_t gl_texture_1d = 0x0DE0u, gl_texture_2d = 0x0DE1u, gl_texture_3d = 0x806Fu,
                        gl_texture_1d_array = 0x8C18u, gl_texture_2d_array = 0x8C1Au,
                        gl_texture_rectangle = 0x84F5u, gl_cube_negative_y = 0x8518u;
constexpr std::uint32_t gl_rgba8 = 0x8058u, gl_rgba32f = 0x8814u, gl_r32ui = 0x8236u, gl_rg8 = 0x822Bu,
                        gl_r16 = 0x822Au, gl_rgb8 = 0x8051u;
constexpr std::uint32_t gl_rgba = 0x1908u, gl_rg = 0x8227u, gl_red = 0x1903u, gl_red_integer = 0x8D94u,
                        gl_rgb = 0x1907u;
constexpr std::uint32_t gl_unsigned_byte = 0x1401u, gl_unsigned_short = 0x1403u, gl_unsigned_int = 0x1405u,
                        gl_float = 0x1406u;

/* The vehicle: a context in the rig's share group, and the two things the
 * copies go through on it. */
class Vehicle {
public:
    explicit Vehicle(HostGlRig &rig) : context_(), objects_(), textures_()
    {
        const av::GlContextAttributes plain(nullptr);
        if (!rig.ready() || !context_.open(rig.display(), plain, &rig.gl())) return;
        objects_.configure(context_);
        textures_.configure(context_, rig.display());
    }
    ~Vehicle()
    {
        objects_.give_back();
        textures_.give_back();
        context_.close();
    }
    Vehicle(const Vehicle &) = delete;
    Vehicle &operator=(const Vehicle &) = delete;

    bool                       ready() const { return objects_.moves_bytes() && textures_.makes_textures(); }
    av::GlObjectBytes         &objects() { return objects_; }
    av::GlRenderbufferTexture &textures() { return textures_; }

private:
    av::GlContext             context_;
    av::GlObjectBytes         objects_;
    av::GlRenderbufferTexture textures_;
};

constexpr std::size_t buffer_bytes = 4096u;

std::uint8_t pattern(std::size_t at, std::uint8_t seed) { return static_cast<std::uint8_t>((at * 7u + seed) % 251u); }

void fill(std::uint8_t *bytes, std::size_t count, std::uint8_t seed)
{
    for (std::size_t at = 0u; at < count; at++) bytes[at] = pattern(at, seed);
}

bool filled(const std::uint8_t *bytes, std::size_t count, std::uint8_t seed)
{
    for (std::size_t at = 0u; at < count; at++)
        if (bytes[at] != pattern(at, seed)) return false;
    return true;
}

}  // namespace

/* The whole turn of a buffer: GL's bytes are OpenCL's after the acquire, and
 * OpenCL's are GL's after the release; each gives the guest the event it asked
 * for, and the cl_mem says what GL object it is. */
TEST(copied_gl_sharing_device, a_buffer_s_bytes_cross_at_the_acquire_and_come_back_at_the_release)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) { std::fprintf(stderr, "no OpenCL and EGL GPU here\n"); return; }
  CopiedGlSharing sharing(vehicle.objects(), vehicle.textures());
  static std::uint8_t bytes[buffer_bytes];
  fill(bytes, sizeof bytes, 1u);
  const std::uint32_t name = rig.buffer(bytes, sizeof bytes);
  cl_int              error = CL_INVALID_VALUE;
  cl_mem const        mem = sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, name, &error);
  CHECK(mem != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(sharing.mirrors(), 1u);
  std::size_t size = 0u;
  CHECK_EQ(::clGetMemObjectInfo(mem, CL_MEM_SIZE, sizeof size, &size, nullptr), CL_SUCCESS);
  CHECK_EQ(size, buffer_bytes);

  cl_event acquired = nullptr;
  CHECK_EQ(sharing.acquire(rig.queue(), 1u, &mem, 0u, nullptr, &acquired), CL_SUCCESS);
  CHECK(acquired != nullptr);
  CHECK_EQ(::clWaitForEvents(1u, &acquired), CL_SUCCESS);
  static std::uint8_t seen[buffer_bytes];
  CHECK_EQ(::clEnqueueReadBuffer(rig.queue(), mem, CL_TRUE, 0u, sizeof seen, seen, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK(filled(seen, sizeof seen, 1u));
  fill(bytes, sizeof bytes, 2u);
  CHECK_EQ(::clEnqueueWriteBuffer(rig.queue(), mem, CL_TRUE, 0u, sizeof bytes, bytes, 0u, nullptr, nullptr), CL_SUCCESS);
  /* Waiting on the acquire's event is a wait list the release takes. */
  cl_event released = nullptr;
  CHECK_EQ(sharing.release(rig.queue(), 1u, &mem, 1u, &acquired, &released), CL_SUCCESS);
  CHECK(released != nullptr);
  CHECK_EQ(::clWaitForEvents(1u, &released), CL_SUCCESS);
  std::memset(seen, 0, sizeof seen);
  rig.read_buffer(name, seen, sizeof seen);
  CHECK(filled(seen, sizeof seen, 2u));

  cl_gl_object_type type = 0u;
  cl_GLuint         named = 0u;
  CHECK_EQ(sharing.object_info(mem, &type, &named), CL_SUCCESS);
  CHECK_EQ(type, static_cast<cl_gl_object_type>(CL_GL_OBJECT_BUFFER));
  CHECK_EQ(named, name);
  CHECK_EQ(sharing.object_info(mem, nullptr, nullptr), CL_SUCCESS);
  cl_GLenum target = 0u;
  CHECK_EQ(sharing.texture_info(mem, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr), CL_INVALID_GL_OBJECT);

  ::clReleaseEvent(acquired);
  ::clReleaseEvent(released);
  sharing.forget(mem);
  CHECK_EQ(sharing.mirrors(), 0u);
  CHECK_EQ(sharing.object_info(mem, &type, &named), CL_INVALID_GL_OBJECT);
  ::clReleaseMemObject(mem);
  rig.delete_buffer(name); }

/* Read-only is never copied back, whatever OpenCL did to it; write-only is
 * still copied in, so what a kernel leaves alone is GL's still; and an object
 * released without being acquired is not copied back at all. */
TEST(copied_gl_sharing_device, only_what_opencl_may_have_written_is_copied_back)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  static std::uint8_t bytes[buffer_bytes], seen[buffer_bytes];
  fill(bytes, sizeof bytes, 3u);
  const std::uint32_t read_only = rig.buffer(bytes, sizeof bytes);
  const std::uint32_t write_only = rig.buffer(bytes, sizeof bytes);
  const std::uint32_t untaken = rig.buffer(bytes, sizeof bytes);
  cl_int              error = CL_SUCCESS;
  cl_mem const        reader = sharing.from_buffer(rig.context(), CL_MEM_READ_ONLY, read_only, &error);
  cl_mem const        writer = sharing.from_buffer(rig.context(), CL_MEM_WRITE_ONLY, write_only, &error);
  cl_mem const        idle = sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, untaken, &error);
  CHECK(reader != nullptr && writer != nullptr && idle != nullptr);
  const cl_mem both[2] = {reader, writer};
  CHECK_EQ(sharing.acquire(rig.queue(), 2u, both, 0u, nullptr, nullptr), CL_SUCCESS);
  const std::uint8_t ones[16] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  const cl_mem       three[3] = {reader, writer, idle};
  for (cl_mem const each : three)
      CHECK_EQ(::clEnqueueWriteBuffer(rig.queue(), each, CL_TRUE, 0u, sizeof ones, ones, 0u, nullptr, nullptr),
               CL_SUCCESS);
  CHECK_EQ(sharing.release(rig.queue(), 2u, both, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(sharing.release(rig.queue(), 1u, &idle, 0u, nullptr, nullptr), CL_SUCCESS);
  rig.read_buffer(read_only, seen, sizeof seen);
  CHECK(filled(seen, sizeof seen, 3u));
  rig.read_buffer(untaken, seen, sizeof seen);
  CHECK(filled(seen, sizeof seen, 3u));
  rig.read_buffer(write_only, seen, sizeof seen);
  CHECK(std::memcmp(seen, ones, sizeof ones) == 0);
  bool rest_kept = true;
  for (std::size_t at = sizeof ones; at < sizeof seen; at++) rest_kept = rest_kept && seen[at] == pattern(at, 3u);
  CHECK(rest_kept);
  /* A second release copies nothing back: the turn is GL's already. */
  const std::uint8_t twos[16] = {2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u};
  CHECK_EQ(::clEnqueueWriteBuffer(rig.queue(), writer, CL_TRUE, 0u, sizeof twos, twos, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(sharing.release(rig.queue(), 1u, &writer, 0u, nullptr, nullptr), CL_SUCCESS);
  rig.read_buffer(write_only, seen, sizeof seen);
  CHECK(std::memcmp(seen, ones, sizeof ones) == 0);
  sharing.forget_all();
  CHECK_EQ(sharing.mirrors(), 0u);
  for (cl_mem const each : three) ::clReleaseMemObject(each);
  const std::uint32_t names[3] = {read_only, write_only, untaken};
  for (const std::uint32_t each : names) rig.delete_buffer(each); }

/* What clCreateFromGLBuffer refuses: flags that are not one access flag
 * alone, and a name that is no buffer with a store; and a context the host
 * refuses is the host's answer. Nothing is remembered for any of them. */
TEST(copied_gl_sharing_device, a_buffer_that_cannot_be_mirrored_is_refused_as_the_extension_says)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const std::uint32_t name = rig.buffer(nullptr, 64u);
  const cl_mem_flags  bad_flags[] = {0u, CL_MEM_READ_WRITE | CL_MEM_READ_ONLY, CL_MEM_USE_HOST_PTR,
                                     CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR};
  for (const cl_mem_flags flags : bad_flags) {
      cl_int error = CL_SUCCESS;
      CHECK(sharing.from_buffer(rig.context(), flags, name, &error) == nullptr);
      CHECK_EQ(error, CL_INVALID_VALUE);
  }
  cl_int error = CL_SUCCESS;
  CHECK(sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_GL_OBJECT);
  CHECK(sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, name + 1000u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_GL_OBJECT);
  /* A buffer with no store yet. */
  const std::uint32_t empty = rig.buffer(nullptr, 0u);
  CHECK(sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, empty, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_GL_OBJECT);
  CHECK(sharing.from_buffer(nullptr, CL_MEM_READ_WRITE, name, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(sharing.from_buffer(nullptr, CL_MEM_READ_WRITE, name, nullptr) == nullptr);
  CHECK_EQ(sharing.mirrors(), 0u);
  rig.delete_buffer(empty);
  rig.delete_buffer(name); }

/* As many as the table holds, and one more refused with its cl_mem let go. */
TEST(copied_gl_sharing_device, a_full_table_refuses_the_next_mirror_and_releases_its_cl_mem)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const std::uint32_t name = rig.buffer(nullptr, 64u);
  static cl_mem       made[av::ClGlMirrors::max_mirrors];
  for (cl_mem &each : made) each = sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, name, nullptr);
  CHECK_EQ(sharing.mirrors(), av::ClGlMirrors::max_mirrors);
  cl_int error = CL_SUCCESS;
  CHECK(sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, name, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, name, nullptr) == nullptr);
  sharing.forget_all();
  for (cl_mem each : made) ::clReleaseMemObject(each);
  rig.delete_buffer(name); }

/* What an acquire or a release refuses before copying anything: objects that
 * do not match their count, a cl_mem made from no GL object, a wait list the
 * host refuses. No objects at all is a marker. */
TEST(copied_gl_sharing_device, an_acquire_or_release_of_what_is_no_mirror_copies_nothing)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const std::uint32_t name = rig.buffer(nullptr, 64u);
  cl_mem const        mem = sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, name, nullptr);
  cl_int              error = CL_SUCCESS;
  cl_mem const        plain = ::clCreateBuffer(rig.context(), CL_MEM_READ_WRITE, 64u, nullptr, &error);
  const cl_mem        mixed[2] = {mem, plain};
  for (int pass = 0; pass < 2; pass++) {
      auto const turn = pass == 0 ? &CopiedGlSharing::acquire : &CopiedGlSharing::release;
      CHECK_EQ((sharing.*turn)(rig.queue(), 1u, nullptr, 0u, nullptr, nullptr), CL_INVALID_VALUE);
      CHECK_EQ((sharing.*turn)(rig.queue(), 0u, &mem, 0u, nullptr, nullptr), CL_INVALID_VALUE);
      CHECK_EQ((sharing.*turn)(rig.queue(), 2u, mixed, 0u, nullptr, nullptr), CL_INVALID_GL_OBJECT);
      CHECK_EQ((sharing.*turn)(rig.queue(), 1u, &mem, 1u, nullptr, nullptr), CL_INVALID_EVENT_WAIT_LIST);
      cl_event marker = nullptr;
      CHECK_EQ((sharing.*turn)(rig.queue(), 0u, nullptr, 0u, nullptr, &marker), CL_SUCCESS);
      CHECK(marker != nullptr);
      CHECK_EQ(::clWaitForEvents(1u, &marker), CL_SUCCESS);
      ::clReleaseEvent(marker);
  }
  sharing.forget_all();
  ::clReleaseMemObject(plain);
  ::clReleaseMemObject(mem);
  rig.delete_buffer(name); }

/* A GL object deleted while its cl_mem lives reads and writes as nothing:
 * the GL object is refused. A copy the host refuses is the host's answer,
 * and leaves the object unacquired. */
TEST(copied_gl_sharing_device, a_gl_object_gone_or_a_copy_the_host_refuses_is_answered_as_such)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const std::uint32_t kept = rig.buffer(nullptr, 64u);
  const std::uint32_t doomed = rig.buffer(nullptr, 64u);
  cl_mem const        mem = sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, kept, nullptr);
  cl_mem const        gone = sharing.from_buffer(rig.context(), CL_MEM_READ_WRITE, doomed, nullptr);
  CHECK(mem != nullptr && gone != nullptr);
  /* Gone before the acquire. */
  rig.delete_buffer(doomed);
  CHECK_EQ(sharing.acquire(rig.queue(), 1u, &gone, 0u, nullptr, nullptr), CL_INVALID_GL_OBJECT);

  /* A queue of another context: the barrier is taken, the copy is not. */
  cl_int                      error = CL_SUCCESS;
  cl_device_id const          device = static_cast<cl_device_id>(rig.device().device());
  const cl_context_properties properties[] = {
      CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(rig.device().platform()), 0};
  cl_context const       other = ::clCreateContext(properties, 1u, &device, nullptr, nullptr, &error);
  cl_command_queue const elsewhere = ::clCreateCommandQueueWithProperties(other, device, nullptr, &error);
  CHECK_EQ(sharing.acquire(elsewhere, 1u, &mem, 0u, nullptr, nullptr), CL_INVALID_CONTEXT);
  /* Not acquired, so a release copies nothing back and is no refusal. */
  CHECK_EQ(sharing.release(elsewhere, 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(sharing.acquire(rig.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(sharing.release(elsewhere, 1u, &mem, 0u, nullptr, nullptr), CL_INVALID_CONTEXT);
  /* Gone between the acquire and the release. */
  CHECK_EQ(sharing.acquire(rig.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  rig.delete_buffer(kept);
  CHECK_EQ(sharing.release(rig.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_INVALID_GL_OBJECT);
  sharing.forget_all();
  ::clReleaseCommandQueue(elsewhere);
  ::clReleaseContext(other);
  ::clReleaseMemObject(gone);
  ::clReleaseMemObject(mem); }

namespace {

struct Level {
    std::uint32_t     target;
    std::int32_t      level;
    std::uint32_t     internal, format, type, width, height, depth, texel;
    cl_gl_object_type object;
};

/* Texels a format holds exactly whichever way they cross: bytes for the
 * normalized and integer formats, small whole floats for the float ones. */
void texels_of(const Level &shape, std::uint8_t *into, std::size_t bytes, std::uint8_t seed)
{
    if (shape.type != gl_float) {
        fill(into, bytes, seed);
        return;
    }
    for (std::size_t at = 0u; at < bytes / sizeof(float); at++) {
        const float value = static_cast<float>(pattern(at, seed)) * 0.5f - 7.0f;
        std::memcpy(into + at * sizeof value, &value, sizeof value);
    }
}

}  // namespace

/* Every kind of level the extension names, at a format of each kind: what GL
 * holds is the image OpenCL reads after the acquire, byte for byte, and what
 * OpenCL writes is what GL reads after the release. The cl_mem is the image
 * the level is, and says which target and level it was named at. */
TEST(copied_gl_sharing_device, each_kind_of_texture_level_crosses_both_ways_byte_for_byte)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing sharing(vehicle.objects(), vehicle.textures());
  const Level     levels[] = {
      {gl_texture_2d, 0, gl_rgba8, gl_rgba, gl_unsigned_byte, 16u, 8u, 1u, 4u, CL_GL_OBJECT_TEXTURE2D},
      {gl_texture_2d, 1, gl_rgba32f, gl_rgba, gl_float, 8u, 4u, 1u, 16u, CL_GL_OBJECT_TEXTURE2D},
      {gl_texture_rectangle, 0, gl_rgba8, gl_rgba, gl_unsigned_byte, 12u, 5u, 1u, 4u, CL_GL_OBJECT_TEXTURE2D},
      {gl_cube_negative_y, 0, gl_rgba8, gl_rgba, gl_unsigned_byte, 8u, 8u, 1u, 4u, CL_GL_OBJECT_TEXTURE2D},
      {gl_texture_1d, 0, gl_r16, gl_red, gl_unsigned_short, 32u, 1u, 1u, 2u, CL_GL_OBJECT_TEXTURE1D},
      {gl_texture_1d_array, 0, gl_rgba8, gl_rgba, gl_unsigned_byte, 16u, 3u, 1u, 4u, CL_GL_OBJECT_TEXTURE1D_ARRAY},
      {gl_texture_2d_array, 0, gl_r32ui, gl_red_integer, gl_unsigned_int, 8u, 4u, 3u, 4u,
       CL_GL_OBJECT_TEXTURE2D_ARRAY},
      {gl_texture_3d, 0, gl_rg8, gl_rg, gl_unsigned_byte, 8u, 4u, 2u, 2u, CL_GL_OBJECT_TEXTURE3D},
  };
  static std::uint8_t sent[16384], seen[16384], back[16384];
  for (const Level &shape : levels) {
      const std::size_t bytes = std::size_t{shape.width} * shape.height * shape.depth * shape.texel;
      texels_of(shape, sent, bytes, 5u);
      const std::uint32_t name = rig.texture(shape.target, shape.level, shape.internal, shape.format, shape.type,
                                             shape.width, shape.height, shape.depth, sent);
      cl_int       error = CL_INVALID_VALUE;
      cl_mem const mem = sharing.from_texture(rig.context(), CL_MEM_READ_WRITE, shape.target, shape.level, name, &error);
      if (mem == nullptr) {
          std::fprintf(stderr, "target %x internal %x: %d\n", shape.target, shape.internal, error);
          CHECK(mem != nullptr);
          continue;
      }
      CHECK_EQ(error, CL_SUCCESS);
      CHECK_EQ(sharing.acquire(rig.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
      const std::size_t origin[3] = {0u, 0u, 0u};
      const std::size_t region[3] = {shape.width, shape.height, shape.depth};
      std::memset(seen, 0, bytes);
      CHECK_EQ(::clEnqueueReadImage(rig.queue(), mem, CL_TRUE, origin, region, 0u, 0u, seen, 0u, nullptr, nullptr),
               CL_SUCCESS);
      CHECK(std::memcmp(seen, sent, bytes) == 0);
      texels_of(shape, sent, bytes, 9u);
      CHECK_EQ(::clEnqueueWriteImage(rig.queue(), mem, CL_TRUE, origin, region, 0u, 0u, sent, 0u, nullptr, nullptr),
               CL_SUCCESS);
      CHECK_EQ(sharing.release(rig.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
      std::memset(back, 0, bytes);
      rig.read_texture(shape.target, name, shape.level, shape.format, shape.type, back);
      CHECK(std::memcmp(back, sent, bytes) == 0);

      cl_gl_object_type type = 0u;
      cl_GLuint         named = 0u;
      CHECK_EQ(sharing.object_info(mem, &type, &named), CL_SUCCESS);
      CHECK_EQ(type, shape.object);
      CHECK_EQ(named, name);
      cl_GLenum   target = 0u;
      cl_GLint    level = -1;
      std::size_t answered = 0u;
      CHECK_EQ(sharing.texture_info(mem, CL_GL_TEXTURE_TARGET, sizeof target, &target, &answered), CL_SUCCESS);
      CHECK_EQ(target, shape.target);
      CHECK_EQ(answered, sizeof target);
      CHECK_EQ(sharing.texture_info(mem, CL_GL_MIPMAP_LEVEL, sizeof level, &level, nullptr), CL_SUCCESS);
      CHECK_EQ(level, shape.level);
      sharing.forget(mem);
      ::clReleaseMemObject(mem);
      rig.delete_texture(name);
  }
  CHECK_EQ(sharing.mirrors(), 0u); }

/* The texture questions: the size alone, room too small, a question the
 * extension does not ask, and a cl_mem of no texture. */
TEST(copied_gl_sharing_device, a_texture_question_is_answered_as_clGetGLTextureInfo_answers)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const std::uint32_t name = rig.texture(gl_texture_2d, 0, gl_rgba8, gl_rgba, gl_unsigned_byte, 4u, 4u, 1u, nullptr);
  cl_mem const        mem = sharing.from_texture(rig.context(), CL_MEM_READ_ONLY, gl_texture_2d, 0, name, nullptr);
  CHECK(mem != nullptr);
  std::size_t answered = 0u;
  CHECK_EQ(sharing.texture_info(mem, CL_GL_MIPMAP_LEVEL, 0u, nullptr, &answered), CL_SUCCESS);
  CHECK_EQ(answered, sizeof(cl_GLint));
  CHECK_EQ(sharing.texture_info(mem, CL_GL_TEXTURE_TARGET, 0u, nullptr, nullptr), CL_SUCCESS);
  std::uint8_t cramped = 0xEEu;
  CHECK_EQ(sharing.texture_info(mem, CL_GL_TEXTURE_TARGET, sizeof cramped, &cramped, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(cramped, 0xEEu);
  std::int32_t samples = 0;
  CHECK_EQ(sharing.texture_info(mem, CL_GL_NUM_SAMPLES, sizeof samples, &samples, nullptr), CL_INVALID_VALUE);
  int nothing = 0;
  CHECK_EQ(sharing.texture_info(reinterpret_cast<cl_mem>(&nothing), CL_GL_TEXTURE_TARGET, 0u, nullptr, nullptr),
           CL_INVALID_GL_OBJECT);
  sharing.forget(mem);
  ::clReleaseMemObject(mem);
  rig.delete_texture(name); }

/* What clCreateFromGLTexture refuses: a target the extension does not name,
 * a negative level, flags that are not one access flag, a name that is no
 * texture of the target or has no texels at the level, an internal format
 * OpenCL has no image for; a context the host refuses is its answer. */
TEST(copied_gl_sharing_device, a_texture_that_cannot_be_mirrored_is_refused_as_the_extension_says)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const std::uint32_t name = rig.texture(gl_texture_2d, 0, gl_rgba8, gl_rgba, gl_unsigned_byte, 4u, 4u, 1u, nullptr);
  const std::uint32_t rgb = rig.texture(gl_texture_2d, 0, gl_rgb8, gl_rgb, gl_unsigned_byte, 4u, 4u, 1u, nullptr);
  struct Case {
      cl_mem_flags  flags;
      std::uint32_t target;
      std::int32_t  level;
      std::uint32_t texture;
      cl_int        refusal;
  };
  const Case cases[] = {
      {CL_MEM_READ_WRITE, 0x8C2Au /* TEXTURE_BUFFER */, 0, name, CL_INVALID_VALUE},
      {CL_MEM_READ_WRITE, 0x9100u /* TEXTURE_2D_MULTISAMPLE */, 0, name, CL_INVALID_VALUE},
      {CL_MEM_READ_WRITE, gl_texture_2d, -1, name, CL_INVALID_MIP_LEVEL},
      {CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, gl_texture_2d, 0, name, CL_INVALID_VALUE},
      {CL_MEM_READ_WRITE, gl_texture_3d, 0, name, CL_INVALID_GL_OBJECT},
      {CL_MEM_READ_WRITE, gl_texture_2d, 0, 0u, CL_INVALID_GL_OBJECT},
      {CL_MEM_READ_WRITE, gl_texture_2d, 0, name + 1000u, CL_INVALID_GL_OBJECT},
      {CL_MEM_READ_WRITE, gl_texture_2d, 3, name, CL_INVALID_GL_OBJECT},
      {CL_MEM_READ_WRITE, gl_texture_2d, 0, rgb, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR},
  };
  for (const Case &item : cases) {
      cl_int error = CL_SUCCESS;
      CHECK(sharing.from_texture(rig.context(), item.flags, item.target, item.level, item.texture, &error) == nullptr);
      CHECK_EQ(error, item.refusal);
      CHECK(sharing.from_texture(rig.context(), item.flags, item.target, item.level, item.texture, nullptr) ==
            nullptr);
  }
  cl_int error = CL_SUCCESS;
  CHECK(sharing.from_texture(nullptr, CL_MEM_READ_WRITE, gl_texture_2d, 0, name, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK_EQ(sharing.mirrors(), 0u);
  rig.delete_texture(rgb);
  rig.delete_texture(name); }

/* A renderbuffer crosses through a texture of its storage: what GL cleared it
 * to is what OpenCL reads, what OpenCL writes is what GL reads out of the
 * renderbuffer, and the guest is told of the renderbuffer and never of the
 * texture, which goes with the mirror. */
TEST(copied_gl_sharing_device, a_renderbuffer_crosses_through_a_texture_of_its_storage)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const float         red[4] = {0.8f, 0.1f, 0.3f, 1.0f};
  const std::uint32_t renderbuffer = rig.renderbuffer(gl_rgba8, 16u, 16u, red);
  cl_int              error = CL_INVALID_VALUE;
  cl_mem const        mem = sharing.from_renderbuffer(rig.context(), CL_MEM_READ_WRITE, renderbuffer, &error);
  if (mem == nullptr) {
      std::fprintf(stderr, "this host makes no texture of a renderbuffer (%d)\n", error);
      rig.delete_renderbuffer(renderbuffer);
      return;
  }
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(sharing.acquire(rig.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  const std::size_t at[3] = {5u, 7u, 0u}, one[3] = {1u, 1u, 1u};
  std::uint8_t      pixel[4] = {};
  CHECK_EQ(::clEnqueueReadImage(rig.queue(), mem, CL_TRUE, at, one, 0u, 0u, pixel, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK(pixel[0] >= 203u && pixel[0] <= 205u);
  CHECK(pixel[1] >= 25u && pixel[1] <= 27u);
  CHECK(pixel[2] >= 76u && pixel[2] <= 78u);
  CHECK_EQ(pixel[3], 255u);
  const std::uint8_t green[4] = {10u, 230u, 40u, 255u};
  CHECK_EQ(::clEnqueueWriteImage(rig.queue(), mem, CL_TRUE, at, one, 0u, 0u, green, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(sharing.release(rig.queue(), 1u, &mem, 0u, nullptr, nullptr), CL_SUCCESS);
  static std::uint8_t pixels[16u * 16u * 4u];
  rig.read_renderbuffer(renderbuffer, 16u, 16u, pixels);
  CHECK(std::memcmp(&pixels[(7u * 16u + 5u) * 4u], green, sizeof green) == 0);
  CHECK(pixels[(7u * 16u + 6u) * 4u] >= 203u);

  cl_gl_object_type type = 0u;
  cl_GLuint         named = 0u;
  CHECK_EQ(sharing.object_info(mem, &type, &named), CL_SUCCESS);
  CHECK_EQ(type, static_cast<cl_gl_object_type>(CL_GL_OBJECT_RENDERBUFFER));
  CHECK_EQ(named, renderbuffer);
  cl_GLenum target = 0u;
  CHECK_EQ(sharing.texture_info(mem, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr), CL_INVALID_GL_OBJECT);
  sharing.forget(mem);
  CHECK_EQ(sharing.mirrors(), 0u);
  ::clReleaseMemObject(mem);

  /* Left held, it goes with everything else. */
  cl_mem const held = sharing.from_renderbuffer(rig.context(), CL_MEM_READ_ONLY, renderbuffer, nullptr);
  CHECK(held != nullptr);
  sharing.forget_all();
  CHECK_EQ(sharing.mirrors(), 0u);
  ::clReleaseMemObject(held);
  rig.delete_renderbuffer(renderbuffer); }

/* No renderbuffer by the name, one whose format OpenCL has no image for, and
 * flags that are not one access flag: nothing is made, and the texture made
 * on the way goes. */
TEST(copied_gl_sharing_device, a_renderbuffer_that_cannot_be_mirrored_is_refused)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing     sharing(vehicle.objects(), vehicle.textures());
  const float         grey[4] = {0.5f, 0.5f, 0.5f, 1.0f};
  const std::uint32_t rgba = rig.renderbuffer(gl_rgba8, 4u, 4u, grey);
  const std::uint32_t rgb = rig.renderbuffer(gl_rgb8, 4u, 4u, grey);
  cl_int              error = CL_SUCCESS;
  CHECK(sharing.from_renderbuffer(rig.context(), CL_MEM_READ_WRITE, rgba + 1000u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_GL_OBJECT);
  CHECK(sharing.from_renderbuffer(rig.context(), CL_MEM_READ_WRITE, rgba + 1000u, nullptr) == nullptr);
  CHECK(sharing.from_renderbuffer(rig.context(), CL_MEM_READ_WRITE, rgb, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
  CHECK(sharing.from_renderbuffer(rig.context(), 0u, rgba, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(sharing.mirrors(), 0u);
  rig.delete_renderbuffer(rgb);
  rig.delete_renderbuffer(rgba); }

/* The guest's context is its own: no context made here names a GL one. It is
 * made through GlSharing, as the backend makes it whichever way its GL objects
 * reach OpenCL. */
TEST(copied_gl_sharing_device, the_guest_s_context_is_made_as_it_asked)
{ HostGlRig rig;
  Vehicle   vehicle(rig);
  if (!rig.ready() || !vehicle.ready()) return;
  CopiedGlSharing             sharing(vehicle.objects(), vehicle.textures());
  cl_device_id const          device = static_cast<cl_device_id>(rig.device().device());
  const cl_context_properties properties[] = {
      CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(rig.device().platform()), 0};
  cl_int           error = CL_INVALID_VALUE;
  av::GlSharing   &chosen = sharing;
  cl_context const made = chosen.make_context(properties, 1u, &device, &error);
  CHECK(made != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  cl_context_properties named[8] = {};
  std::size_t           bytes = 0u;
  CHECK_EQ(::clGetContextInfo(made, CL_CONTEXT_PROPERTIES, sizeof named, named, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, sizeof properties);
  ::clReleaseContext(made); }

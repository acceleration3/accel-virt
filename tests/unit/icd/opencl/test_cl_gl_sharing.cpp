#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_object_table.hpp"
#include "icd/opencl/cl_wait_list.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"
#include "proto/context/sub_api.hpp"

#include <CL/cl_gl.h>

#include <cstdint>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClObject;

namespace {

constexpr std::uint64_t host_platform = 0x7200u;
constexpr std::uint64_t host_device = 0x7300u;
constexpr std::uint64_t host_context = 0x7400u;
constexpr std::uint64_t host_buffer = 0x7500u;
constexpr std::uint64_t host_queue = 0x7600u;
constexpr std::uint64_t host_event = 0x7700u;
constexpr std::uint32_t gl_share_name = 0x40000C42u;

class Found {
public:
    explicit Found(ClIcdRig &rig) : rig_(rig)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().arena_import(0x77u, 3u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS) device_ = nullptr;
    }

    cl_device_id device() const { return device_; }

    cl_context create(const cl_context_properties *properties, cl_int &error)
    {
        rig_.host().arena_import(0x78u, 3u);
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

}

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
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), av::Status::ok);
  CHECK(record.sub_api() == av::SubApi::opencl);
  CHECK_EQ(record.options().share_context(), gl_share_name);
  CHECK(record.options().shares_only());
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
  rig.host().cl_info(CL_SUCCESS, "cl_khr_icd cl_khr_gl_sharing", 29u);
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof named, &named, &size),
           CL_SUCCESS);
  CHECK(named == found.device());
  CHECK_EQ(size, sizeof(cl_device_id));
  named = nullptr;
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_DEVICES_FOR_GL_CONTEXT_KHR, sizeof named, &named, nullptr),
           CL_SUCCESS);
  CHECK(named == found.device());
  const cl_context_properties other[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(other_gl),
                                         CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
  CHECK_EQ(table->clGetGLContextInfoKHR(other, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof named, &named, nullptr),
           CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR);
  CHECK_EQ(table->clGetGLContextInfoKHR(current, 0x2005u, sizeof named, &named, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(table->clGetGLContextInfoKHR(current, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, 1u, &named, nullptr),
           CL_INVALID_VALUE); }


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

}

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

  const cl_context plain = found.create(nullptr, error);
  CHECK_EQ(error, CL_SUCCESS);
  if (plain == nullptr) return;
  CHECK(table->clCreateFromGLTexture(plain, CL_MEM_READ_ONLY, 0x0DE1u, 0, 9u, &made) == nullptr);
  CHECK_EQ(made, CL_INVALID_CONTEXT);
  CHECK_EQ(rig.gl_share().runs(), 3u); }

namespace av::clicd {
struct ClIcdRootProbe {
    static ClObject *without_a_context(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, nullptr, nullptr, made) == av::Status::ok ? made : nullptr; }
    static void exhaust_the_contexts(ClIcdRig &rig)
    { GuestClContext *made = nullptr;
      rig.platform().fail_allocation_at(1u);
      for (std::uint32_t count = 0u; count < 1024u; ++count)
          if (rig.root().contexts_.create(made, rig.root().devices_.at(0u).connection(), rig.root().memory_,
                                          rig.root().send_lock_, rig.root().blocks_lock_, rig.root().lines_,
                                          rig.root().shared_handles_) != av::Status::ok)
              break;
      rig.platform().fail_allocation_at(0u); }
    static GuestClContext *context(ClIcdRoot &root, cl_context handle)
    { ClObject *const held = root.judged(handle, ClObject::Kind::context);
      return held != nullptr ? root.context_of(*held) : nullptr; }
};
}

using av::clicd::ClIcdRootProbe;

namespace {

const cl_context_properties sharing_properties[] = {
    CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
    CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};

class Sharing {
public:
    explicit Sharing(ClIcdRig &rig) : rig_(rig), found_(rig)
    {
        rig_.gl_share().make_current(current_gl, gl_share_name);
        cl_int error = CL_INVALID_VALUE;
        context_ = found_.create(sharing_properties, error);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_queue, CL_SUCCESS);
        queue_ = rig_.loader().create_queue(context_, found_.device(), nullptr);
        rig_.host().cl_handle(host_buffer, CL_SUCCESS);
        buffer_ = rig_.root().create_from_gl_buffer(context_, CL_MEM_READ_WRITE, 7u, &error);
    }

    bool             made() const { return queue_ != nullptr && buffer_ != nullptr; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    cl_mem           buffer() const { return buffer_; }
    cl_device_id     device() const { return found_.device(); }

    void lose()
    {
        rig_.kmt().withhold_answer_at(1u);
        static_cast<void>(rig_.loader().create_user_event(context_, nullptr));
    }

    void fault_unnoticed()
    {
        av::ClientTransport &wire = *av::clicd::ClIcdRootProbe::context(rig_.root(), context_)->wire();
        rig_.kmt().withhold_answer_at(1u);
        static_cast<void>(av::clproto::av_clFlush(wire, 0u));
    }

private:
    ClIcdRig        &rig_;
    Found            found_;
    cl_context       context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_mem           buffer_ = nullptr;
};

}

TEST(cl_gl_sharing, acquiring_and_releasing_gl_objects_names_them_to_the_host_and_hands_back_its_event)
{ ClIcdRig rig;
  Sharing  sharing(rig);
  CHECK(sharing.made());
  if (!sharing.made()) return;
  const cl_mem objects[1] = {sharing.buffer()};
  cl_event     event = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), 1u, objects, 0u, nullptr, &event), CL_SUCCESS);
  CHECK(event != nullptr);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event released = nullptr;
  CHECK_EQ(rig.root().enqueue_release_gl(sharing.queue(), 1u, objects, 1u, &event, &released), CL_SUCCESS);
  CHECK(released != nullptr);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), 1u, objects, 0u, nullptr, nullptr), CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_release_gl(sharing.queue(), 0u, nullptr, 0u, nullptr, nullptr), CL_SUCCESS);
  rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  cl_event refused = nullptr;
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), 1u, objects, 0u, nullptr, &refused), CL_INVALID_OPERATION);
  CHECK(refused == nullptr);
  rig.host().cl_result(CL_INVALID_OPERATION);
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), 1u, objects, 0u, nullptr, nullptr), CL_INVALID_OPERATION);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_gl_sharing, an_acquire_or_release_the_api_would_refuse_sends_nothing)
{ ClIcdRig rig;
  Sharing  sharing(rig);
  CHECK(sharing.made());
  if (!sharing.made()) return;
  ClObject *const bare_queue = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::command_queue, 0x7601u);
  ClObject *const bare_memory = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::mem, 0x7501u);
  CHECK(bare_queue != nullptr && bare_memory != nullptr);
  if (bare_queue == nullptr || bare_memory == nullptr) return;
  const cl_mem objects[1] = {sharing.buffer()};
  const cl_mem foreign[1] = {reinterpret_cast<cl_mem>(bare_memory)};
  const cl_mem unknown[1] = {reinterpret_cast<cl_mem>(sharing.queue())};
  int          not_an_event = 0;
  const cl_event stray = reinterpret_cast<cl_event>(&not_an_event);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_acquire_gl(reinterpret_cast<cl_command_queue>(sharing.buffer()), 1u, objects, 0u,
                                         nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_acquire_gl(reinterpret_cast<cl_command_queue>(bare_queue), 1u, objects, 0u, nullptr,
                                         nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), 1u, nullptr, 0u, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), 1u, unknown, 0u, nullptr, nullptr), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_release_gl(sharing.queue(), 1u, foreign, 0u, nullptr, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_release_gl(sharing.queue(), 1u, objects, 1u, &stray, nullptr),
           CL_INVALID_EVENT_WAIT_LIST);
  cl_mem many[av::clicd::ClHostNumbers::kept_inline + 1u] = {};
  for (cl_mem &one : many) one = sharing.buffer();
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), av::clicd::ClHostNumbers::kept_inline + 1u, many, 0u,
                                         nullptr, nullptr),
           CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_gl_sharing, a_lost_gl_context_refuses_its_objects_acquires_and_texture_queries)
{ ClIcdRig rig;
  Sharing  sharing(rig);
  CHECK(sharing.made());
  if (!sharing.made()) return;
  sharing.fault_unnoticed();
  const std::uint32_t sent = rig.kmt().commands();
  const cl_mem        objects[1] = {sharing.buffer()};
  cl_int              error = CL_SUCCESS;
  CHECK(rig.root().create_from_gl_buffer(sharing.context(), CL_MEM_READ_WRITE, 7u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_acquire_gl(sharing.queue(), 1u, objects, 0u, nullptr, nullptr), CL_OUT_OF_RESOURCES);
  std::size_t wide = 0u;
  CHECK_EQ(rig.root().gl_texture_info(sharing.buffer(), CL_GL_TEXTURE_TARGET, sizeof wide, &wide, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_gl_sharing, a_textures_info_is_the_hosts_and_an_object_that_is_no_memory_has_none)
{ ClIcdRig rig;
  Sharing  sharing(rig);
  CHECK(sharing.made());
  if (!sharing.made()) return;
  std::uint32_t target = 0u;
  const std::uint32_t texture_2d = 0x0DE1u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&texture_2d), sizeof texture_2d);
  CHECK_EQ(rig.root().gl_texture_info(sharing.buffer(), CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr),
           CL_SUCCESS);
  CHECK_EQ(target, texture_2d);
  CHECK_EQ(rig.root().gl_texture_info(reinterpret_cast<cl_mem>(sharing.queue()), CL_GL_TEXTURE_TARGET, sizeof target,
                                      &target, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(cl_gl_sharing, the_gl_context_query_judges_the_platform_the_share_group_and_the_device_it_must_open)
{ { ClIcdRig rig;
    Found     found(rig);
    rig.gl_share().make_current(current_gl, gl_share_name);
    const cl_platform_id platform = rig.loader().platform(0u);
    const cl_context_properties named[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platform),
                                           CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                           CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
    int stranger = 0;
    const cl_context_properties alien[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(&stranger),
                                           CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(current_gl),
                                           CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(window_dc), 0};
    const cl_context_properties nameless[] = {CL_CONTEXT_PLATFORM, 0, CL_GL_CONTEXT_KHR,
                                              reinterpret_cast<cl_context_properties>(current_gl), CL_WGL_HDC_KHR,
                                              reinterpret_cast<cl_context_properties>(window_dc), 0};
    const cl_context_properties no_gl[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platform), 0};
    const cl_context_properties zero_gl[] = {CL_GL_CONTEXT_KHR, 0, CL_WGL_HDC_KHR,
                                             reinterpret_cast<cl_context_properties>(window_dc), 0};
    cl_device_id device = nullptr;
    std::size_t  size = 0u;
    rig.host().cl_info(CL_SUCCESS, "cl_khr_icd cl_khr_gl_sharing", 29u);
    CHECK_EQ(rig.root().gl_context_info(named, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device, &device, &size),
             CL_SUCCESS);
    CHECK(device == found.device());
    CHECK_EQ(rig.root().gl_context_info(alien, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device, &device, &size),
             CL_INVALID_PLATFORM);
    CHECK_EQ(rig.root().gl_context_info(nameless, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device, &device, &size),
             CL_SUCCESS);
    CHECK_EQ(rig.root().gl_context_info(no_gl, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device, &device, &size),
             CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR);
    CHECK_EQ(rig.root().gl_context_info(zero_gl, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device, &device, &size),
             CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR);
    size = 0u;
    CHECK_EQ(rig.root().gl_context_info(named, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, 0u, nullptr, &size), CL_SUCCESS);
    CHECK_EQ(size, sizeof(cl_device_id));
    CHECK_EQ(rig.kmt().queued_answers(), 0u); }
  { ClIcdRig rig;
    rig.gl_share().make_current(current_gl, gl_share_name);
    rig.kmt().fail_at(av::MockKmt::Call::enumerate_adapters, 1u, 0xC0000001u);
    cl_device_id device = nullptr;
    CHECK_EQ(rig.root().gl_context_info(sharing_properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device,
                                        &device, nullptr),
             CL_OUT_OF_RESOURCES);
    CHECK(device == nullptr); }
  { ClIcdRig rig;
    rig.gl_share().make_current(current_gl, gl_share_name);
    rig.kmt().fail_at(av::MockKmt::Call::create_device, 1u, 0xC0000001u);
    cl_device_id device = nullptr;
    CHECK_EQ(rig.root().gl_context_info(sharing_properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device,
                                        &device, nullptr),
             CL_OUT_OF_RESOURCES);
    CHECK(device == nullptr); } }

TEST(cl_gl_sharing, a_context_that_cannot_be_made_names_the_share_group_when_it_asked_for_one_and_memory_when_it_lacked_it)
{ bool memory = false;
  bool share_group = false;
  bool device = false;
  for (std::uint32_t nth = 1u; nth <= 24u; ++nth) {
      ClIcdRig rig;
      Found    found(rig);
      rig.gl_share().make_current(current_gl, gl_share_name);
      rig.host().arena_import(0x78u, 3u);
      rig.platform().fail_allocation_at(nth);
      cl_int error = CL_SUCCESS;
      cl_device_id named = found.device();
      const cl_context context = rig.loader().dispatch_of(found.device())->clCreateContext(
          sharing_properties, 1u, &named, nullptr, nullptr, &error);
      if (context != nullptr) continue;
      memory = memory || error == CL_OUT_OF_HOST_MEMORY;
      CHECK(rig.root().invariants_hold());
  }
  for (unsigned call = 0u; call < static_cast<unsigned>(av::MockKmt::Call::count); ++call) {
      for (std::uint32_t nth = 1u; nth <= 3u; ++nth) {
          for (int shared = 0; shared < 2; ++shared) {
              ClIcdRig rig;
              Found    found(rig);
              rig.gl_share().make_current(current_gl, gl_share_name);
              rig.host().arena_import(0x78u, 3u);
              rig.kmt().fail_at(static_cast<av::MockKmt::Call>(call), nth, 0xC0000001u);
              cl_int error = CL_SUCCESS;
              cl_device_id named = found.device();
              const cl_context context = rig.loader().dispatch_of(found.device())->clCreateContext(
                  shared != 0 ? sharing_properties : nullptr, 1u, &named, nullptr, nullptr, &error);
              if (context != nullptr) continue;
              share_group = share_group || (shared != 0 && error == CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR);
              device = device || (shared == 0 && error == CL_DEVICE_NOT_AVAILABLE);
              CHECK(rig.root().invariants_hold());
          }
      }
  }
  CHECK(memory);
  CHECK(share_group);
  CHECK(device); }

TEST(cl_gl_sharing, a_context_the_root_has_no_room_for_is_out_of_host_memory_and_makes_nothing)
{ ClIcdRig rig;
  Found    found(rig);
  av::clicd::ClIcdRootProbe::exhaust_the_contexts(rig);
  rig.host().arena_import(0x79u, 3u);
  rig.platform().fail_allocation_at(1u);
  cl_device_id named = found.device();
  cl_int       error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &named, 1u, &error, av::clicd::ClContextNotify()) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold()); }

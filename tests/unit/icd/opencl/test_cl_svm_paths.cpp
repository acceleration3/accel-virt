#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"

#include <CL/cl_ext.h>
#include <cstdint>

using av::ClIcdRig;
using av::FakeClLoader;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_shared = 0x0000'7F55'0000'1000ull;
constexpr std::size_t   shared_bytes = 256u;

class Opened {
public:
    explicit Opened(ClIcdRig &rig) : rig_(rig), device_(nullptr), context_(nullptr), queue_(nullptr)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().arena_import(0x77u, 3u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS)
            return;
        rig_.host().arena_import(0x78u, 3u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        context_ = rig_.loader().create_context(device_, nullptr);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_queue, CL_SUCCESS);
        queue_ = rig_.loader().create_queue(context_, device_, nullptr);
    }

    bool             made() const { return queue_ != nullptr; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }

private:
    ClIcdRig        &rig_;
    cl_device_id     device_;
    cl_context       context_;
    cl_command_queue queue_;
};

}

TEST(cl_svm_paths, an_allocation_of_nothing_or_on_nothing_is_not_made)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().svm_alloc(reinterpret_cast<cl_context>(opened.queue()), CL_MEM_READ_WRITE,
                             shared_bytes, 0u) == nullptr);
  CHECK(rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, 0u, 0u) == nullptr);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, a_host_that_allocates_nothing_hands_the_application_nothing)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u) == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, an_allocation_with_no_mirror_is_given_back_to_the_host)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  rig.platform().fail_allocation_at(1u);
  CHECK(rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u) == nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, freeing_what_no_allocation_answered_sends_nothing)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  rig.root().svm_free(reinterpret_cast<cl_context>(opened.queue()), mirror);
  rig.root().svm_free(opened.context(), nullptr);
  int invented = 0;
  rig.root().svm_free(opened.context(), &invented);
  rig.root().svm_free(opened.context(), static_cast<std::uint8_t *>(mirror) + 8u);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_result(CL_SUCCESS);
  rig.root().svm_free(opened.context(), mirror);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  rig.root().svm_free(opened.context(), mirror);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, a_map_or_unmap_of_an_address_in_no_allocation_is_refused_in_the_guest)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  int invented = 0;
  CHECK_EQ(rig.root().svm_map(reinterpret_cast<cl_command_queue>(opened.context()), CL_TRUE,
                              CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().svm_unmap(reinterpret_cast<cl_command_queue>(opened.context()), mirror,
                                nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, &invented, shared_bytes,
                              nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, 0u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), &invented, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, a_map_the_host_refuses_leaves_the_allocation_unmapped)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;

  rig.host().cl_result(CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_OUT_OF_RESOURCES);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, an_unmap_the_host_refuses_leaves_the_map_depth_where_it_was)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;

  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.log().holding("shared memory is mapped"), 1u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.log().holding("shared memory is mapped"), 1u);

  rig.host().cl_result(CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, nullptr), CL_OUT_OF_RESOURCES);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, nullptr), CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, a_kernel_argument_that_is_an_address_is_translated_or_refused)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(0x6700u, CL_SUCCESS);
  cl_program program = rig.loader().create_program(opened.context(), "__kernel void k(void){}",
                                                   nullptr);
  CHECK(program != nullptr);
  if (program == nullptr) return;
  rig.host().cl_handle(0x6800u, CL_SUCCESS);
  cl_kernel kernel = rig.loader().create_kernel(program, "k", nullptr);
  CHECK(kernel != nullptr);
  if (kernel == nullptr) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  int                 invented = 0;
  CHECK_EQ(rig.root().set_kernel_svm_argument(reinterpret_cast<cl_kernel>(opened.queue()), 0u,
                                              mirror),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, &invented), CL_INVALID_ARG_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, static_cast<std::uint8_t *>(mirror) + 8u),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, every_shared_memory_entry_on_a_lost_context_refuses_at_once)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;
  rig.host().cl_handle(0x6700u, CL_SUCCESS);
  cl_program program = rig.loader().create_program(opened.context(), "__kernel void k(void){}",
                                                   nullptr);
  CHECK(program != nullptr);
  if (program == nullptr) return;
  rig.host().cl_handle(0x6800u, CL_SUCCESS);
  cl_kernel kernel = rig.loader().create_kernel(program, "k", nullptr);
  CHECK(kernel != nullptr);
  if (kernel == nullptr) return;

  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_OUT_OF_RESOURCES);

  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, mirror), CL_OUT_OF_RESOURCES);
  rig.root().svm_free(opened.context(), mirror);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u) == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, a_map_and_an_unmap_asked_for_an_event_hand_one_back)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;

  rig.host().cl_enqueued(CL_SUCCESS, 0x6900u);
  cl_event mapped = nullptr;
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, &mapped),
           CL_SUCCESS);
  CHECK(mapped != nullptr);
  rig.host().cl_enqueued(CL_SUCCESS, 0x6901u);
  cl_event unmapped = nullptr;
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, &unmapped), CL_SUCCESS);
  CHECK(unmapped != nullptr);
  CHECK(rig.root().invariants_hold()); }

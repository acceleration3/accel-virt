/* Shared virtual memory, and what each of its entries answers when it cannot.
 *
 * An application is handed a mirror, never the host's own address: a kernel
 * may only dereference what the host's clSVMAlloc produced, and the guest
 * cannot touch that memory. So every entry here translates before it encodes,
 * and an address inside no live allocation is refused in the guest without a
 * command being sent - which is the property that matters, because the wire
 * carries host addresses and nothing downstream re-checks them.
 *
 * Coarse-grained is all that is reported, and the ICD says once per allocation
 * that the mirror is not synchronised between map and unmap: no command on
 * this wire carries bytes between guest memory and a host SVM address. */
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
/* An address far from anything this process owns, so a bug that dereferenced
 * it would crash rather than pass. */
constexpr std::uint64_t host_shared = 0x0000'7F55'0000'1000ull;
constexpr std::size_t   shared_bytes = 256u;

class Opened {
public:
    explicit Opened(ClIcdRig &rig) : rig_(rig), device_(nullptr), context_(nullptr), queue_(nullptr)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().blob_import(0u, 0x77u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS)
            return;
        rig_.host().blob_import(0u, 0x78u);
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

}  // namespace

TEST(cl_svm_paths, an_allocation_of_nothing_or_on_nothing_is_not_made)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  const std::uint32_t sent = rig.kmt().commands();
  /* Not a context, and a size of nothing: neither reaches the host. */
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
  /* The host's allocator answered no address, so there is no pair to make. */
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u) == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_svm_paths, an_allocation_with_no_mirror_is_given_back_to_the_host)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  /* The host allocated and the guest has no memory for the mirror. The
   * application is handed nothing, so the host's allocation has to go back:
   * an address nobody was given is one nobody will ever free. */
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  rig.platform().fail_allocation_at(1u);
  CHECK(rig.root().svm_alloc(opened.context(), CL_MEM_READ_WRITE, shared_bytes, 0u) == nullptr);
  /* Two commands: the allocation, then the free that undoes it. */
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
  /* Not a context, no pointer at all, an address the application invented,
   * and an address *inside* the allocation rather than the base it was
   * handed: clSVMFree takes the base, and an address inside is not what the
   * host's allocator answered. */
  rig.root().svm_free(reinterpret_cast<cl_context>(opened.queue()), mirror);
  rig.root().svm_free(opened.context(), nullptr);
  int invented = 0;
  rig.root().svm_free(opened.context(), &invented);
  rig.root().svm_free(opened.context(), static_cast<std::uint8_t *>(mirror) + 8u);
  CHECK_EQ(rig.kmt().commands(), sent);

  /* The base does free it, once. */
  rig.host().cl_result(CL_SUCCESS);
  rig.root().svm_free(opened.context(), mirror);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  /* And freeing it twice frees nothing more. */
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
  /* Not a queue. */
  CHECK_EQ(rig.root().svm_map(reinterpret_cast<cl_command_queue>(opened.context()), CL_TRUE,
                              CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().svm_unmap(reinterpret_cast<cl_command_queue>(opened.context()), mirror,
                                nullptr),
           CL_INVALID_COMMAND_QUEUE);
  /* An address this ICD never answered, and a map of no bytes. */
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, &invented, shared_bytes,
                              nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, 0u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), &invented, nullptr), CL_INVALID_VALUE);
  /* Nothing crossed: the wire carries host addresses, and nothing downstream
   * would check one this side invented. */
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
  /* Not mapped, so an unmap now has nothing to undo - and the host is still
   * asked, because it is the host's map depth that the unmap names. */
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
  /* The limitation is said once, when the first map happens. */
  CHECK_EQ(rig.log().holding("shared memory is mapped"), 1u);
  /* A nested map says nothing more: the application is told once per
   * allocation, not once per call. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.log().holding("shared memory is mapped"), 1u);

  /* An unmap the host refuses changes nothing this side: the depth is the
   * host's, and pretending it dropped would free memory still mapped. */
  rig.host().cl_result(CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, nullptr), CL_OUT_OF_RESOURCES);
  /* Both nested maps still have to be undone. */
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
  /* Not a kernel, and an address inside no live allocation: refused here,
   * because the wire would carry it as a host address and the device would
   * have nothing to judge it against. */
  CHECK_EQ(rig.root().set_kernel_svm_argument(reinterpret_cast<cl_kernel>(opened.queue()), 0u,
                                              mirror),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, &invented), CL_INVALID_ARG_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  /* An address inside the allocation is a legal argument, and it translates
   * to the same offset of the host's. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, static_cast<std::uint8_t *>(mirror) + 8u),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

/* Shared memory on a context whose transport is gone.
 *
 * These entries reach the wire only once they have translated an address, so
 * a lost context alone does not get them there - the allocation has to exist
 * first and the context to die after it. That is the order a real application
 * meets: it allocates, the device faults, and it carries on calling. */
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

  /* The fault. */
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_OUT_OF_RESOURCES);

  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().svm_map(opened.queue(), CL_TRUE, CL_MAP_READ, mirror, shared_bytes, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().svm_unmap(opened.queue(), mirror, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, mirror), CL_OUT_OF_RESOURCES);
  /* Freeing sends nothing either, and the allocation is still forgotten:
   * the device frees it with the context. */
  rig.root().svm_free(opened.context(), mirror);
  CHECK_EQ(rig.kmt().commands(), sent);
  /* And nothing more can be allocated on it. */
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

  /* Both of these enqueue, so both can be asked for the event they made. */
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

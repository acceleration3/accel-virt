/* Which of the two paths a buffer takes, and that the log says which.
 *
 * Where the capset offers host-visible memory the context opens a blob arena,
 * the device makes a mappable buffer's cl_mem over that very memory with
 * CL_MEM_USE_HOST_PTR, and a map hands the application the arena's own view -
 * no copy in, none out. Where it does not, the buffer is the host's own and a
 * map stages a block, fills it and writes it back.
 *
 * Both are correct; only one is fast. What must never happen is the ICD
 * believing it took the first while taking the second, so the commands it
 * sends are what these tests read. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
/* What the host's own map answers. Not an address the guest ever reads
 * through - the unmap names it and nothing else does. */
constexpr std::uint64_t host_mapped = 0x7F0000008000ull;

constexpr std::size_t buffer_bytes = 256u;

/* `arena` says whether the device gives the context one: a blob import that
 * fails is a device with no host-visible memory to lend, which is the staged
 * path's precondition. */
void device_opens(ClIcdRig &rig, bool arena)
{
    rig.host().blob_import(arena ? 0u : 1u, arena ? 0x77u : 0u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
}

/* A context and a queue on a device opened with or without an arena. */
class Opened {
public:
    Opened(ClIcdRig &rig, bool arena) : rig_(rig), context_(nullptr), queue_(nullptr)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        device_opens(rig_, arena);
        cl_device_id device = nullptr;
        cl_uint      devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) != CL_SUCCESS)
            return;
        rig_.host().blob_import(arena ? 0u : 1u, arena ? 0x78u : 0u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        context_ = rig_.loader().create_context(device, nullptr);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_queue, CL_SUCCESS);
        queue_ = rig_.loader().create_queue(context_, device, nullptr);
    }

    bool             made() const { return context_ != nullptr && queue_ != nullptr; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }

private:
    ClIcdRig        &rig_;
    cl_context       context_;
    cl_command_queue queue_;
};

}  // namespace

TEST(cl_buffer_paths, an_arena_backed_buffer_is_made_over_the_arena_and_the_log_says_so)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  /* Each context says which path its buffers take, once, when it opens -
   * and two have: the one the ICD opens to answer for the device, and the
   * application's own. */
  CHECK_EQ(rig.log().holding("buffers map through the blob arena"), 2u);
  CHECK_EQ(rig.log().holding("buffers are staged"), 0u);

  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                             buffer_bytes, &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  /* With properties, because the offset of the arena is one: the plain
   * clCreateBuffer has nowhere to carry it. */
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBufferWithProperties));
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_map_of_an_arena_backed_buffer_reads_nothing_back)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                             buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  /* The host is still asked to map - its map is what says a kernel is no
   * longer running over the bytes - and it answers an address. */
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  /* One command, and it is the map: a staged map would have followed it with
   * a blocking read to fill the block. Nothing is read because there is
   * nothing to read - the bytes are already here. */
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMapBuffer));

  /* And the unmap writes nothing back, for the same reason. */
  const std::uint32_t before_unmap = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before_unmap + 1u);
  CHECK_EQ(rig.kmt().command_id(before_unmap),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, without_an_arena_a_buffer_is_the_hosts_own_and_a_map_stages)
{ ClIcdRig rig;
  Opened opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  /* The other half of the log, from both contexts: a device with no
   * host-visible memory says so, and every buffer is staged. */
  CHECK_EQ(rig.log().holding("buffers are staged"), 2u);
  CHECK_EQ(rig.log().holding("buffers map through the blob arena"), 0u);

  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                             buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  /* The plain call: there is no offset to name. */
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  const std::uint32_t mapping = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  /* Two: the host's map, then the read that fills the staged block. That
   * second command is the copy the arena path does not make. */
  CHECK_EQ(rig.kmt().commands(), mapping + 2u);
  CHECK_EQ(rig.kmt().command_id(mapping),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMapBuffer));
  CHECK_EQ(rig.kmt().command_id(mapping + 1u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueReadBuffer));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_buffer_the_application_brought_memory_for_is_never_arena_backed)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  /* CL_MEM_COPY_HOST_PTR names the application's own memory, which is not
   * the arena's; backing it there would be answering a different call. */
  cl_mem buffer = rig.loader().create_buffer(
      opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR,
      buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_buffer_that_did_not_ask_for_host_memory_is_not_given_the_arena)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  /* The arena is finite, and a buffer a kernel reads hardest belongs in the
   * device's own memory. Only CL_MEM_ALLOC_HOST_PTR asks for this. */
  cl_mem buffer =
      rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));
  CHECK(rig.root().invariants_hold()); }

/* ---- what a map and an unmap answer when they cannot ---------------------- */

namespace {

/* A buffer on an opened context, by whichever path its flags ask for. */
cl_mem buffer_on(ClIcdRig &rig, const Opened &opened, cl_mem_flags flags,
                 std::uint64_t handle = host_buffer)
{
    rig.host().cl_handle(handle, CL_SUCCESS);
    return rig.loader().create_buffer(opened.context(), flags, buffer_bytes, nullptr);
}

}  // namespace

TEST(cl_buffer_paths, a_map_of_something_that_is_not_a_queue_or_a_buffer_is_refused)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  /* The context is not a queue, and the queue is not a buffer. Each is told
   * which object it should have been. */
  CHECK(rig.root().map_buffer(reinterpret_cast<cl_command_queue>(opened.context()), buffer, CL_TRUE,
                              CL_MAP_READ, 0u, buffer_bytes, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_COMMAND_QUEUE);
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), reinterpret_cast<cl_mem>(opened.queue()), CL_TRUE,
                              CL_MAP_READ, 0u, buffer_bytes, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_MEM_OBJECT);
  /* A map of nothing is not a map. */
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, 0u, nullptr,
                              &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  /* None of them reached the host. */
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_host_that_will_not_map_gives_its_error_back_and_stages_nothing)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);

  /* And a host that answers success with no address: it contradicted itself,
   * which the API has no word for, so the map simply failed. */
  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_MAP_FAILURE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_staged_map_whose_fill_fails_keeps_no_mapping)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  /* The host maps, then refuses the read that fills the staged block. The
   * application is handed nothing, so the block it would have held has to go
   * back - otherwise the context keeps a mapping nobody can unmap. */
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold());

  /* The slot came back, so the next map can have it. */
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u,
                                             buffer_bytes, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_write_invalidate_map_reads_nothing_because_it_promises_nothing)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  /* CL_MAP_WRITE_INVALIDATE_REGION says the application will not read what
   * was there, so filling the block would be a copy nobody asked for. */
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE,
                                             CL_MAP_WRITE_INVALIDATE_REGION, 0u, buffer_bytes,
                                             nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  /* One command, the host's own map, and no read behind it. */
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, an_unmap_names_the_pointer_the_map_answered_and_the_buffer_it_was_of)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem first = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  /* A different host handle, because the unmap tells them apart by it. */
  cl_mem second = buffer_on(rig, opened, CL_MEM_READ_WRITE, host_buffer + 1u);
  CHECK(first != nullptr && second != nullptr);
  if (first == nullptr || second == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  void *const mapped = rig.root().map_buffer(opened.queue(), first, CL_TRUE, CL_MAP_READ, 0u,
                                             buffer_bytes, nullptr, nullptr);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  /* Not a queue, not a buffer, the wrong buffer, and an address inside the
   * mapping rather than the one it answered: each refused before anything
   * is sent, because an unmap that guessed would unmap something else. */
  CHECK_EQ(rig.root().unmap(reinterpret_cast<cl_command_queue>(opened.context()), first, mapped,
                            nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().unmap(opened.queue(), reinterpret_cast<cl_mem>(opened.queue()), mapped,
                            nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().unmap(opened.queue(), second, mapped, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().unmap(opened.queue(), first, static_cast<std::uint8_t *>(mapped) + 1u,
                            nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  /* The mapping survived being asked about wrongly. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().unmap(opened.queue(), first, mapped, nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, an_unmap_the_host_refuses_writes_nothing_back)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, nullptr);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;

  /* The host refuses the unmap. The write-back must not follow it: the
   * mapping is still the host's, and writing over the buffer now would be
   * writing under a mapping that is still live. */
  rig.host().cl_result(CL_OUT_OF_RESOURCES);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_write_back_the_host_refuses_is_what_the_unmap_answers)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, nullptr);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;

  /* The unmap succeeds and the write-back behind it does not. The
   * application wrote through that mapping, so a failure to put the bytes
   * where it asked is the unmap's failure and not something to swallow. */
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_OUT_OF_RESOURCES);
  /* The mapping is gone either way: it is the host's that was ended. */
  CHECK(rig.root().invariants_hold()); }

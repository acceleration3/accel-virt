/* The cl_mem objects one OpenCL context imported from a Win32 handle
 * (feature 053, US2, T038).
 *
 * cl_khr_external_memory_win32 is the only way an OpenCL application here ever
 * names memory another API made, and the handle it names it by is a value only
 * the guest kernel resolves - nothing of it crosses the wire. So what every
 * test asks is the same: does a handle this process may open reach the object
 * behind it, and does everything else - a handle from nowhere, a handle type
 * this path does not take, a device that will not open it - refuse without
 * leaving a row or a reference behind.
 */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opencl/cl_external_memory.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

namespace av::clicd {

struct ClExternalMemoryProbe {
    static ClImportedMemoryRow &row(ClExternalMemory &memory, std::uint32_t index)
    { return memory.rows_[index]; }
    static void set_live(ClExternalMemory &memory, std::uint32_t live) { memory.live_ = live; }
};

}  // namespace av::clicd

namespace {

using av::MockKmt;
using av::MockTransport;
using av::SharedObjectAnswer;
using av::SharedObjectKind;
using av::Status;
using av::clicd::ClExternalMemory;
using av::clicd::ClExternalMemoryProbe;
using av::clicd::ClPropertyList;

constexpr std::uint64_t object_bytes = 4u * 4096u;
/* The cl_mem the device answers, as a wire handle. */
constexpr std::uint64_t host_buffer = 0x5150u;

/* The device's 16-byte answer, as the transport hands it back. */
class Answer {
public:
    Answer(std::uint32_t status, std::uint64_t made) : bytes_{}
    {
        std::memcpy(bytes_, &status, sizeof status);
        std::memcpy(bytes_ + 8u, &made, sizeof made);
    }
    const std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t bytes_[SharedObjectAnswer::encoded_size];
};

/* One process: a kernel that mints identities and NT handles, the ICD's handle
 * table over it, and a transport playing the device. */
class Process {
public:
    Process() : trace_(), lock_(trace_, 'S'), kmt_(), log_(), wire_(), device_(0u),
                handles_(kmt_, lock_), memory_(handles_, log_)
    {
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapter = 0u;
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(kmt_.create_device(adapter, device_), Status::ok);
    }

    ClExternalMemory &memory() { return memory_; }
    MockTransport    &wire() { return wire_; }
    av::RecordingLog &log() { return log_; }
    std::uint32_t     device() const { return device_; }

    /* A handle this process may open: an allocation the kernel minted an
     * identity into, shared, and adopted into the ICD's table. */
    std::uint64_t handle()
    {
        const av::AllocationDescriptor descriptor(av::AllocationKind::client, true, av::SurfaceLayout(),
                                                  4096u, av::ScanoutTarget());
        std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
        CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(SharedObjectKind::memory), 0u,
                                            object_bytes);
        CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        std::uint32_t allocation = 0u;
        CHECK_STATUS(kmt_.create_shared_allocation(device_, bytes, sizeof bytes, allocation), Status::ok);
        std::uint64_t named = 0u;
        CHECK_STATUS(kmt_.share_object(device_, allocation, named), Status::ok);
        carrier_ = allocation;
        return named;
    }

    /* Another NT handle over the last carrier: a second name for one object,
     * which is what an application sharing the same surface twice ends up
     * with. */
    std::uint64_t share_again()
    {
        std::uint64_t named = 0u;
        CHECK_STATUS(kmt_.share_object(device_, carrier_, named), Status::ok);
        return named;
    }

private:
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    MockKmt                 kmt_;
    av::RecordingLog        log_;
    MockTransport           wire_;
    std::uint32_t           device_;
    av::icd::SharedHandles  handles_;
    ClExternalMemory        memory_;
    std::uint32_t           carrier_ = 0u;
};

/* A property list of one pair as the application wrote it, read the way the
 * ICD reads one. An empty list is the default. */
class Properties {
public:
    Properties() : pairs_{}, list_() { CHECK(list_.read(pairs_)); }
    Properties(std::uint64_t name, std::uint64_t value) : pairs_{name, value, 0u, 0u}, list_()
    { CHECK(list_.read(pairs_)); }
    Properties(std::uint64_t name, std::uint64_t value, std::uint64_t second, std::uint64_t second_value)
        : pairs_{name, value, second, second_value}, list_()
    { CHECK(list_.read(pairs_)); }
    Properties(const Properties &) = delete;
    Properties &operator=(const Properties &) = delete;

    const ClPropertyList &list() const { return list_; }

private:
    std::uint64_t  pairs_[6];
    ClPropertyList list_;
};

}  // namespace

TEST(cl_external_memory, a_new_table_holds_nothing_and_asks_for_nothing)
{ Process          process;
  const Properties empty;
  CHECK(!process.memory().asked_for(empty.list()));
  CHECK_EQ(process.memory().live(), 0u);
  CHECK(process.memory().invariants_hold()); }

/* The property this ICD answers, and the four it does not. All five are an
 * external create: one that names a handle type this path cannot turn into a
 * host object is refused, never answered with ordinary memory - an application
 * that got a blank buffer where it asked for somebody else's image would read
 * zeroes and never know why. */
TEST(cl_external_memory, every_external_handle_property_is_an_external_create)
{ Process process;
  const std::uint64_t external[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_NAME_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR};
  for (const std::uint64_t name : external) {
      const Properties properties(name, 7u);
      CHECK(process.memory().asked_for(properties.list()));
  }
  /* And a list that names none of them is an ordinary buffer. */
  const Properties other(CL_MEM_DEVICE_HANDLE_LIST_KHR, 0u);
  CHECK(!process.memory().asked_for(other.list()));
  const Properties none;
  CHECK(!process.memory().asked_for(none.list())); }

TEST(cl_external_memory, a_handle_this_process_opened_reaches_the_object_behind_it)
{ Process             process;
  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const Answer        answered(0u, host_buffer);
  process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t made = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                 made),
           CL_SUCCESS);
  CHECK_EQ(made, host_buffer);
  CHECK_EQ(process.memory().live(), 1u);
  CHECK(process.memory().invariants_hold());
  /* What went on the wire is the object, not the handle: an NT handle means
   * nothing to the device. */
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::open_shared_object_id);
  std::uint64_t object_id = 0u;
  std::memcpy(&object_id, process.wire().last_payload(), sizeof object_id);
  CHECK(object_id != 0u);
  CHECK(object_id != named);

  /* It says what it holds, so a state dump names the object and the cl_mem
   * over it rather than a count nobody can check. */
  char            dump[512] = {};
  av::StateWriter writer(dump, sizeof dump);
  writer.open("context", 0u);
  process.memory().report(writer);
  writer.close();
  CHECK(std::strstr(writer.dump(), "external_memory/0 live=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "size=16384") != nullptr);

  /* Given back, the device is told and the row goes. */
  const Answer released(0u, 0u);
  process.wire().set_reply(released.bytes(), SharedObjectAnswer::encoded_size);
  CHECK(process.memory().give_back(process.wire(), host_buffer));
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(process.memory().live(), 0u);
  CHECK(process.memory().invariants_hold());
  /* And a cl_mem of 0 names nothing, as an ordinary buffer's does not. */
  CHECK(!process.memory().give_back(process.wire(), 0u));

  /* The handle is still this process's, so the same object opened again
   * resolves rather than asking the kernel a second time. */
  const Answer again(0u, host_buffer + 1u);
  process.wire().set_reply(again.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t second = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                 second),
           CL_SUCCESS);
  CHECK_EQ(second, host_buffer + 1u);
  CHECK_EQ(process.log().holding("rvkcl: a Win32 handle this process may not open"), 0u);

  /* And what the acquire and release of cl_khr_external_memory are judged by:
   * the cl_mem this table answered is one of its own, and nothing else is -
   * not an ordinary buffer's, and not the object id behind the row, which is
   * a different number entirely. */
  CHECK(process.memory().answered(host_buffer + 1u));
  CHECK(!process.memory().answered(host_buffer));
  CHECK(!process.memory().answered(0u));
  CHECK(!process.memory().answered(object_id)); }

/* A size that is not the object's is the device's to refuse, but a size of 0
 * never reaches it: the wire body has no shape for a memory object of no
 * bytes, so it would be refused as malformed and tell the application nothing. */
TEST(cl_external_memory, each_way_an_open_is_refused_leaves_no_row)
{ { /* A handle this process neither minted nor opened. */
    Process          process;
    const Properties properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x4004u);
    std::uint64_t    made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_INVALID_VALUE);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u);
    CHECK_EQ(process.wire().sends(), 0u);
    CHECK_EQ(process.log().holding("rvkcl: a Win32 handle this process may not open"), 1u); }

  { /* A handle of 0, which is no handle at all. */
    Process          process;
    const Properties properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0u);
    std::uint64_t    made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_INVALID_VALUE);
    CHECK_EQ(process.wire().sends(), 0u); }

  { /* Each handle type this path cannot turn into a host object. */
    Process             process;
    const std::uint64_t named = process.handle();
    const std::uint64_t refused[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR,
                                     CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_NAME_KHR,
                                     CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR,
                                     CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR};
    for (const std::uint64_t name : refused) {
        const Properties properties(name, named);
        std::uint64_t    made = 9u;
        CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(),
                                       object_bytes, made),
                 CL_INVALID_PROPERTY);
        CHECK_EQ(made, 0u);
    }
    CHECK_EQ(process.memory().live(), 0u);
    CHECK_EQ(process.wire().sends(), 0u); }

  { /* Two handle types at once: two objects named for one buffer, which is a
     * list to refuse rather than to answer with whichever came first. */
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    both(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named,
                             CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, 3u);
    CHECK(process.memory().asked_for(both.list()));
    std::uint64_t made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), both.list(), object_bytes, made),
             CL_INVALID_PROPERTY);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u);
    CHECK_EQ(process.wire().sends(), 0u); }

  { /* A size no whole number of pages holds, and a size of 0. */
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
    std::uint64_t       made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), 0u, made),
             CL_INVALID_VALUE);
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(),
                                   ~std::uint64_t{0}, made),
             CL_INVALID_VALUE);
    CHECK_EQ(process.wire().sends(), 0u);
    CHECK_EQ(process.memory().live(), 0u); }

  { /* A list that names no external handle at all, which is an ordinary
     * buffer and never reaches here through the root. */
    Process          process;
    const Properties none;
    std::uint64_t    made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), none.list(), object_bytes, made),
             CL_INVALID_VALUE);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.wire().sends(), 0u); }

  { /* The device answered ok and named no cl_mem, which the call refuses as
     * malformed: an ok carrying nothing is an answer no guest could use. */
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
    const Answer        nothing(0u, 0u);
    process.wire().set_reply(nothing.bytes(), SharedObjectAnswer::encoded_size);
    std::uint64_t made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_OUT_OF_RESOURCES);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u); }

  { /* The device would not open it. */
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
    const Answer        refused(2u, 0u);
    process.wire().set_reply(refused.bytes(), SharedObjectAnswer::encoded_size);
    std::uint64_t made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_OUT_OF_RESOURCES);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u);
    CHECK(process.memory().invariants_hold());
    CHECK_EQ(process.log().holding("rvkcl: the device would not open a shared object for this context"),
             1u); } }

/* The size the application asked for is rounded to whole pages before it is
 * named, because that is what the publisher's record says and an open is
 * refused against it. A buffer smaller than the object it is over is what an
 * application that wants part of a shared image asks for. */
TEST(cl_external_memory, the_size_named_is_the_application_s_rounded_to_pages)
{ Process             process;
  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const Answer        answered(0u, host_buffer);
  process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t made = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), 4097u, made),
           CL_SUCCESS);
  std::uint64_t asked = 0u;
  std::memcpy(&asked, process.wire().last_payload() + 24u, sizeof asked);
  CHECK_EQ(asked, 8192u); }

/* A context torn down gives back everything it opened: the device counts it as
 * a holder of each, and an object nobody gives back is one the host keeps for
 * as long as the device lives. */
TEST(cl_external_memory, a_context_going_gives_back_everything_it_opened)
{ Process process;
  for (std::uint32_t index = 0u; index < 3u; index++) {
      const std::uint64_t named = process.handle();
      const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
      const Answer        answered(0u, host_buffer + index);
      process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
      std::uint64_t made = 0u;
      CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                     made),
               CL_SUCCESS);
      CHECK_EQ(made, host_buffer + index);
      CHECK(process.memory().invariants_hold());
  }
  CHECK_EQ(process.memory().live(), 3u);
  /* A cl_mem the table never made is not one it gives back. */
  const std::uint32_t sends = process.wire().sends();
  process.memory().give_back(process.wire(), host_buffer + 9u);
  CHECK_EQ(process.wire().sends(), sends);
  CHECK_EQ(process.memory().live(), 3u);

  const Answer released(0u, 0u);
  process.wire().set_reply(released.bytes(), SharedObjectAnswer::encoded_size);
  process.memory().release_all(process.wire());
  CHECK_EQ(process.memory().live(), 0u);
  CHECK_EQ(process.wire().sends(), sends + 3u);
  CHECK(process.memory().invariants_hold());
  /* And again on an empty table sends nothing. */
  process.memory().release_all(process.wire());
  CHECK_EQ(process.wire().sends(), sends + 3u); }

/* One object opened twice into one context is refused before the device, which
 * refuses it too: a context holding two cl_mem over one object could release
 * it twice, and the second release would take a reference another context
 * still holds. A second handle over the same carrier is how an application
 * would do it by accident. */
TEST(cl_external_memory, an_object_this_context_already_holds_is_not_opened_again)
{ Process             process;
  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const Answer        answered(0u, host_buffer);
  process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t made = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes, made),
           CL_SUCCESS);
  const std::uint32_t sends = process.wire().sends();

  /* A second NT handle over the same carrier names the same object. */
  const std::uint64_t again = process.share_again();
  const Properties    twice(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, again);
  CHECK(again != named);
  std::uint64_t second = 9u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), twice.list(), object_bytes, second),
           CL_INVALID_VALUE);
  CHECK_EQ(second, 0u);
  CHECK_EQ(process.wire().sends(), sends);
  CHECK_EQ(process.memory().live(), 1u);
  CHECK(process.memory().invariants_hold());
  CHECK_EQ(process.log().holding("rvkcl: an object this context already imported"), 1u); }

/* A table with no room refuses before the device is asked: an object opened
 * into a table with nowhere to put it would be one nothing ever gave back.
 * Filled by hand, because filling it honestly would need more shared
 * allocations than the kernel this runs on holds. */
TEST(cl_external_memory, it_holds_as_many_as_its_table_and_no_more)
{ Process process;
  for (std::uint32_t index = 0u; index < ClExternalMemory::max_objects; index++)
      ClExternalMemoryProbe::row(process.memory(), index) =
          av::clicd::ClImportedMemoryRow{index + 1u, host_buffer + index, object_bytes};
  ClExternalMemoryProbe::set_live(process.memory(), ClExternalMemory::max_objects);
  CHECK(process.memory().invariants_hold());

  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const std::uint32_t sends = process.wire().sends();
  std::uint64_t       made = 9u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes, made),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), sends);
  CHECK_EQ(process.memory().live(), ClExternalMemory::max_objects);
  CHECK(process.memory().invariants_hold()); }

/* Every way a row can be wrong, made by hand: the invariant exists to fail. */
TEST(cl_external_memory, invariants_reject_rows_no_open_leaves_behind)
{ Process process;
  CHECK(process.memory().invariants_hold());
  ClExternalMemoryProbe::set_live(process.memory(), 1u);
  CHECK(!process.memory().invariants_hold());          /* a row of nothing */
  ClExternalMemoryProbe::row(process.memory(), 0u).object_id_ = 7u;
  CHECK(!process.memory().invariants_hold());          /* no cl_mem behind it */
  ClExternalMemoryProbe::row(process.memory(), 0u).made_ = host_buffer;
  CHECK(!process.memory().invariants_hold());          /* and no size */
  ClExternalMemoryProbe::row(process.memory(), 0u).size_ = object_bytes;
  CHECK(process.memory().invariants_hold());
  /* One cl_mem naming two objects is a table no give-back could unpick. */
  ClExternalMemoryProbe::set_live(process.memory(), 2u);
  ClExternalMemoryProbe::row(process.memory(), 1u) =
      av::clicd::ClImportedMemoryRow{8u, host_buffer, object_bytes};
  CHECK(!process.memory().invariants_hold());
  ClExternalMemoryProbe::row(process.memory(), 1u).made_ = host_buffer + 1u;
  CHECK(process.memory().invariants_hold());
  /* And one object twice is the same table from the other side. */
  ClExternalMemoryProbe::row(process.memory(), 1u).object_id_ = 7u;
  CHECK(!process.memory().invariants_hold());
  ClExternalMemoryProbe::set_live(process.memory(), ClExternalMemory::max_objects + 1u);
  CHECK(!process.memory().invariants_hold()); }

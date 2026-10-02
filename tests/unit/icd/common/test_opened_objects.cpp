/* The shared objects one context opened from a Win32 handle (OpenedObjects),
 * which OpenGL's and OpenCL's imports are both built on: a handle another
 * process sent is opened through the kernel, a memory object is named as the
 * whole pages it was published as and a fence as none, one object is opened
 * once, and every refusal leaves no row and sends nothing it should not.
 * Invariants after every change, and every state the table never builds
 * caught by them. */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/opened_objects.hpp"
#include "icd/common/shared_handles.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"

#include <cstdint>
#include <cstring>

using av::SharedObjectAnswer;
using av::SharedObjectKind;
using av::Status;
using av::icd::OpenedObjects;

namespace av::icd {

struct OpenedObjectsProbe {
    static OpenedObjectRow &row(OpenedObjects &opened, std::uint32_t index) { return opened.rows_[index]; }
    static void set_live(OpenedObjects &opened, std::uint32_t live) { opened.live_ = live; }
};

}  // namespace av::icd

using av::icd::OpenedObjectsProbe;

namespace {

constexpr std::uint64_t page = av::SharedObjectRecord::page_size;
constexpr std::uint64_t made_memory = 0x5150u;
constexpr std::uint64_t made_fence = 0x6160u;

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
 * table over it, a transport playing the device, and the table under test. */
class Process {
public:
    Process() : trace_(), lock_(trace_, 'S'), kmt_(), log_(), wire_(), device_(0u),
                handles_(kmt_, lock_), opened_(handles_, log_, "avtest: ")
    {
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapter = 0u;
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(kmt_.create_device(adapter, device_), Status::ok);
    }

    OpenedObjects            &opened() { return opened_; }
    av::MockTransport        &wire() { return wire_; }
    av::RecordingLog         &log() { return log_; }
    av::icd::SharedHandles   &handles() { return handles_; }
    std::uint32_t             device() const { return device_; }

    /* A handle another process sent: an allocation the kernel minted an
     * identity of `kind` into, and shared - never adopted here. */
    std::uint64_t handle(std::uint32_t kind = SharedObjectKind::memory)
    {
        const av::AllocationDescriptor descriptor(av::AllocationKind::client, true, av::SurfaceLayout(), page,
                                                  av::ScanoutTarget());
        std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
        CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
        const bool                   memory = kind == SharedObjectKind::memory;
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(kind), 0u, memory ? 2u * page : 0u);
        CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        std::uint32_t allocation = 0u;
        CHECK_STATUS(kmt_.create_shared_allocation(device_, bytes, sizeof bytes, allocation), Status::ok);
        std::uint64_t named = 0u;
        CHECK_STATUS(kmt_.share_object(device_, allocation, named), Status::ok);
        return named;
    }

    void answer(std::uint32_t status, std::uint64_t made)
    {
        const Answer answered(status, made);
        wire_.set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
    }

private:
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    av::MockKmt             kmt_;
    av::RecordingLog        log_;
    av::MockTransport       wire_;
    std::uint32_t           device_;
    av::icd::SharedHandles  handles_;
    OpenedObjects           opened_;
};

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

}  // namespace

TEST(opened_objects, a_new_table_holds_nothing)
{ Process process;
  CHECK_EQ(process.opened().live(), 0u);
  CHECK(!process.opened().answered(SharedObjectKind::memory, made_memory));
  CHECK(process.opened().invariants_hold()); }

/* A handle this process never held is adopted through the kernel - which is
 * what tells it from a number an application invented - and the size the
 * application gave in bytes is named as whole pages, which the wire demands. */
TEST(opened_objects, a_handle_another_process_sent_opens_as_whole_pages)
{ Process             process;
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::done, made_memory);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory,
                                     page + 1u, made),
               Status::ok);
  CHECK_EQ(made, made_memory);
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::open_shared_object_id);
  CHECK_EQ(u32_at(process.wire().last_payload(), 8u), SharedObjectKind::memory);
  CHECK_EQ(u64_at(process.wire().last_payload(), 24u), 2u * page);
  CHECK_EQ(process.handles().live(), 1u);
  CHECK(process.opened().answered(SharedObjectKind::memory, made_memory));
  CHECK_EQ(process.opened().live(), 1u);
  CHECK(process.opened().invariants_hold()); }

/* A fence has no bytes, whatever the caller passed for a size. */
TEST(opened_objects, a_fence_opens_with_no_size)
{ Process             process;
  const std::uint64_t named = process.handle(SharedObjectKind::monitored_fence);
  process.answer(SharedObjectAnswer::done, made_fence);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::monitored_fence,
                                     page, made),
               Status::ok);
  CHECK_EQ(u64_at(process.wire().last_payload(), 24u), 0u);
  CHECK(process.opened().answered(SharedObjectKind::monitored_fence, made_fence));
  CHECK(!process.opened().answered(SharedObjectKind::memory, made_fence));
  CHECK(process.opened().invariants_hold()); }

/* Nothing about a handle of 0, a memory object of no bytes or of a size that
 * would wrap, or a kind the wire does not have reaches the kernel or the
 * device. */
TEST(opened_objects, a_request_that_names_no_object_is_refused_before_anything_is_asked)
{ Process             process;
  const std::uint64_t named = process.handle();
  std::uint64_t       made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), 0u, SharedObjectKind::memory, page, made),
               Status::invalid_parameter);
  CHECK_EQ(made, 0u);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, 0u, made),
               Status::invalid_parameter);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory,
                                     ~std::uint64_t{0}, made),
               Status::invalid_parameter);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::native_fence + 1u,
                                     page, made),
               Status::invalid_parameter);
  CHECK_EQ(process.wire().sends(), 0u);
  CHECK_EQ(process.handles().live(), 0u);
  CHECK_EQ(process.opened().live(), 0u); }

/* The refusal this table exists for: a number an application invented is not
 * a handle the kernel lets this process open. */
TEST(opened_objects, a_handle_this_process_may_not_open_is_not_found_and_sends_nothing)
{ Process       process;
  std::uint64_t made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), 0xDEADu, SharedObjectKind::memory, page,
                                     made),
               Status::not_found);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), 0u);
  CHECK_EQ(process.log().holding("avtest: a Win32 handle this process may not open"), 1u);
  CHECK_EQ(process.opened().live(), 0u); }

/* One object opened twice in one context is two handles a guest could release
 * it through: the second open is refused, by either handle to it. */
TEST(opened_objects, one_object_is_opened_once)
{ Process             process;
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::done, made_memory);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, made),
               Status::ok);
  const std::uint32_t sends = process.wire().sends();
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, made),
               Status::conflict);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), sends);
  CHECK_EQ(process.log().holding("avtest: an object this context already imported"), 1u);
  CHECK_EQ(process.opened().live(), 1u);
  CHECK(process.opened().invariants_hold()); }

/* A device that will not open it leaves no row, and says why. */
TEST(opened_objects, a_device_that_refuses_leaves_no_row)
{ Process             process;
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::unknown_object, 0u);
  std::uint64_t made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, made),
               Status::invalid_state);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.opened().live(), 0u);
  CHECK_EQ(process.log().holding("avtest: the device would not open a shared object for this context, status 2"),
           1u);
  CHECK(process.opened().invariants_hold()); }

/* A table with no room refuses before the kernel or the device is asked: an
 * object opened with nowhere to keep it is one nothing would give back. */
TEST(opened_objects, a_full_table_refuses_before_anything_is_asked)
{ Process process;
  for (std::uint32_t index = 0u; index < OpenedObjects::max_objects; index++)
      OpenedObjectsProbe::row(process.opened(), index) =
          av::icd::OpenedObjectRow{index + 1u, made_memory + index, page, SharedObjectKind::memory};
  OpenedObjectsProbe::set_live(process.opened(), OpenedObjects::max_objects);
  CHECK(process.opened().invariants_hold());
  const std::uint64_t named = process.handle();
  std::uint64_t       made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, made),
               Status::no_space);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), 0u);
  CHECK_EQ(process.handles().live(), 0u); }

/* A memory object and a fence the device answered the same number for are two
 * objects - OpenGL's namespaces are separate - and each is given back by its
 * own kind, naming the size it was opened as. */
TEST(opened_objects, each_object_is_given_back_by_its_kind_and_size)
{ Process             process;
  const std::uint64_t memory = process.handle();
  const std::uint64_t fence = process.handle(SharedObjectKind::monitored_fence);
  std::uint64_t       made = 0u;
  process.answer(SharedObjectAnswer::done, made_memory);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), memory, SharedObjectKind::memory,
                                     3u * page, made),
               Status::ok);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), fence, SharedObjectKind::monitored_fence,
                                     0u, made),
               Status::ok);
  CHECK(process.opened().invariants_hold());
  CHECK(!process.opened().give_back(process.wire(), SharedObjectKind::memory, made_memory + 1u));
  CHECK(!process.opened().give_back(process.wire(), SharedObjectKind::memory, 0u));
  CHECK(process.opened().give_back(process.wire(), SharedObjectKind::memory, made_memory));
  /* Given back once: what is left with that number is the fence. */
  CHECK(!process.opened().give_back(process.wire(), SharedObjectKind::memory, made_memory));
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(u32_at(process.wire().last_payload(), 8u), SharedObjectKind::memory);
  CHECK_EQ(u64_at(process.wire().last_payload(), 24u), 3u * page);
  CHECK(!process.opened().answered(SharedObjectKind::memory, made_memory));
  CHECK(process.opened().answered(SharedObjectKind::monitored_fence, made_memory));
  CHECK_EQ(process.opened().live(), 1u);
  CHECK(process.opened().invariants_hold()); }

/* A context torn down gives back everything it still holds, each once. */
TEST(opened_objects, release_all_gives_every_object_back)
{ Process       process;
  std::uint64_t made = 0u;
  for (std::uint32_t index = 0u; index < 3u; index++) {
      const std::uint64_t named = process.handle();
      process.answer(SharedObjectAnswer::done, made_memory + index);
      CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page,
                                         made),
                   Status::ok);
  }
  const std::uint32_t sends = process.wire().sends();
  process.opened().release_all(process.wire());
  CHECK_EQ(process.wire().sends(), sends + 3u);
  CHECK_EQ(process.opened().live(), 0u);
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, report_names_each_object)
{ Process             process;
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::done, made_memory);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, made),
               Status::ok);
  char            text[512] = {};
  av::StateWriter writer(text, sizeof text);
  process.opened().report(writer);
  CHECK(std::strstr(writer.dump(), "object") != nullptr);
  CHECK(std::strstr(writer.dump(), "made=20816") != nullptr);
  CHECK(std::strstr(writer.dump(), "size=4096") != nullptr); }

/* Every way a row can be wrong, made by hand: the invariant exists to fail. */
TEST(opened_objects, invariants_reject_every_row_no_open_leaves)
{ Process process;
  OpenedObjectsProbe::set_live(process.opened(), 2u);
  av::icd::OpenedObjectRow &first = OpenedObjectsProbe::row(process.opened(), 0u);
  av::icd::OpenedObjectRow &second = OpenedObjectsProbe::row(process.opened(), 1u);
  first = av::icd::OpenedObjectRow{7u, made_memory, page, SharedObjectKind::memory};
  second = av::icd::OpenedObjectRow{8u, made_fence, 0u, SharedObjectKind::monitored_fence};
  CHECK(process.opened().invariants_hold());
  const av::icd::OpenedObjectRow whole = second;
  second.object_id_ = 0u;
  CHECK(!process.opened().invariants_hold());          /* no object */
  second = whole;
  second.made_ = 0u;
  CHECK(!process.opened().invariants_hold());          /* nothing the device made */
  second = whole;
  second.size_ = page;
  CHECK(!process.opened().invariants_hold());          /* a fence with bytes */
  second = whole;
  second.kind_ = SharedObjectKind::native_fence + 1u;
  CHECK(!process.opened().invariants_hold());          /* a kind the wire has not */
  second = av::icd::OpenedObjectRow{8u, made_fence, 0u, SharedObjectKind::memory};
  CHECK(!process.opened().invariants_hold());          /* memory of no bytes */
  second.size_ = page + 1u;
  CHECK(!process.opened().invariants_hold());          /* memory not in pages */
  second = whole;
  second.object_id_ = first.object_id_;
  CHECK(!process.opened().invariants_hold());          /* one object twice */
  second = whole;
  second.kind_ = SharedObjectKind::memory;
  second.size_ = page;
  second.made_ = made_memory;
  CHECK(!process.opened().invariants_hold());          /* one kind's answer twice */
  second.kind_ = SharedObjectKind::native_fence;
  second.size_ = 0u;
  CHECK(process.opened().invariants_hold());           /* another kind's is its own */
  OpenedObjectsProbe::set_live(process.opened(), OpenedObjects::max_objects + 1u);
  CHECK(!process.opened().invariants_hold()); }

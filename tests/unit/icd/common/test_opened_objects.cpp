#include "core/log/keyed_log.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
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
    static OpenedObjectRow &add(OpenedObjects &opened, std::uint32_t kind, std::uint64_t made,
                                const OpenedObjectRow &row)
    {
        std::uint32_t slot = 0u;
        CHECK_STATUS(opened.opens_of(kind)->insert(made, row, slot), Status::ok);
        return *opened.opens_of(kind)->find(made);
    }
};

}

using av::icd::OpenedObjectsProbe;

namespace {

constexpr std::uint64_t page = av::SharedObjectRecord::page_size;
constexpr std::uint64_t made_memory = 0x5150u;
constexpr std::uint64_t made_fence = 0x6160u;
constexpr std::uint64_t into = 0x7770u;

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

class Process {
public:
    Process() : memory_(), trace_(), lock_(trace_, 'S'), kmt_(), log_(), lines_(memory_, log_, "avtest: "), wire_(), device_(0u),
                handles_(kmt_, lock_, memory_), opened_(handles_, memory_, lines_)
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
    av::MockPlatform         &memory() { return memory_; }
    std::uint32_t             device() const { return device_; }

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
    av::MockPlatform        memory_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    av::MockKmt             kmt_;
    av::RecordingLog        log_;
    av::KeyedLog            lines_;
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

}

TEST(opened_objects, a_new_table_holds_nothing)
{ Process process;
  CHECK_EQ(process.opened().live(), 0u);
  CHECK(!process.opened().answered(SharedObjectKind::memory, made_memory));
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, a_handle_another_process_sent_opens_as_whole_pages)
{ Process             process;
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::done, made_memory);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory,
                                     page + 1u, into, made),
               Status::ok);
  CHECK_EQ(made, made_memory);
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::open_shared_object_id);
  CHECK_EQ(u32_at(process.wire().last_payload(), 8u), SharedObjectKind::memory);
  CHECK_EQ(u64_at(process.wire().last_payload(), 24u), 2u * page);
  CHECK_EQ(process.handles().live(), 1u);
  CHECK(process.opened().answered(SharedObjectKind::memory, made_memory));
  CHECK_EQ(process.opened().live(), 1u);
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, a_fence_opens_with_no_size)
{ Process             process;
  const std::uint64_t named = process.handle(SharedObjectKind::monitored_fence);
  process.answer(SharedObjectAnswer::done, made_fence);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::monitored_fence,
                                     page, into, made),
               Status::ok);
  CHECK_EQ(u64_at(process.wire().last_payload(), 24u), 0u);
  CHECK(process.opened().answered(SharedObjectKind::monitored_fence, made_fence));
  CHECK(!process.opened().answered(SharedObjectKind::memory, made_fence));
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, a_request_that_names_no_object_is_refused_before_anything_is_asked)
{ Process             process;
  const std::uint64_t named = process.handle();
  std::uint64_t       made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), 0u, SharedObjectKind::memory, page, into, made),
               Status::invalid_parameter);
  CHECK_EQ(made, 0u);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, 0u, into, made),
               Status::invalid_parameter);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory,
                                     ~std::uint64_t{0}, into, made),
               Status::invalid_parameter);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::native_fence + 1u,
                                     page, into, made),
               Status::invalid_parameter);
  CHECK_EQ(process.wire().sends(), 0u);
  CHECK_EQ(process.handles().live(), 0u);
  CHECK_EQ(process.opened().live(), 0u); }

TEST(opened_objects, a_handle_this_process_may_not_open_is_not_found_and_sends_nothing)
{ Process       process;
  std::uint64_t made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), 0xDEADu, SharedObjectKind::memory, page,
                                     into, made),
               Status::not_found);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), 0u);
  CHECK_EQ(process.log().holding("avtest: a Win32 handle this process may not open"), 1u);
  CHECK_EQ(process.opened().live(), 0u); }

TEST(opened_objects, one_object_is_opened_once)
{ Process             process;
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::done, made_memory);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, into, made),
               Status::ok);
  const std::uint32_t sends = process.wire().sends();
  process.answer(SharedObjectAnswer::conflict, 0u);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, into, made),
               Status::conflict);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), sends + 1u);
  CHECK_EQ(process.log().holding("avtest: an object this context already opened into the same object"), 1u);
  CHECK_EQ(process.opened().live(), 1u);
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, a_device_that_refuses_leaves_no_row)
{ Process             process;
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::unknown_object, 0u);
  std::uint64_t made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, into, made),
               Status::invalid_state);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.opened().live(), 0u);
  CHECK_EQ(process.log().holding("avtest: the device would not open a shared object for this context, status 2"),
           1u);
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, opens_past_any_fixed_table)
{ Process process;
  for (std::uint32_t index = 0u; index < 100u; index++)
      static_cast<void>(OpenedObjectsProbe::add(process.opened(), SharedObjectKind::memory,
                                                made_memory + 0x10000u + index,
                                                av::icd::OpenedObjectRow{index + 1000u, into, page}));
  CHECK(process.opened().invariants_hold());
  const std::uint64_t named = process.handle();
  process.answer(SharedObjectAnswer::done, made_memory);
  std::uint64_t made = 0u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, into,
                                     made),
               Status::ok);
  CHECK_EQ(made, made_memory);
  CHECK_EQ(process.opened().live(), 101u);
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, no_memory_for_a_row_refuses_before_the_device_is_asked)
{ Process             process;
  const std::uint64_t named = process.handle();
  std::uint64_t       adopted = 0u;
  CHECK_STATUS(process.handles().adopt(process.device(), named, adopted), Status::ok);
  process.memory().fail_allocation_at(1u);
  std::uint64_t made = 7u;
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, into, made),
               Status::no_memory);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), 0u);
  CHECK_EQ(process.opened().live(), 0u);
  CHECK_EQ(process.log().holding("avtest: no memory to keep an opened shared object"), 1u);
  CHECK(process.opened().invariants_hold());
  Process             fresh;
  const std::uint64_t another = fresh.handle();
  fresh.memory().fail_allocation_at(1u);
  CHECK_STATUS(fresh.opened().open(fresh.wire(), fresh.device(), another, SharedObjectKind::memory, page, into, made),
               Status::no_memory);
  CHECK_EQ(fresh.wire().sends(), 0u);
  CHECK_EQ(fresh.handles().live(), 0u);
  CHECK_EQ(fresh.log().holding("avtest: no memory to keep a Win32 handle"), 1u); }

TEST(opened_objects, each_object_is_given_back_by_its_kind_and_size)
{ Process             process;
  const std::uint64_t memory = process.handle();
  const std::uint64_t fence = process.handle(SharedObjectKind::monitored_fence);
  std::uint64_t       made = 0u;
  process.answer(SharedObjectAnswer::done, made_memory);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), memory, SharedObjectKind::memory,
                                     3u * page, into, made),
               Status::ok);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), fence, SharedObjectKind::monitored_fence,
                                     0u, into, made),
               Status::ok);
  CHECK(process.opened().invariants_hold());
  CHECK(!process.opened().give_back(process.wire(), SharedObjectKind::memory, made_memory + 1u));
  CHECK(!process.opened().give_back(process.wire(), SharedObjectKind::memory, 0u));
  CHECK(process.opened().give_back(process.wire(), SharedObjectKind::memory, made_memory));
  CHECK(!process.opened().give_back(process.wire(), SharedObjectKind::memory, made_memory));
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(u32_at(process.wire().last_payload(), 8u), SharedObjectKind::memory);
  CHECK_EQ(u64_at(process.wire().last_payload(), 24u), 3u * page);
  CHECK(!process.opened().answered(SharedObjectKind::memory, made_memory));
  CHECK(process.opened().answered(SharedObjectKind::monitored_fence, made_memory));
  CHECK_EQ(process.opened().live(), 1u);
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, release_all_gives_every_object_back)
{ Process       process;
  std::uint64_t made = 0u;
  for (std::uint32_t index = 0u; index < 3u; index++) {
      const std::uint64_t named = process.handle();
      process.answer(SharedObjectAnswer::done, made_memory + index);
      CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page,
                                         into, made),
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
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page, into, made),
               Status::ok);
  char            text[512] = {};
  av::StateWriter writer(text, sizeof text);
  process.opened().report(writer);
  CHECK(std::strstr(writer.dump(), "object") != nullptr);
  CHECK(std::strstr(writer.dump(), "opened/20816") != nullptr);
  CHECK(std::strstr(writer.dump(), "kind=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "into=0x") != nullptr);
  CHECK(std::strstr(writer.dump(), "size=4096") != nullptr); }

TEST(opened_objects, invariants_reject_every_row_no_open_leaves)
{ Process process;
  av::icd::OpenedObjectRow &first = OpenedObjectsProbe::add(
      process.opened(), SharedObjectKind::memory, made_memory, av::icd::OpenedObjectRow{7u, into, page});
  av::icd::OpenedObjectRow &second = OpenedObjectsProbe::add(
      process.opened(), SharedObjectKind::monitored_fence, made_fence, av::icd::OpenedObjectRow{8u, into, 0u});
  CHECK(process.opened().invariants_hold());
  const av::icd::OpenedObjectRow whole = second;
  second.object_id_ = 0u;
  CHECK(!process.opened().invariants_hold());
  second = whole;
  second.size_ = page;
  CHECK(!process.opened().invariants_hold());
  second = whole;
  CHECK(process.opened().invariants_hold());
  const av::icd::OpenedObjectRow memory = first;
  first.size_ = 0u;
  CHECK(!process.opened().invariants_hold());
  first.size_ = page + 1u;
  CHECK(!process.opened().invariants_hold());
  first.object_id_ = 0u;
  first.size_ = page;
  CHECK(!process.opened().invariants_hold());
  first = memory;
  CHECK(process.opened().invariants_hold()); }

TEST(opened_objects, a_context_gives_back_only_what_it_opened_into)
{ Process       process;
  const std::uint64_t other = into + 0x10u;
  std::uint64_t made = 0u;
  for (std::uint32_t index = 0u; index < 3u; index++) {
      const std::uint64_t named = process.handle();
      process.answer(SharedObjectAnswer::done, made_memory + index);
      CHECK_STATUS(process.opened().open(process.wire(), process.device(), named, SharedObjectKind::memory, page,
                                         index == 1u ? other : into, made),
                   Status::ok);
  }
  const std::uint64_t fence = process.handle(SharedObjectKind::monitored_fence);
  process.answer(SharedObjectAnswer::done, made_fence);
  CHECK_STATUS(process.opened().open(process.wire(), process.device(), fence, SharedObjectKind::monitored_fence,
                                     0u, into, made),
               Status::ok);
  CHECK_EQ(process.opened().live(), 4u);
  const std::uint32_t sends = process.wire().sends();
  process.opened().give_back_into(process.wire(), 0u);
  CHECK_EQ(process.wire().sends(), sends);
  process.opened().give_back_into(process.wire(), into);
  CHECK_EQ(process.wire().sends(), sends + 3u);
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(process.opened().live(), 1u);
  CHECK(process.opened().answered(SharedObjectKind::memory, made_memory + 1u));
  CHECK(!process.opened().answered(SharedObjectKind::memory, made_memory));
  CHECK(!process.opened().answered(SharedObjectKind::monitored_fence, made_fence));
  process.opened().give_back_into(process.wire(), into);
  CHECK_EQ(process.wire().sends(), sends + 3u);
  CHECK(process.opened().invariants_hold()); }

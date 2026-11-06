#include "core/log/keyed_log.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_allocation.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/opengl/pinned_buffers.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/blob_import_answer.hpp"

#include <cstdint>

using av::GuestConnection;
using av::HostAllocation;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;
using av::glicd::PinnedBuffers;

namespace {

const PciIdentity avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t first_memory = 0x0000700000000001ull;
constexpr std::uint64_t page = 4096u;

alignas(4096) std::uint8_t pages[3u * 4096u];

class Rig {
public:
    Rig()
        : kmt_(), platform_(), connection_(kmt_), host_(kmt_), log_(), lines_(platform_, log_, "avtest: "), trace_(),
          lock_(trace_, 'S'),
          channel_(platform_, lines_, av::icd::ChannelShape(SubApi::opengl, 12u, 4096u, 4096u, 0u, 0u), lock_),
          pinned_(platform_, channel_)
    {
        kmt_.add_adapter(avgpu, 3200u);
        kmt_.open_host_window(MockKmt::rig_window_bytes);
        std::uint32_t adapter = 0u, count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter), Status::ok);
        CHECK_STATUS(channel_.open(connection_), Status::ok);
    }
    ~Rig()
    {
        channel_.close(true);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    PinnedBuffers      &pinned() { return pinned_; }
    av::icd::HostChannel &channel() { return channel_; }
    av::ScriptedHost   &host() { return host_; }
    av::MockPlatform   &platform() { return platform_; }

private:
    MockKmt                 kmt_;
    av::MockPlatform        platform_;
    GuestConnection         connection_;
    av::ScriptedHost        host_;
    av::RecordingLog        log_;
    av::KeyedLog            lines_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    av::icd::HostChannel    channel_;
    PinnedBuffers           pinned_;
};

}

TEST(pinned_buffers, nothing_is_pinned_for_a_buffer_that_is_not_bound)
{ Rig rig;
  CHECK_EQ(rig.pinned().bound(), 0u);
  HostAllocation *lent = reinterpret_cast<HostAllocation *>(&rig);
  std::uint64_t   offset = 9u;
  CHECK_STATUS(rig.pinned().pin(pages, 64u, lent, offset), Status::invalid_state);
  CHECK(lent == nullptr);
  CHECK_EQ(offset, 0u);
  CHECK_EQ(rig.channel().memories(), 0u); }

TEST(pinned_buffers, a_pin_lends_the_pages_under_the_address_and_names_where_in_them_it_begins)
{ Rig rig;
  rig.pinned().bind(7u);
  CHECK_EQ(rig.pinned().bound(), 7u);
  rig.host().blob_import(0u, first_memory);
  HostAllocation *lent = nullptr;
  std::uint64_t   offset = 0u;
  CHECK_STATUS(rig.pinned().pin(pages + 100u, 200u, lent, offset), Status::ok);
  CHECK(lent != nullptr);
  CHECK(lent->view() == pages);
  CHECK_EQ(lent->bytes(), page);
  CHECK_EQ(lent->handle(), first_memory);
  CHECK_EQ(offset, 100u);
  CHECK_EQ(rig.channel().memories(), 1u);
  CHECK(rig.channel().invariants_hold());
  rig.pinned().bind(8u);
  rig.host().blob_import(0u, first_memory + 1u);
  HostAllocation *across = nullptr;
  CHECK_STATUS(rig.pinned().pin(pages + page - 6u, 20u, across, offset), Status::ok);
  CHECK(across->view() == pages);
  CHECK_EQ(across->bytes(), 2u * page);
  CHECK_EQ(offset, page - 6u);
  CHECK_EQ(rig.channel().memories(), 2u); }

TEST(pinned_buffers, a_buffer_is_pinned_once_and_a_pin_the_channel_refuses_pins_nothing)
{ Rig rig;
  rig.pinned().bind(7u);
  rig.host().blob_import(0u, first_memory);
  HostAllocation *lent = nullptr;
  std::uint64_t   offset = 0u;
  CHECK_STATUS(rig.pinned().pin(pages, 64u, lent, offset), Status::ok);
  HostAllocation *again = reinterpret_cast<HostAllocation *>(&rig);
  CHECK_STATUS(rig.pinned().pin(pages, 64u, again, offset), Status::conflict);
  CHECK(again == nullptr);
  CHECK_EQ(rig.channel().memories(), 1u);
  rig.pinned().bind(9u);
  rig.host().blob_import(av::BlobImportAnswer::executor_refused, 0u);
  CHECK(rig.pinned().pin(pages + page, 64u, again, offset) != Status::ok);
  CHECK(again == nullptr);
  CHECK_EQ(rig.channel().memories(), 1u);
  CHECK_EQ(rig.pinned().bound(), 9u); }

TEST(pinned_buffers, a_pin_with_no_memory_to_keep_it_pins_nothing)
{ Rig rig;
  rig.pinned().bind(7u);
  rig.platform().fail_allocation_at(1u);
  HostAllocation *lent = reinterpret_cast<HostAllocation *>(&rig);
  std::uint64_t   offset = 0u;
  CHECK_STATUS(rig.pinned().pin(pages, 64u, lent, offset), Status::no_memory);
  CHECK(lent == nullptr);
  CHECK_EQ(rig.channel().memories(), 0u);
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(rig.pinned().pin(pages, 64u, lent, offset), Status::ok);
  CHECK_EQ(rig.channel().memories(), 1u); }

TEST(pinned_buffers, forgetting_a_buffer_retires_its_pin_until_reclaimed_and_unbinds_it)
{ Rig rig;
  rig.pinned().bind(7u);
  rig.host().blob_import(0u, first_memory);
  HostAllocation *lent = nullptr;
  std::uint64_t   offset = 0u;
  CHECK_STATUS(rig.pinned().pin(pages, 64u, lent, offset), Status::ok);
  const std::uint32_t others[] = {3u, 4u};
  rig.pinned().forget(2, others);
  CHECK_EQ(rig.pinned().bound(), 7u);
  rig.pinned().forget(0, nullptr);
  CHECK_EQ(rig.channel().memories(), 1u);
  const std::uint32_t going[] = {3u, 7u};
  rig.pinned().forget(2, going);
  CHECK_EQ(rig.pinned().bound(), 0u);
  CHECK_EQ(rig.channel().memories(), 1u);
  rig.host().nop();
  rig.pinned().reclaim();
  CHECK_EQ(rig.channel().memories(), 0u);
  rig.pinned().bind(7u);
  rig.host().blob_import(0u, first_memory + 1u);
  CHECK_STATUS(rig.pinned().pin(pages, 64u, lent, offset), Status::ok);
  CHECK(rig.channel().invariants_hold()); }

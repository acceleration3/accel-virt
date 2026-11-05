#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/arena_segment.hpp"
#include "guest/transport/arena_slots.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/published_slot_layout.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>


using av::ClientBlobLease;
using av::CommandHeader;
using av::GuestConnection;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using av::SubApi;

namespace {

const PciIdentity pci(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t imported_memory = 0x0000600000000077ull;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint64_t window_bytes = std::uint64_t{256} << 20u;
constexpr std::uint64_t slot = av::GuestBytes::slot_bytes;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), host_(kmt_), staging_{}, spare_{},
          transport_(connection_, SubApi::opengl, 12u, reply_bytes, staging_, sizeof staging_),
          slots_(), segment_(connection_, transport_, slots_)
    {
        kmt_.add_adapter(pci, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        kmt_.open_host_window(window_bytes);
        CHECK(segment_.invariants_hold());
    }
    ~Rig() { CHECK_EQ(kmt_.misuses(), 0u); }

    MockKmt         &kmt() { return kmt_; }
    ScriptedHost    &host() { return host_; }
    GuestConnection &connection() { return connection_; }
    std::uint8_t    *spare_staging() { return spare_; }
    KmtTransport &transport() { return transport_; }
    av::ArenaSegment &segment() { return segment_; }
    av::ArenaSlots   &slots() { return slots_; }

    void host_takes_the_import(std::uint64_t memory) { host_.blob_import(0u, memory); }

    void check_nothing_is_held()
    {
        CHECK(segment_.invariants_hold());
        CHECK(!segment_.held());
        CHECK_EQ(slots_.taken(), 0u);
        CHECK_EQ(kmt_.window_chunks(), 0u);
    }

private:
    MockKmt         kmt_;
    std::uint32_t   adapter_;
    GuestConnection connection_;
    ScriptedHost    host_;
    std::uint8_t    staging_[1024];
    std::uint8_t    spare_[1024];
    KmtTransport    transport_;
    av::ArenaSlots slots_;
    av::ArenaSegment segment_;
};

std::uint32_t command_of(const MockKmt &kmt, std::uint32_t id, std::uint32_t nth)
{
    for (std::uint32_t at = 0u; at < kmt.commands(); at++)
        if (kmt.command_id(at) == id && nth-- == 0u) return at;
    return 0xFFFFFFFFu;
}

const std::uint8_t *body_of(const MockKmt &kmt, std::uint32_t id, std::uint32_t nth)
{
    return kmt.command_bytes(command_of(kmt, id, nth)) + CommandHeader::encoded_size;
}

std::uint32_t u32_at(const std::uint8_t *bytes, std::uint32_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::uint32_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}


}

TEST(arena_segment, a_new_segment_holds_nothing_and_no_slot)
{ Rig rig;
  CHECK(!rig.segment().held());
  CHECK_EQ(rig.segment().bytes(), 0u);
  CHECK(rig.segment().view() == nullptr);
  CHECK_STATUS(rig.segment().close(), Status::ok);
  CHECK_STATUS(rig.segment().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.check_nothing_is_held(); }

TEST(arena_segment_open_at, imports_whole_pages_at_the_base_with_an_arena_role)
{ Rig rig;
  rig.host_takes_the_import(imported_memory);
  CHECK_STATUS(rig.segment().open_at(0u, 5000u), Status::ok);
  CHECK(rig.segment().invariants_hold());
  CHECK(rig.segment().held());
  CHECK_EQ(rig.segment().bytes(), 8192u);
  CHECK_EQ(rig.segment().base(), 0u);
  CHECK(rig.segment().view() == rig.kmt().window_chunk_memory(MockKmt::first_window_resource));
  const std::uint8_t *const request = body_of(rig.kmt(), CommandHeader::import_blob_id, 0u);
  CHECK_EQ(u32_at(request, 0u), MockKmt::first_window_resource);
  CHECK_EQ(u32_at(request, 4u), 1u);
  CHECK_EQ(u64_at(request, 8u), 0u);
  CHECK_EQ(rig.slots().taken(), 1u);
  rig.host().nop();
  CHECK_STATUS(rig.segment().close(), Status::ok);
  CHECK_EQ(u64_at(body_of(rig.kmt(), CommandHeader::release_blob_id, 0u), 0u), imported_memory);
  rig.check_nothing_is_held(); }

TEST(arena_segment_open, takes_the_first_free_slot_after_the_control_slot_and_all_it_covers)
{ Rig rig;
  rig.host_takes_the_import(imported_memory);
  CHECK_STATUS(rig.segment().open(slot + 4096u), Status::ok);
  CHECK_EQ(rig.segment().base(), slot);
  CHECK_EQ(rig.segment().bytes(), slot + 4096u);
  CHECK_EQ(rig.slots().taken(), 2u);
  CHECK_EQ(u64_at(body_of(rig.kmt(), CommandHeader::import_blob_id, 0u), 8u), slot);
  rig.host().nop();
  CHECK_STATUS(rig.segment().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(arena_segment_open, refuses_empty_overflowing_taken_and_second_opens_without_calling_the_host)
{ Rig rig;
  CHECK_STATUS(rig.segment().open(0u), Status::no_space);
  CHECK_STATUS(rig.segment().open(~std::uint64_t{0}), Status::no_space);
  CHECK_STATUS(rig.segment().open(slot * av::GuestBytes::slot_count), Status::no_space);
  CHECK_STATUS(rig.segment().open_at(0u, 0u), Status::no_space);
  CHECK_STATUS(rig.segment().open_at(4096u, 4096u), Status::no_space);
  CHECK(rig.slots().take_at(0u, 1u));
  CHECK_STATUS(rig.segment().open_at(0u, 4096u), Status::no_space);
  rig.slots().give(0u, 1u);
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.host_takes_the_import(imported_memory);
  CHECK_STATUS(rig.segment().open(4096u), Status::ok);
  CHECK_STATUS(rig.segment().open(4096u), Status::invalid_state);
  CHECK_STATUS(rig.segment().open_at(0u, 4096u), Status::invalid_state);
  rig.host().nop();
  CHECK_STATUS(rig.segment().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(arena_segment_open, an_import_the_host_refuses_gives_the_slots_and_the_chunk_back)
{ Rig rig;
  rig.host().blob_import(av::BlobImportAnswer::no_room, 0u);
  CHECK_STATUS(rig.segment().open(4096u), Status::no_memory);
  rig.check_nothing_is_held();
  rig.host().blob_import(av::BlobImportAnswer::executor_refused, 0u);
  CHECK_STATUS(rig.segment().open_at(0u, 4096u), Status::invalid_parameter);
  rig.check_nothing_is_held(); }

TEST(arena_segment_close, lets_the_slots_go_even_when_the_host_answers_badly)
{ Rig rig;
  rig.host_takes_the_import(imported_memory);
  CHECK_STATUS(rig.segment().open(4096u), Status::ok);
  rig.host().blob_import(0u, 1u);
  CHECK_STATUS(rig.segment().close(), Status::malformed);
  rig.check_nothing_is_held(); }

TEST(arena_segment_abandon, lets_the_chunk_and_slots_go_without_a_host_call)
{ Rig rig;
  rig.host_takes_the_import(imported_memory);
  CHECK_STATUS(rig.segment().open(4096u), Status::ok);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_STATUS(rig.segment().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), commands);
  rig.check_nothing_is_held(); }

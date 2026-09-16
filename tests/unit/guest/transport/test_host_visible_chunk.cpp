/* A process's host-visible chunk over MockKmt, which binds as the driver does:
 * made resident and locked on the connection's device, bound by escape to the
 * transport's context with the answer read back, and every part released
 * whatever failed. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_visible_chunk.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/host_visible_bind_record.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct HostVisibleChunkProbe {
    static void view_without_allocation(HostVisibleChunk &chunk) { chunk.allocation_ = 0u; }
    static void bound_without_pages(HostVisibleChunk &chunk) { chunk.pages_ = 0u; }
    static void bound_without_allocation(HostVisibleChunk &chunk)
    {
        chunk.allocation_ = 0u;
        chunk.view_ = nullptr;
    }
    static void forget(HostVisibleChunk &chunk)
    {
        chunk.allocation_ = 0u;
        chunk.view_ = nullptr;
        chunk.resource_id_ = 0u;
        chunk.pages_ = 0u;
    }
};

}  // namespace av

using av::GuestConnection;
using av::HostVisibleBindRecord;
using av::HostVisibleChunk;
using av::HostVisibleChunkProbe;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;

namespace {

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t chunk_bytes = std::uint64_t{64} << 20u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint32_t refused = 0xC0000001u;
/* The connection's device and queue, the transport's four allocations, its DMA
 * mapping and its context, with the adapter. */
constexpr std::uint32_t link_objects = 9u;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), staging_{},
          transport_(connection_, SubApi::vulkan, 12u, reply_bytes, staging_, sizeof staging_)
    {
        kmt_.add_adapter(rvk, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), link_objects);
    }

    MockKmt      &kmt() { return kmt_; }
    GuestConnection &connection() { return connection_; }
    KmtTransport &transport() { return transport_; }

    void check_only_the_link_is_left()
    {
        CHECK_EQ(kmt_.open_objects(), link_objects);
        CHECK_EQ(kmt_.bindings(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

private:
    MockKmt         kmt_;
    std::uint32_t   adapter_;
    GuestConnection connection_;
    std::uint8_t    staging_[1024];
    KmtTransport    transport_;
};

std::uint32_t u32_at(const std::uint8_t *bytes, std::uint32_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

}  // namespace

TEST(host_visible_chunk, allocates_locks_binds_and_releases_on_the_transports_context)
{ Rig rig;
  HostVisibleChunk chunk(rig.connection(), rig.transport());
  CHECK(chunk.invariants_hold());
  CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
  CHECK(chunk.invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), link_objects + 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::make_resident), 5u);   /* the transport's four, and this */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_fence), 6u);  /* and the DMA buffer's mapping */
  CHECK(chunk.view() == nullptr);
  CHECK_STATUS(chunk.lock(), Status::ok);
  CHECK(chunk.invariants_hold());
  CHECK(chunk.view() != nullptr);
  chunk.view()[chunk_bytes - 1u] = 0x5Au;
  CHECK_EQ(chunk.resource_id(), 0u);
  CHECK_STATUS(chunk.bind(), Status::ok);
  CHECK(chunk.invariants_hold());
  CHECK_EQ(chunk.resource_id(), MockKmt::first_chunk_resource);
  CHECK_EQ(chunk.pages(), chunk_bytes / 4096u);
  CHECK_EQ(rig.kmt().bindings(), 1u);
  /* The escape named the chunk, of the host-visible kind, on the transport's context. */
  const std::uint32_t allocation = rig.kmt().bound_allocation(0u);
  CHECK_EQ(rig.kmt().allocation_kind(allocation), static_cast<std::uint16_t>(av::AllocationKind::host_visible_memory));
  CHECK_EQ(rig.kmt().allocation_pages(allocation), chunk_bytes / 4096u);
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(rig.kmt().view_of(allocation)),
           reinterpret_cast<std::uintptr_t>(chunk.view()));
  CHECK_EQ(u32_at(rig.kmt().last_escape(), 0u), HostVisibleBindRecord::magic);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK(chunk.invariants_hold());
  CHECK(chunk.view() == nullptr);
  CHECK_EQ(chunk.resource_id(), 0u);
  CHECK_EQ(chunk.pages(), 0u);
  /* The unbind named the resource the bind was answered. */
  CHECK_EQ(u32_at(rig.kmt().last_escape(), 12u), MockKmt::first_chunk_resource);
  rig.check_only_the_link_is_left();
  /* Released twice, nothing is asked. */
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes); }

TEST(host_visible_chunk, each_step_out_of_order_is_refused_without_a_call)
{ Rig rig;
  HostVisibleChunk chunk(rig.connection(), rig.transport());
  CHECK_STATUS(chunk.lock(), Status::invalid_state);
  CHECK_STATUS(chunk.bind(), Status::invalid_state);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::lock), 2u);   /* the transport's ring and reply */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 0u);
  CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
  CHECK_STATUS(chunk.allocate(chunk_bytes), Status::invalid_state);
  CHECK_STATUS(chunk.lock(), Status::ok);
  CHECK_STATUS(chunk.lock(), Status::invalid_state);
  CHECK_STATUS(chunk.bind(), Status::ok);
  CHECK_STATUS(chunk.bind(), Status::invalid_state);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), 5u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), 1u);
  CHECK_STATUS(chunk.release(), Status::ok);
  rig.check_only_the_link_is_left(); }

/* A bind needs only the allocation: an unlocked chunk binds and releases too. */
TEST(host_visible_chunk, an_unlocked_chunk_binds_and_releases)
{ Rig rig;
  HostVisibleChunk chunk(rig.connection(), rig.transport());
  CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
  CHECK_STATUS(chunk.bind(), Status::ok);
  const std::uint32_t unlocks = rig.kmt().calls(MockKmt::Call::unlock);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::unlock), unlocks);
  rig.check_only_the_link_is_left(); }

/* Each call's refusal is the step's answer with the driver's code kept, and
 * release takes back what was made before it. */
TEST(host_visible_chunk, a_refusal_at_each_call_is_answered_and_released)
{ struct Case { MockKmt::Call call; std::uint32_t nth; std::uint32_t step; };
  /* step: 0 allocate, 1 lock, 2 bind */
  const Case cases[] = {{MockKmt::Call::create_allocation, 1u, 0u}, {MockKmt::Call::make_resident, 1u, 0u},
                        {MockKmt::Call::wait_for_fence, 1u, 0u},    {MockKmt::Call::lock, 1u, 1u},
                        {MockKmt::Call::escape, 1u, 2u}};
  for (const Case &one : cases) {
      Rig rig;
      HostVisibleChunk chunk(rig.connection(), rig.transport());
      rig.kmt().fail_at(one.call, one.nth, refused);
      Status status = chunk.allocate(chunk_bytes);
      if (one.step >= 1u) { CHECK_STATUS(status, Status::ok); status = chunk.lock(); }
      if (one.step >= 2u) { CHECK_STATUS(status, Status::ok); status = chunk.bind(); }
      CHECK_STATUS(status, Status::invalid_state);
      CHECK_EQ(rig.kmt().last_code(), refused);
      CHECK(chunk.invariants_hold());
      CHECK_EQ(chunk.resource_id(), 0u);
      CHECK_STATUS(chunk.release(), Status::ok);
      rig.check_only_the_link_is_left();
  } }

/* What the kernel writes back is its word: a record left as it was sent - no
 * resource - or one naming no pages is no answer, and nothing is bound. */
TEST(host_visible_chunk, an_answer_that_does_not_answer_the_bind_is_malformed)
{ { Rig rig;
    HostVisibleChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
    rig.kmt().zero_next(MockKmt::Call::escape);
    CHECK_STATUS(chunk.bind(), Status::malformed);
    CHECK_EQ(chunk.resource_id(), 0u);
    CHECK(chunk.invariants_hold());
    const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
    CHECK_STATUS(chunk.release(), Status::ok);
    CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);   /* nothing bound, nothing unbound */
    rig.check_only_the_link_is_left(); }
  { Rig rig;
    HostVisibleChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
    /* A well-formed answer, but to a bind of some other allocation. */
    std::uint8_t answer[HostVisibleBindRecord::encoded_size] = {};
    CHECK_STATUS(HostVisibleBindRecord(HostVisibleBindRecord::Operation::bind, 0x7FFFFFF0u, 0u)
                     .answered(MockKmt::first_chunk_resource, chunk_bytes / 4096u)
                     .encode(answer, sizeof answer),
                 Status::ok);
    rig.kmt().answer_next_escape(answer, sizeof answer);
    CHECK_STATUS(chunk.bind(), Status::malformed);
    CHECK_EQ(chunk.pages(), 0u);
    CHECK_STATUS(chunk.release(), Status::ok);
    rig.check_only_the_link_is_left(); } }

/* Every part is let go whatever the one before answered, and the first failure
 * is what release says. */
TEST(host_visible_chunk, release_attempts_every_part_and_answers_the_first_failure)
{ struct Case { MockKmt::Call first; MockKmt::Call second; };
  const Case cases[] = {{MockKmt::Call::escape, MockKmt::Call::unlock},
                        {MockKmt::Call::unlock, MockKmt::Call::destroy_allocation},
                        {MockKmt::Call::destroy_allocation, MockKmt::Call::count}};
  for (const Case &one : cases) {
      Rig rig;
      HostVisibleChunk chunk(rig.connection(), rig.transport());
      CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
      CHECK_STATUS(chunk.lock(), Status::ok);
      CHECK_STATUS(chunk.bind(), Status::ok);
      const std::uint32_t destroys = rig.kmt().calls(MockKmt::Call::destroy_allocation);
      rig.kmt().fail_at(one.first, 1u, refused);
      /* A later failure of another kind, which must not replace the first. */
      if (one.second != MockKmt::Call::count) rig.kmt().fail_at(one.second, 1u, 0xC0000017u);
      CHECK_STATUS(chunk.release(), Status::invalid_state);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_allocation), destroys + 1u);
      CHECK(chunk.invariants_hold());
      CHECK(chunk.view() == nullptr);
      CHECK_EQ(chunk.resource_id(), 0u);
      /* Nothing is asked again: what the Kmt refused to let go is its to keep. */
      const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
      CHECK_STATUS(chunk.release(), Status::ok);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  } }

/* On a transport the host has faulted the driver refuses the unbind and
 * unbinds the chunk itself as it is destroyed, so the unbind is not asked:
 * the view and the allocation still go. */
TEST(host_visible_chunk, release_on_a_faulted_transport_leaves_the_unbind_to_the_kernel)
{ Rig rig;
  HostVisibleChunk chunk(rig.connection(), rig.transport());
  CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
  CHECK_STATUS(chunk.lock(), Status::ok);
  CHECK_STATUS(chunk.bind(), Status::ok);
  rig.kmt().withhold_answer_at(1u);
  av::WireWriter request = rig.transport().request_writer();
  request.write_u32(1u);
  av::WireReader reply(nullptr, 0u);
  CHECK(rig.transport().send_sync(1u, request, reply) != av::TransportResult::ok);
  rig.transport().end_call();
  CHECK(rig.transport().faulted());
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  const std::uint32_t unlocks = rig.kmt().calls(MockKmt::Call::unlock);
  CHECK_STATUS(chunk.release(), Status::ok);
  CHECK(chunk.invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::unlock), unlocks + 1u);
  CHECK(chunk.view() == nullptr);
  CHECK_EQ(chunk.resource_id(), 0u);
  rig.check_only_the_link_is_left(); }

/* A chunk that goes out of scope holding everything gives it all back. */
TEST(host_visible_chunk, the_destructor_releases_what_is_held)
{ Rig rig;
  { HostVisibleChunk chunk(rig.connection(), rig.transport());
    CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
    CHECK_STATUS(chunk.lock(), Status::ok);
    CHECK_STATUS(chunk.bind(), Status::ok); }
  rig.check_only_the_link_is_left(); }

TEST(host_visible_chunk, invariants_refuse_a_view_or_binding_without_its_parts)
{ Rig rig;
  HostVisibleChunk chunk(rig.connection(), rig.transport());
  CHECK_STATUS(chunk.allocate(chunk_bytes), Status::ok);
  CHECK_STATUS(chunk.lock(), Status::ok);
  CHECK_STATUS(chunk.bind(), Status::ok);
  HostVisibleChunkProbe::bound_without_pages(chunk);
  CHECK(!chunk.invariants_hold());
  HostVisibleChunkProbe::view_without_allocation(chunk);
  CHECK(!chunk.invariants_hold());
  HostVisibleChunkProbe::bound_without_allocation(chunk);
  CHECK(!chunk.invariants_hold());
  HostVisibleChunkProbe::forget(chunk);
  CHECK(chunk.invariants_hold()); }

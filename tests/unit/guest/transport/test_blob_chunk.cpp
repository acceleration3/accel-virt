/* A chunk of whichever kind the kernel can give (docs/host-window.md), over
 * MockKmt: the host window first, and guest RAM for the chunk the window
 * cannot hold - a device with no window, a full window, no free chunk slot -
 * while an answer the kernel garbled is not papered over with guest RAM. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/blob_chunk.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_kind.hpp"

#include <cstdint>

namespace av {

struct BlobChunkProbe {
    /* Guest RAM held beside a window chunk, which open never leaves. */
    static void hold_guest_ram_too(BlobChunk &chunk, std::uint64_t bytes)
    {
        CHECK_STATUS(chunk.guest_ram_.allocate(bytes), Status::ok);
        CHECK_STATUS(chunk.guest_ram_.lock(), Status::ok);
    }
    static HostWindowChunk  &window(BlobChunk &chunk) { return chunk.window_; }
    static HostVisibleChunk &guest_ram(BlobChunk &chunk) { return chunk.guest_ram_; }
};

struct HostWindowChunkProbe {
    static void claim_bytes(HostWindowChunk &chunk, std::uint64_t bytes) { chunk.bytes_ = bytes; }
};

struct HostVisibleChunkProbe {
    static void claim_pages(HostVisibleChunk &chunk, std::uint64_t pages) { chunk.pages_ = pages; }
};

}  // namespace av

using av::BlobChunk;
using av::BlobChunkProbe;
using av::GuestConnection;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;

namespace {

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint32_t refused = 0xC0000001u;
/* The connection's device and queue, the transport's four allocations, its DMA
 * mapping and its context, with the adapter. */
constexpr std::uint32_t link_objects = 9u;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), staging_{},
          transport_(connection_, SubApi::opengl, 12u, reply_bytes, staging_, sizeof staging_),
          chunk_(connection_, transport_)
    {
        kmt_.add_adapter(rvk, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
    }

    MockKmt   &kmt() { return kmt_; }
    BlobChunk &chunk() { return chunk_; }

    void check_nothing_is_held()
    {
        CHECK(chunk_.invariants_hold());
        CHECK_EQ(chunk_.resource_id(), 0u);
        CHECK_EQ(chunk_.bytes(), 0u);
        CHECK(chunk_.view() == nullptr);
        CHECK_EQ(kmt_.open_objects(), link_objects);
        CHECK_EQ(kmt_.bindings(), 0u);
        CHECK_EQ(kmt_.window_chunks(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    /* The chunk is guest RAM: a host-visible allocation bound to the context,
     * seen through its lock. */
    void check_guest_ram(std::uint64_t bytes)
    {
        CHECK(chunk_.invariants_hold());
        CHECK_EQ(kmt_.bindings(), 1u);
        CHECK_EQ(kmt_.window_chunks(), 0u);
        const std::uint32_t allocation = kmt_.bound_allocation(0u);
        CHECK_EQ(kmt_.allocation_kind(allocation), static_cast<std::uint16_t>(av::AllocationKind::host_visible_memory));
        CHECK_EQ(chunk_.resource_id(), MockKmt::first_chunk_resource);
        CHECK_EQ(chunk_.bytes(), bytes);
        CHECK(chunk_.view() == kmt_.view_of(allocation));
    }

private:
    MockKmt         kmt_;
    std::uint32_t   adapter_;
    GuestConnection connection_;
    std::uint8_t    staging_[1024];
    KmtTransport    transport_;
    BlobChunk       chunk_;
};

}  // namespace

TEST(blob_chunk, takes_the_window_when_the_kernel_grants_it)
{ Rig rig;
  rig.kmt().open_host_window(64u * mib);
  const std::uint32_t allocations = rig.kmt().calls(MockKmt::Call::create_allocation);
  CHECK_STATUS(rig.chunk().open(16u * mib), Status::ok);
  CHECK(rig.chunk().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), allocations);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_EQ(rig.kmt().bindings(), 0u);
  CHECK_EQ(rig.chunk().resource_id(), MockKmt::first_window_resource);
  CHECK_EQ(rig.chunk().bytes(), 16u * mib);
  CHECK(rig.chunk().view() == rig.kmt().window_base());
  rig.chunk().view()[16u * mib - 1u] = 0x3Cu;
  CHECK_EQ(rig.kmt().window_base()[16u * mib - 1u], 0x3Cu);
  CHECK_STATUS(rig.chunk().open(16u * mib), Status::invalid_state);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_STATUS(rig.chunk().release(), Status::ok);
  rig.check_nothing_is_held(); }

/* A device with no window, a window without room and a kernel with no free
 * chunk slot each leave the chunk to guest RAM, which is then the chunk. */
TEST(blob_chunk, falls_back_to_guest_ram_for_a_chunk_the_window_cannot_hold)
{ { Rig rig;
    const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
    CHECK_STATUS(rig.chunk().open(16u * mib), Status::ok);
    CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 2u);   /* the window's open, then the bind */
    rig.check_guest_ram(16u * mib);
    CHECK_STATUS(rig.chunk().open(16u * mib), Status::invalid_state);
    CHECK_EQ(rig.kmt().bindings(), 1u);
    CHECK_STATUS(rig.chunk().release(), Status::ok);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.kmt().open_host_window(8u * mib);
    CHECK_STATUS(rig.chunk().open(16u * mib), Status::ok);
    rig.check_guest_ram(16u * mib);
    CHECK_STATUS(rig.chunk().release(), Status::ok);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.kmt().open_host_window(64u * mib);
    rig.kmt().refuse_next_window_open(MockKmt::invalid_parameter);
    CHECK_STATUS(rig.chunk().open(16u * mib), Status::ok);
    rig.check_guest_ram(16u * mib);
    CHECK_STATUS(rig.chunk().release(), Status::ok);
    /* The window has room again: the next chunk is its. */
    CHECK_STATUS(rig.chunk().open(16u * mib), Status::ok);
    CHECK_EQ(rig.kmt().window_chunks(), 1u);
    CHECK_STATUS(rig.chunk().release(), Status::ok);
    rig.check_nothing_is_held(); } }

/* The kernel said yes and wrote something that is no answer: it may hold a
 * chunk of the window for this context, so that is the answer, and no guest
 * RAM is taken on top of it. */
TEST(blob_chunk, a_garbled_window_answer_is_not_covered_with_guest_ram)
{ Rig rig;
  rig.kmt().open_host_window(64u * mib);
  const std::uint32_t allocations = rig.kmt().calls(MockKmt::Call::create_allocation);
  rig.kmt().zero_next(MockKmt::Call::escape);
  CHECK_STATUS(rig.chunk().open(16u * mib), Status::malformed);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), allocations);
  rig.check_nothing_is_held(); }

/* When guest RAM is the fallback, its refusal at any step is the answer, with
 * nothing held. */
TEST(blob_chunk, a_guest_ram_refusal_at_each_step_leaves_nothing_held)
{ struct Case { MockKmt::Call call; std::uint32_t nth; };
  const Case cases[] = {{MockKmt::Call::create_allocation, 1u}, {MockKmt::Call::make_resident, 1u},
                        {MockKmt::Call::lock, 1u}, {MockKmt::Call::escape, 2u}};
  for (const Case &one : cases) {
      Rig rig;
      rig.kmt().fail_at(one.call, one.nth, refused);
      CHECK_STATUS(rig.chunk().open(16u * mib), Status::invalid_state);
      rig.check_nothing_is_held();
      CHECK_STATUS(rig.chunk().open(16u * mib), Status::ok);
      rig.check_guest_ram(16u * mib);
      CHECK_STATUS(rig.chunk().release(), Status::ok);
  } }

/* Release answers the refusal of whichever kind was held, and holds nothing
 * after. */
TEST(blob_chunk, release_answers_the_refusal_of_the_kind_held)
{ { Rig rig;
    rig.kmt().open_host_window(64u * mib);
    CHECK_STATUS(rig.chunk().open(mib), Status::ok);
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);
    CHECK_STATUS(rig.chunk().release(), Status::invalid_state);
    CHECK(rig.chunk().invariants_hold());
    CHECK(rig.chunk().view() == nullptr); }
  { Rig rig;
    CHECK_STATUS(rig.chunk().open(mib), Status::ok);
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, refused);   /* the unbind */
    CHECK_STATUS(rig.chunk().release(), Status::invalid_state);
    CHECK(rig.chunk().invariants_hold());
    CHECK_EQ(rig.chunk().resource_id(), 0u);
    CHECK_EQ(rig.kmt().open_objects(), link_objects); } }

TEST(blob_chunk, invariants_refuse_both_kinds_at_once_and_either_kinds_own_breach)
{ { Rig rig;
    rig.kmt().open_host_window(64u * mib);
    CHECK_STATUS(rig.chunk().open(mib), Status::ok);
    CHECK(rig.chunk().invariants_hold());
    BlobChunkProbe::hold_guest_ram_too(rig.chunk(), mib);
    CHECK(!rig.chunk().invariants_hold());
    CHECK_STATUS(rig.chunk().release(), Status::ok);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.kmt().open_host_window(64u * mib);
    CHECK_STATUS(rig.chunk().open(mib), Status::ok);
    av::HostWindowChunkProbe::claim_bytes(BlobChunkProbe::window(rig.chunk()), mib + 1u);
    CHECK(!rig.chunk().invariants_hold());
    av::HostWindowChunkProbe::claim_bytes(BlobChunkProbe::window(rig.chunk()), mib);
    CHECK(rig.chunk().invariants_hold());
    CHECK_STATUS(rig.chunk().release(), Status::ok);
    rig.check_nothing_is_held(); }
  { Rig rig;
    CHECK_STATUS(rig.chunk().open(mib), Status::ok);
    av::HostVisibleChunkProbe::claim_pages(BlobChunkProbe::guest_ram(rig.chunk()), 0u);
    CHECK(!rig.chunk().invariants_hold());
    av::HostVisibleChunkProbe::claim_pages(BlobChunkProbe::guest_ram(rig.chunk()), mib / MockKmt::page_bytes);
    CHECK(rig.chunk().invariants_hold());
    CHECK_STATUS(rig.chunk().release(), Status::ok);
    rig.check_nothing_is_held(); } }

#include "harness/framework/test_registry.hpp"
#include "harness/transport/roomed_transport.hpp"
#include "icd/common/arena_region.hpp"
#include "proto/client/client_blob_arena.hpp"

#include <cstdint>

using av::ClientBlobArena;
using av::ClientBlobLease;
using av::icd::ArenaRegion;

namespace {

alignas(4096) std::uint8_t memory[8192];

class RegionTransport final : public av::RoomedTransport {
public:
    RegionTransport() : av::RoomedTransport(4096u, 4096u), opened_(0u), closed_(0u), refuse_(false), last_closed_() {}

    av::WireWriter request_writer() override { return av::WireWriter(nullptr, 0u); }
    [[nodiscard]] av::TransportResult send_async(std::uint32_t, av::WireWriter &) override { return av::TransportResult::failed; }
    [[nodiscard]] av::TransportResult send_sync(std::uint32_t, av::WireWriter &, av::WireReader &) override
    {
        return av::TransportResult::failed;
    }
    void             end_call() override {}
    av::WireArena   &decode_arena() override { return arena_; }
    ClientBlobArena &blobs() override { return blobs_; }

    ClientBlobLease open_region(std::uint64_t bytes) override
    {
        opened_++;
        if (refuse_ || bytes > sizeof memory) return ClientBlobLease();
        return ClientBlobLease(memory, std::uint64_t{5} << 26u, static_cast<std::size_t>(bytes));
    }
    void close_region(const ClientBlobLease &region) override
    {
        closed_++;
        last_closed_ = region;
    }

    std::uint32_t   opened_;
    std::uint32_t   closed_;
    bool            refuse_;
    ClientBlobLease last_closed_;
    av::WireArena   arena_{nullptr, 0u};
    ClientBlobArena blobs_;
};

}

TEST(arena_region, an_unopened_region_holds_nothing)
{ ArenaRegion region;
  CHECK(!region.held());
  CHECK(region.view() == nullptr);
  CHECK_EQ(region.bytes(), 0u);
  CHECK(region.invariants_hold());
  region.close(); }

TEST(arena_region_open, the_transport_s_lease_is_the_region_s_view_offset_and_size)
{ RegionTransport wire;
  ArenaRegion     region;
  CHECK(region.open(wire, 4096u));
  CHECK(region.held());
  CHECK(region.view() == memory);
  CHECK_EQ(region.offset(), std::uint64_t{5} << 26u);
  CHECK_EQ(region.bytes(), 4096u);
  CHECK(region.invariants_hold());
  region.close(); }

TEST(arena_region_open, a_second_open_an_empty_open_or_a_refusal_leaves_the_region_as_it_was)
{ RegionTransport wire;
  ArenaRegion     region;
  CHECK(!region.open(wire, 0u));
  CHECK_EQ(wire.opened_, 0u);
  wire.refuse_ = true;
  CHECK(!region.open(wire, 4096u));
  CHECK(!region.held());
  CHECK(region.invariants_hold());
  wire.refuse_ = false;
  CHECK(region.open(wire, 4096u));
  CHECK(!region.open(wire, 8192u));
  CHECK_EQ(region.bytes(), 4096u);
  CHECK_EQ(wire.opened_, 2u);
  region.close(); }

TEST(arena_region_close, hands_the_lease_back_to_the_transport_that_made_it_exactly_once)
{ RegionTransport wire;
  ArenaRegion     region;
  CHECK(region.open(wire, 4096u));
  region.close();
  CHECK_EQ(wire.closed_, 1u);
  CHECK_EQ(wire.last_closed_.offset(), std::uint64_t{5} << 26u);
  CHECK(!region.held());
  CHECK(region.view() == nullptr);
  region.close();
  CHECK_EQ(wire.closed_, 1u);
  CHECK(region.invariants_hold()); }

TEST(arena_region_close, a_region_goes_back_when_it_goes_out_of_scope)
{ RegionTransport wire;
  { ArenaRegion region;
    CHECK(region.open(wire, 4096u)); }
  CHECK_EQ(wire.closed_, 1u); }

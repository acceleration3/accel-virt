/* A device's host-visible memory (research.md §7): each allocation is a range
 * of whole pages in a 64 MiB chunk - or a chunk of its own size when larger -
 * imported with IMPORT_BLOB as the memory type asked for, and named by the
 * device memory the host answered. A chunk is bound when the first range needs
 * it and given back when its last range is freed. Every step refused leaves
 * nothing behind, and invariants hold after every change. The host is played
 * through ScriptedHost, and each IMPORT_BLOB is read back off MockKmt's ring. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/open_connection.hpp"
#include "icd/vulkan/chunk_allocator.hpp"
#include "icd/vulkan/chunk_placement.hpp"
#include "icd/vulkan/chunk_space.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::BlobImportRequest;
using av::CommandHeader;
using av::MockKmt;
using av::OpenConnection;
using av::Status;
using av::icd::ChunkAllocator;
using av::icd::ChunkPlacement;
using av::icd::ChunkSpace;

namespace av::icd {

struct ChunkAllocatorProbe {
    static void set_chunks(ChunkAllocator &allocator, std::uint32_t count) { allocator.chunk_count_ = count; }
    static void set_placements(ChunkAllocator &allocator, std::uint32_t count) { allocator.placement_count_ = count; }
    static ChunkSpace *&chunk(ChunkAllocator &allocator, std::uint32_t index) { return allocator.chunks_[index]; }
};

}  // namespace av::icd

using av::icd::ChunkAllocatorProbe;

namespace {

constexpr std::uint64_t page = ChunkSpace::page_bytes;
constexpr std::uint64_t chunk_bytes = ChunkAllocator::chunk_pages * page;
constexpr std::uint64_t memory = 0x0000700000000040ull;
constexpr std::uint32_t host_visible_type = 3u;
constexpr std::uint32_t refused_type = 4u;   /* the host's IMPORT_BLOB status for a type it will not import */

class Rig {
public:
    Rig() : allocator_(rig_.platform(), rig_.connection()), imports_(0u) {}

    OpenConnection &rig() { return rig_; }
    MockKmt        &kmt() { return rig_.kmt(); }
    ChunkAllocator &allocator() { return allocator_; }

    /* An allocation the host imports as the next device memory. */
    ChunkPlacement *allocate(std::uint64_t bytes, std::uint32_t type = host_visible_type)
    {
        rig_.host().blob_import(0u, memory + imports_);
        ChunkPlacement *placed = nullptr;
        CHECK_STATUS(allocator_.allocate(bytes, type, placed), Status::ok);
        CHECK(allocator_.invariants_hold());
        CHECK(placed != nullptr);
        CHECK_EQ(placed->memory(), memory + imports_);
        CHECK(allocator_.find(memory + imports_) == placed);
        imports_++;
        return placed;
    }

    void free(ChunkPlacement *placed)
    {
        rig_.host().nop();
        CHECK_STATUS(allocator_.free(*placed), Status::ok);
        CHECK(allocator_.invariants_hold());
    }

    /* The last command on the ring, as an IMPORT_BLOB's body. */
    BlobImportRequest last_import()
    {
        const std::uint32_t last = kmt().commands() - 1u;
        CHECK_EQ(kmt().command_id(last), CommandHeader::import_blob_id);
        BlobImportRequest request;
        CHECK_STATUS(request.decode(kmt().command_bytes(last) + CommandHeader::encoded_size,
                                    kmt().command_size(last) - CommandHeader::encoded_size),
                     Status::ok);
        return request;
    }

    void check_nothing_held()
    {
        CHECK_EQ(allocator_.chunks(), 0u);
        CHECK_EQ(allocator_.placements(), 0u);
        CHECK_EQ(kmt().bindings(), 0u);
        CHECK_EQ(kmt().open_objects(), OpenConnection::link_objects);
        CHECK_EQ(kmt().misuses(), 0u);
        CHECK(allocator_.invariants_hold());
    }

private:
    OpenConnection rig_;
    ChunkAllocator allocator_;
    std::uint32_t  imports_;
};

}  // namespace

TEST(chunk_allocator, holds_nothing_until_asked)
{ Rig rig;
  CHECK(rig.allocator().invariants_hold());
  CHECK(rig.allocator().find(memory) == nullptr);
  CHECK(rig.allocator().find(0u) == nullptr);
  rig.check_nothing_held();
  /* The null memory is never an allocation's. */
  static_cast<void>(rig.allocate(page));
  CHECK(rig.allocator().find(0u) == nullptr); }

/* The first allocation binds a 64 MiB chunk and imports its first pages as
 * the type asked; the view is the chunk's own first byte. */
TEST(chunk_allocator, a_first_allocation_binds_a_chunk_and_imports_its_pages)
{ Rig rig;
  ChunkPlacement *placed = rig.allocate(page + 1u);
  CHECK_EQ(rig.allocator().chunks(), 1u);
  CHECK_EQ(rig.allocator().placements(), 1u);
  CHECK_EQ(rig.kmt().bindings(), 1u);
  CHECK_EQ(rig.kmt().allocation_pages(rig.kmt().bound_allocation(0u)), ChunkAllocator::chunk_pages);
  const BlobImportRequest request = rig.last_import();
  CHECK_EQ(request.resource_id(), MockKmt::first_chunk_resource);
  CHECK_EQ(request.offset(), 0u);
  CHECK_EQ(request.size(), 2u * page);
  CHECK_EQ(request.memory_type(), host_visible_type);
  CHECK_EQ(placed->offset(), 0u);
  CHECK_EQ(placed->bytes(), 2u * page);
  CHECK(placed->view() == placed->space().view());
  CHECK(placed->invariants_hold()); }

/* Later allocations share the chunk, each its own import at its own pages;
 * freeing one releases its import and leaves the chunk for the others; freeing
 * the last gives the chunk back. */
TEST(chunk_allocator, allocations_share_a_chunk_until_the_last_is_freed)
{ Rig rig;
  ChunkPlacement *first = rig.allocate(page);
  ChunkPlacement *second = rig.allocate(3u * page);
  CHECK_EQ(rig.last_import().offset(), page);
  CHECK(second->view() == first->view() + page);
  CHECK_EQ(rig.allocator().chunks(), 1u);
  const std::uint32_t commands = rig.kmt().commands();
  rig.free(first);
  CHECK_EQ(rig.kmt().command_id(commands), CommandHeader::release_blob_id);
  CHECK(rig.allocator().find(memory) == nullptr);
  CHECK(rig.allocator().find(memory + 1u) == second);
  CHECK_EQ(rig.allocator().chunks(), 1u);
  CHECK_EQ(rig.kmt().bindings(), 1u);
  /* The hole the first left is taken again. */
  ChunkPlacement *third = rig.allocate(page);
  CHECK_EQ(third->offset(), 0u);
  rig.free(second);
  rig.free(third);
  rig.check_nothing_held(); }

/* A request of exactly a chunk fills one; a byte more is a chunk of its own
 * size; a request that does not fit the free pages of a chunk opens another. */
TEST(chunk_allocator, a_chunk_s_worth_fills_one_and_a_byte_more_gets_its_own)
{ Rig rig;
  ChunkPlacement *whole = rig.allocate(chunk_bytes);
  CHECK_EQ(rig.allocator().chunks(), 1u);
  ChunkPlacement *larger = rig.allocate(chunk_bytes + 1u);
  CHECK_EQ(rig.allocator().chunks(), 2u);
  CHECK_EQ(larger->bytes(), chunk_bytes + page);
  CHECK_EQ(larger->space().pages(), ChunkAllocator::chunk_pages + 1u);
  CHECK_EQ(rig.kmt().allocation_pages(rig.kmt().bound_allocation(1u)), ChunkAllocator::chunk_pages + 1u);
  CHECK_EQ(rig.last_import().resource_id(), MockKmt::first_chunk_resource + 1u);
  ChunkPlacement *small = rig.allocate(page);
  CHECK_EQ(rig.allocator().chunks(), 3u);
  CHECK(&small->space() != &whole->space());
  rig.free(whole);
  CHECK_EQ(rig.allocator().chunks(), 2u);
  ChunkPlacement *again = rig.allocate(page);
  CHECK(&again->space() == &small->space());
  CHECK_EQ(again->offset(), page);
  rig.free(larger);
  rig.free(small);
  rig.free(again);
  rig.check_nothing_held(); }

/* Zero bytes are no allocation, and a size whose pages do not fit 64 bits of
 * bytes is none either: neither is asked of anyone. */
TEST(chunk_allocator, a_size_that_is_no_allocation_is_refused_unasked)
{ Rig rig;
  const std::uint64_t commands = rig.kmt().commands();
  const std::uint64_t sizes[] = {0u, ~std::uint64_t{0}, ~std::uint64_t{0} - page + 2u};
  for (const std::uint64_t bytes : sizes) {
      ChunkPlacement *placed = reinterpret_cast<ChunkPlacement *>(1);
      CHECK_STATUS(rig.allocator().allocate(bytes, host_visible_type, placed), Status::invalid_parameter);
      CHECK(placed == nullptr);
  }
  CHECK_EQ(rig.kmt().commands(), commands);
  rig.check_nothing_held(); }

/* The chunk refused at any of its steps: nothing is imported and nothing
 * held. */
TEST(chunk_allocator, a_chunk_refused_at_any_step_leaves_nothing)
{ const MockKmt::Call steps[] = {MockKmt::Call::create_allocation, MockKmt::Call::make_resident,
                                 MockKmt::Call::wait_for_fence, MockKmt::Call::lock, MockKmt::Call::escape};
  for (const MockKmt::Call step : steps) {
      Rig rig;
      const std::uint32_t commands = rig.kmt().commands();
      rig.kmt().fail_at(step, 1u, MockKmt::invalid_parameter);
      ChunkPlacement *placed = nullptr;
      CHECK(rig.allocator().allocate(page, host_visible_type, placed) != Status::ok);
      CHECK(placed == nullptr);
      CHECK_EQ(rig.kmt().commands(), commands);
      rig.check_nothing_held();
  }
}

/* The host refusing the import - or answering nothing, or garbage - gives the
 * chunk it opened for it back; a chunk other allocations use stays. */
TEST(chunk_allocator, an_import_the_host_refuses_leaves_nothing_it_made)
{ Rig rig;
  ChunkPlacement *placed = nullptr;
  rig.rig().host().blob_import(refused_type, 0u);
  CHECK_STATUS(rig.allocator().allocate(page, 1u, placed), Status::invalid_parameter);
  CHECK(placed == nullptr);
  rig.check_nothing_held();
  rig.rig().host().nop();
  CHECK_STATUS(rig.allocator().allocate(page, 1u, placed), Status::not_supported);
  rig.check_nothing_held();

  ChunkPlacement *kept = rig.allocate(page);
  rig.rig().host().blob_import(refused_type, 0u);
  CHECK_STATUS(rig.allocator().allocate(page, 1u, placed), Status::invalid_parameter);
  CHECK_EQ(rig.allocator().chunks(), 1u);
  CHECK_EQ(rig.allocator().placements(), 1u);
  CHECK(kept->space().placed() == 1u);
  /* The refused pages are free again. */
  ChunkPlacement *next = rig.allocate(page);
  CHECK_EQ(next->offset(), page);
  rig.free(kept);
  rig.free(next);
  rig.check_nothing_held(); }

/* No memory for the bookkeeping is no allocation, with nothing held. */
TEST(chunk_allocator, no_memory_for_its_records_is_refused_whole)
{ for (std::uint32_t nth = 1u; nth <= 2u; nth++) {
      Rig rig;
      rig.rig().platform().fail_allocation_at(nth);
      ChunkPlacement *placed = nullptr;
      CHECK_STATUS(rig.allocator().allocate(page, host_visible_type, placed), Status::no_memory);
      CHECK(placed == nullptr);
      rig.check_nothing_held();
  }
}

/* A context imports at most 256 blobs, and binds at most 64 chunks; neither
 * limit is asked of the host or the kernel. */
TEST(chunk_allocator, stops_at_a_context_s_imports)
{ Rig rig;
  ChunkPlacement *held = rig.allocate(page);
  const std::uint32_t commands = rig.kmt().commands();
  ChunkAllocatorProbe::set_placements(rig.allocator(), ChunkAllocator::max_placements);
  ChunkPlacement *placed = nullptr;
  CHECK_STATUS(rig.allocator().allocate(page, host_visible_type, placed), Status::no_memory);
  CHECK(placed == nullptr);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(held->space().placed(), 1u);
  ChunkAllocatorProbe::set_placements(rig.allocator(), 1u);
  rig.free(held);
  rig.check_nothing_held(); }

TEST(chunk_allocator, stops_at_a_context_s_chunks)
{ Rig rig;
  ChunkPlacement *full = rig.allocate(chunk_bytes);
  /* Every slot a full chunk: the one real one, named 64 times. */
  for (std::uint32_t index = 1u; index < ChunkAllocator::max_chunks; index++)
      ChunkAllocatorProbe::chunk(rig.allocator(), index) = ChunkAllocatorProbe::chunk(rig.allocator(), 0u);
  ChunkAllocatorProbe::set_chunks(rig.allocator(), ChunkAllocator::max_chunks);
  const std::uint32_t commands = rig.kmt().commands();
  const std::uint32_t allocations = rig.kmt().calls(MockKmt::Call::create_allocation);
  ChunkPlacement *placed = nullptr;
  CHECK_STATUS(rig.allocator().allocate(page, host_visible_type, placed), Status::no_memory);
  CHECK(placed == nullptr);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), allocations);
  ChunkAllocatorProbe::set_chunks(rig.allocator(), 1u);
  CHECK(rig.allocator().invariants_hold());
  rig.free(full);
  rig.check_nothing_held(); }

/* A release the host answers wrongly, or the transport cannot carry, is
 * reported, and the allocation and its chunk are gone all the same. */
TEST(chunk_allocator, a_failed_release_still_frees)
{ Rig rig;
  ChunkPlacement *placed = rig.allocate(page);
  rig.rig().host().blob_import(0u, 1u);   /* bytes where none belong */
  CHECK_STATUS(rig.allocator().free(*placed), Status::malformed);
  rig.check_nothing_held();

  placed = rig.allocate(page);
  rig.kmt().fail_at(MockKmt::Call::unlock, 1u, MockKmt::invalid_parameter);
  rig.rig().host().nop();
  CHECK(rig.allocator().free(*placed) != Status::ok);
  CHECK_EQ(rig.allocator().chunks(), 0u);
  CHECK_EQ(rig.allocator().placements(), 0u); }

/* A placement this allocator does not hold is not freed. */
TEST(chunk_allocator, frees_only_its_own_placements)
{ Rig rig;
  Rig other;
  ChunkPlacement *foreign = other.allocate(page);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_STATUS(rig.allocator().free(*foreign), Status::invalid_parameter);
  CHECK_EQ(rig.kmt().commands(), commands);
  other.free(foreign); }

/* Everything, released in one call: every import given back, every chunk
 * unbound. */
TEST(chunk_allocator, release_all_gives_everything_back)
{ Rig rig;
  static_cast<void>(rig.allocate(page));
  static_cast<void>(rig.allocate(chunk_bytes));
  static_cast<void>(rig.allocate(3u * page));
  CHECK_EQ(rig.allocator().chunks(), 2u);
  for (std::uint32_t release = 0u; release < 3u; release++) rig.rig().host().nop();
  CHECK_STATUS(rig.allocator().release_all(), Status::ok);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  rig.check_nothing_held();
  CHECK_STATUS(rig.allocator().release_all(), Status::ok); }

/* Every release is attempted when some are refused, and the first refusal is
 * the answer. */
TEST(chunk_allocator, release_all_answers_the_first_refusal_and_frees_everything)
{ Rig rig;
  static_cast<void>(rig.allocate(page));
  static_cast<void>(rig.allocate(page));
  rig.rig().host().blob_import(0u, 1u);
  rig.rig().host().cut_next(1u);
  rig.rig().host().blob_import(0u, 1u);
  CHECK_STATUS(rig.allocator().release_all(), Status::malformed);
  rig.check_nothing_held(); }

/* Destroyed with allocations held - the device went with its instance - it
 * gives every chunk back to the kernel without a host call. */
TEST(chunk_allocator, destroying_it_gives_the_chunks_back_unasked)
{ OpenConnection rig;
  std::uint32_t  commands = 0u;
  {
      ChunkAllocator allocator(rig.platform(), rig.connection());
      rig.host().blob_import(0u, memory);
      ChunkPlacement *placed = nullptr;
      CHECK_STATUS(allocator.allocate(page, host_visible_type, placed), Status::ok);
      commands = rig.kmt().commands();
  }
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.kmt().bindings(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), OpenConnection::link_objects);
  CHECK_EQ(rig.platform().live_allocations(), 1u); }

TEST(chunk_allocator, invariants_refuse_counts_that_do_not_match)
{ Rig rig;
  ChunkPlacement *placed = rig.allocate(page);
  ChunkAllocatorProbe::set_placements(rig.allocator(), 0u);
  CHECK(!rig.allocator().invariants_hold());
  ChunkAllocatorProbe::set_placements(rig.allocator(), 1u);
  CHECK(rig.allocator().invariants_hold());
  ChunkAllocatorProbe::set_chunks(rig.allocator(), 2u);
  CHECK(!rig.allocator().invariants_hold());
  ChunkAllocatorProbe::set_chunks(rig.allocator(), 1u);
  /* A chunk nothing is placed in is one that should have been given back. */
  ChunkSpace *const space = ChunkAllocatorProbe::chunk(rig.allocator(), 0u);
  CHECK_STATUS(space->remove(0u), Status::ok);
  CHECK(!rig.allocator().invariants_hold());
  std::uint64_t offset = 0u;
  CHECK_STATUS(space->place(1u, offset), Status::ok);
  CHECK(rig.allocator().invariants_hold());
  ChunkAllocatorProbe::set_placements(rig.allocator(), ChunkAllocator::max_placements + 1u);
  CHECK(!rig.allocator().invariants_hold());
  ChunkAllocatorProbe::set_placements(rig.allocator(), 1u);
  ChunkAllocatorProbe::set_chunks(rig.allocator(), ChunkAllocator::max_chunks + 1u);
  CHECK(!rig.allocator().invariants_hold());
  ChunkAllocatorProbe::set_chunks(rig.allocator(), 1u);
  /* A second allocation whose pages the chunk no longer holds. */
  ChunkPlacement *second = rig.allocate(page);
  CHECK_STATUS(space->remove(second->offset()), Status::ok);
  CHECK(!rig.allocator().invariants_hold());
  CHECK_STATUS(space->place(1u, offset), Status::ok);
  CHECK_EQ(offset, second->offset());
  CHECK(rig.allocator().invariants_hold());
  rig.free(second);
  rig.free(placed); }

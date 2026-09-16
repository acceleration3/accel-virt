/* One host-visible allocation in a chunk: its pages placed and imported, or
 * neither; released through RELEASE_BLOB with its pages given back whatever the
 * host says; dropped without a host call when its device is gone. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/open_connection.hpp"
#include "icd/vulkan/chunk_placement.hpp"
#include "icd/vulkan/chunk_space.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>

using av::CommandHeader;
using av::OpenConnection;
using av::Status;
using av::icd::ChunkPlacement;
using av::icd::ChunkSpace;

namespace av::icd {

struct ChunkPlacementProbe {
    static void set_pages(ChunkPlacement &placement, std::uint64_t pages) { placement.pages_ = pages; }
};

}  // namespace av::icd

using av::icd::ChunkPlacementProbe;

namespace {

constexpr std::uint64_t page = ChunkSpace::page_bytes;
constexpr std::uint64_t memory = 0x0000700000000040ull;

class Rig {
public:
    Rig() : space_(rig_.connection()), placement_(space_, rig_.connection().transport())
    { CHECK_STATUS(space_.open(16u), Status::ok); }

    OpenConnection &rig() { return rig_; }
    ChunkSpace     &space() { return space_; }
    ChunkPlacement &placement() { return placement_; }

private:
    OpenConnection rig_;
    ChunkSpace     space_;
    ChunkPlacement placement_;
};

}  // namespace

TEST(chunk_placement, is_nothing_until_placed)
{ Rig rig;
  CHECK(rig.placement().invariants_hold());
  CHECK_EQ(rig.placement().memory(), 0u);
  CHECK_EQ(rig.placement().bytes(), 0u);
  CHECK(rig.placement().view() == nullptr);
  CHECK(&rig.placement().space() == &rig.space());
  const std::uint32_t commands = rig.rig().kmt().commands();
  CHECK_STATUS(rig.placement().release(), Status::ok);
  CHECK_EQ(rig.rig().kmt().commands(), commands); }

TEST(chunk_placement, a_placement_is_its_pages_imported)
{ Rig rig;
  std::uint64_t taken = 0u;
  CHECK_STATUS(rig.space().place(2u, taken), Status::ok);
  rig.rig().host().blob_import(0u, memory);
  CHECK_STATUS(rig.placement().place(3u, 5u), Status::ok);
  CHECK(rig.placement().invariants_hold());
  CHECK_EQ(rig.placement().memory(), memory);
  CHECK_EQ(rig.placement().offset(), 2u * page);
  CHECK_EQ(rig.placement().bytes(), 3u * page);
  CHECK(rig.placement().view() == rig.space().view() + 2u * page);
  CHECK_EQ(rig.space().placed(), 2u);
  CHECK_STATUS(rig.placement().place(1u, 5u), Status::invalid_state);
  CHECK_EQ(rig.space().placed(), 2u);

  rig.rig().host().nop();
  CHECK_STATUS(rig.placement().release(), Status::ok);
  CHECK(rig.placement().invariants_hold());
  CHECK_EQ(rig.rig().kmt().command_id(rig.rig().kmt().commands() - 1u), CommandHeader::release_blob_id);
  CHECK_EQ(rig.placement().memory(), 0u);
  CHECK(rig.placement().view() == nullptr);
  CHECK_EQ(rig.space().placed(), 1u);
  CHECK_STATUS(rig.space().remove(0u), Status::ok); }

TEST(chunk_placement, pages_the_chunk_lacks_are_asked_of_no_one)
{ Rig rig;
  const std::uint32_t commands = rig.rig().kmt().commands();
  CHECK_STATUS(rig.placement().place(17u, 5u), Status::no_memory);
  CHECK_EQ(rig.rig().kmt().commands(), commands);
  CHECK(rig.placement().invariants_hold()); }

TEST(chunk_placement, a_refused_import_gives_its_pages_back)
{ Rig rig;
  rig.rig().host().blob_import(2u, 0u);
  CHECK_STATUS(rig.placement().place(16u, 5u), Status::invalid_parameter);
  CHECK(rig.placement().invariants_hold());
  CHECK(rig.space().empty());
  CHECK_EQ(rig.placement().memory(), 0u); }

TEST(chunk_placement, a_refused_release_still_gives_the_pages_back)
{ Rig rig;
  rig.rig().host().blob_import(0u, memory);
  CHECK_STATUS(rig.placement().place(1u, 5u), Status::ok);
  rig.rig().host().blob_import(0u, memory);
  CHECK_STATUS(rig.placement().release(), Status::malformed);
  CHECK(rig.space().empty());
  CHECK_EQ(rig.placement().memory(), 0u); }

/* Dropped while placed: the pages go back to the chunk, and the host is not
 * asked. */
TEST(chunk_placement, dropping_it_gives_the_pages_back_unasked)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(16u), Status::ok);
  std::uint32_t commands = 0u;
  {
      ChunkPlacement placement(space, rig.connection().transport());
      rig.host().blob_import(0u, memory);
      CHECK_STATUS(placement.place(4u, 5u), Status::ok);
      commands = rig.kmt().commands();
  }
  CHECK(space.empty());
  CHECK_EQ(rig.kmt().commands(), commands); }

TEST(chunk_placement, invariants_refuse_pages_without_an_import)
{ Rig rig;
  ChunkPlacementProbe::set_pages(rig.placement(), 1u);
  CHECK(!rig.placement().invariants_hold());
  ChunkPlacementProbe::set_pages(rig.placement(), 0u);
  CHECK(rig.placement().invariants_hold()); }

/* The guest's upload and readback heaps as ranges of one host heap imported
 * from the session arena's dma-buf. Importing needs vkd3d-proton on the host
 * GPU; what is judged here is everything refused before and without it. */
#include "executor/d3d/d3d12_arena_heaps.hpp"
#include "executor/guest_region.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12ArenaHeaps;

namespace {

std::uint8_t arena[8192];

}  // namespace

TEST(d3d12_arena_heaps_add, a_range_outside_the_region_is_refused_before_anything_is_imported)
{ av::GuestRegion region;
  region.take(arena, sizeof arena);
  av::D3d12HostDevice device;
  D3d12ArenaHeaps heaps;
  CHECK(!heaps.add(device, region, 1u, 4096u, 4097u));
  CHECK(!heaps.add(device, region, 1u, sizeof arena, 1u));
  CHECK(heaps.heap() == nullptr); }

/* A device vkd3d-proton never opened imports nothing, so no heap is added. */
TEST(d3d12_arena_heaps_add, a_device_that_cannot_import_the_arena_adds_nothing)
{ av::GuestRegion region;
  region.take(arena, sizeof arena);
  av::D3d12HostDevice device;
  D3d12ArenaHeaps heaps;
  CHECK(!heaps.add(device, region, 1u, 0u, 4096u));
  CHECK(heaps.heap() == nullptr);
  std::uint64_t at = 5u;
  CHECK(!heaps.place(1u, 0u, 16u, at));
  CHECK(at == 0u);
  CHECK(!heaps.remove(1u));
  heaps.close();
  heaps.close();
  CHECK(heaps.heap() == nullptr); }

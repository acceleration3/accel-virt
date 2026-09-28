/* One context's view of guest memory. Every offset and length asked of it came
 * off the wire, and OpenGL's decoder asks at more than a thousand sites. */
#include "device/executor/guest_region.hpp"
#include "harness/executor/guest_region_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using av::GuestRegion;
using av::GuestRegionProbe;

namespace {

std::uint8_t mapping[4096];

/* Whether a PROT_NONE mapping - what is left of a reservation - ends where
 * the region begins or begins where its pages end. */
bool reservation_beside(const std::uint8_t *base, std::uint64_t mapped_bytes)
{
    std::FILE *const maps = std::fopen("/proc/self/maps", "r");
    if (maps == nullptr) return true;
    const auto low = reinterpret_cast<std::uintptr_t>(base);
    const std::uintptr_t high = low + mapped_bytes;
    unsigned long start = 0u, end = 0u;
    char permissions[8] = {};
    char line[512];
    bool beside = false;
    while (std::fgets(line, sizeof line, maps) != nullptr) {
        if (std::sscanf(line, "%lx-%lx %7s", &start, &end, permissions) != 3) continue;
        if (std::string_view(permissions) == "---p" && (end == low || start == high)) beside = true;
    }
    std::fclose(maps);
    return beside;
}

}  // namespace

TEST(guest_region, answers_inside_the_mapping_it_was_given)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  CHECK_EQ(region.length(), sizeof mapping);
  CHECK(region.at(0u, sizeof mapping) == mapping);
  CHECK(region.at(16u, 32u) == mapping + 16u);
  /* The empty ask at the very end is inside it, which is what a zero-length
   * array on the wire looks like. */
  CHECK(region.at(sizeof mapping, 0u) == mapping + sizeof mapping);
  CHECK(region.invariants_hold()); }

TEST(guest_region, refuses_anything_past_the_end)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  CHECK(region.at(0u, sizeof mapping + 1u) == nullptr);
  CHECK(region.at(sizeof mapping, 1u) == nullptr);
  CHECK(region.at(sizeof mapping + 1u, 0u) == nullptr);
  CHECK(region.at(16u, sizeof mapping) == nullptr); }

/* The numbers a guest would pick to make a bounds check pass by wrapping. */
TEST(guest_region, refuses_a_length_that_would_wrap)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  CHECK(region.at(~std::uint64_t{0}, 1u) == nullptr);
  CHECK(region.at(1u, ~std::uint64_t{0}) == nullptr);
  CHECK(region.at(~std::uint64_t{0}, ~std::uint64_t{0}) == nullptr); }

/* A context with no region refuses every ask, which is what makes a command
 * that names one refused rather than guessed at. */
TEST(guest_region, with_no_mapping_it_answers_nothing)
{ GuestRegion region;
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 0u) == nullptr);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK(region.invariants_hold());

  region.take(nullptr, 4096u);
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK(region.invariants_hold()); }

TEST(guest_region, giving_it_back_leaves_nothing_to_answer_with)
{ GuestRegion region;
  region.take(mapping, sizeof mapping);
  region.give_back();
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK(region.invariants_hold()); }

/* A length with nothing behind it is a region that would answer bounds checks
 * for memory nobody mapped, if `at` ever stopped checking the base first. */
TEST(guest_region_invariants, a_length_with_no_mapping_behind_it_breaks_them)
{ GuestRegion region;
  GuestRegionProbe(region).set_length(sizeof mapping);
  CHECK(!region.invariants_hold()); }

/* GuestRegion::take_shared: the region taken as a dma-buf is mapped here, and
 * the mapping and descriptor are the region's until it gives them back. What
 * cannot be mapped is refused with the descriptor still the caller's. A memfd
 * stands in for the dma-buf: both are shareable files mmap maps. */
TEST(guest_region_take_shared, maps_the_descriptor_and_gives_both_back)
{ const int shared = memfd_create("guest-region", 0);
  CHECK(shared >= 0);
  CHECK_EQ(ftruncate(shared, 8192), 0);
  const std::uint8_t written = 0x5Au;
  CHECK_EQ(pwrite(shared, &written, 1u, 4100), 1);
  GuestRegion region;
  CHECK(region.take_shared(shared, 8192u));
  CHECK_EQ(region.descriptor(), shared);
  CHECK_EQ(region.length(), 8192u);
  const std::uint8_t *const byte = region.at(4100u, 1u);
  CHECK(byte != nullptr);
  CHECK_EQ(*byte, 0x5Au);
  CHECK(region.at(8192u, 1u) == nullptr);
  CHECK(region.invariants_hold());
  region.give_back();
  CHECK_EQ(region.descriptor(), -1);
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 1u) == nullptr);
  /* The descriptor went with the region. */
  CHECK_EQ(fcntl(shared, F_GETFD), -1); }

/* A dma-buf is mapped where a Direct3D 12 heap over part of it can be made:
 * mmap alone places a mapping at page alignment, and a 1 GiB arena landed
 * where no heap in it could be imported. The address space is moved a page
 * between takes so each lands somewhere else; the reservation each was
 * aligned in is given back around it, and give_back leaves nothing mapped. */
TEST(guest_region_take_shared, maps_at_the_base_alignment_and_keeps_none_of_the_reservation)
{ const std::uint64_t page = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
  const std::uint64_t length = 3u * page + 100u;   /* not a whole number of pages */
  void *shifts[16] = {};
  for (std::uint32_t round = 0u; round < 16u; round++) {
      shifts[round] = mmap(nullptr, page, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      CHECK(shifts[round] != MAP_FAILED);
      const int shared = memfd_create("guest-region", 0);
      CHECK(shared >= 0);
      CHECK_EQ(ftruncate(shared, static_cast<off_t>(length)), 0);
      const std::uint8_t written = static_cast<std::uint8_t>(round + 1u);
      CHECK_EQ(pwrite(shared, &written, 1u, static_cast<off_t>(length - 50u)), 1);
      GuestRegion region;
      CHECK(region.take_shared(shared, length));
      const std::uint8_t *const base = region.at(0u, 1u);
      CHECK(base != nullptr);
      CHECK_EQ(reinterpret_cast<std::uintptr_t>(base) % GuestRegion::base_alignment, 0u);
      CHECK_EQ(*region.at(length - 50u, 1u), written);
      CHECK(!reservation_beside(base, 4u * page));
      region.give_back();
  }
  for (void *shift : shifts) munmap(shift, page); }

TEST(guest_region_take_shared, refuses_what_cannot_be_mapped_and_leaves_the_descriptor_alone)
{ GuestRegion region;
  CHECK(!region.take_shared(-1, 4096u));
  const int shared = memfd_create("guest-region", 0);
  CHECK(shared >= 0);
  CHECK(!region.take_shared(shared, 0u));
  CHECK(!region.take_shared(shared, std::uint64_t{1} << 62u));   /* no address space to reserve */
  int pipes[2] = {-1, -1};
  CHECK_EQ(pipe(pipes), 0);
  CHECK(!region.take_shared(pipes[0], 4096u));   /* a pipe maps nothing */
  CHECK_EQ(region.descriptor(), -1);
  CHECK(region.at(0u, 0u) == nullptr);
  /* Each descriptor is still the caller's to close. */
  CHECK(fcntl(shared, F_GETFD) != -1);
  CHECK(fcntl(pipes[0], F_GETFD) != -1);
  close(shared);
  close(pipes[0]);
  close(pipes[1]);
  region.give_back(); }

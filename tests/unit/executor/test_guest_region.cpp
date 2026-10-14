/* One context's view of host memory it shares with the guest: an import of
 * its GPU's memory source, read and written through the source's mapping.
 * Every offset and length asked of it came off the wire, and OpenGL's decoder
 * asks at more than a thousand sites. */
#include "executor/exported_memory.hpp"
#include "executor/guest_region.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/guest_region_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::ExportedMemory;
using av::GuestRegion;
using av::GuestRegionProbe;
using av::ImportedBlob;
using av::SourceMemory;
using av::Status;

namespace {

alignas(4096) std::uint8_t mapping[8192];

/* An import of memory the source mapped at `pointer`, `size` bytes. */
class Import {
public:
    Import(std::uint8_t *pointer = mapping, std::uint64_t size = sizeof mapping)
        : memory(1u, 0x40u, 0u, 1u, ExportedMemory(0x99u, -1, pointer, size)), blob()
    {
        static_cast<void>(blob.take(5u, memory));
    }

    SourceMemory memory;
    ImportedBlob blob;
};

std::uint64_t address_of(const void *pointer)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

}  // namespace

/* The region's address names the import on the wire: both what the host API
 * made of it and what the guest calls it. */
TEST(guest_region_adopt, an_import_becomes_the_region_and_is_named_by_its_address)
{ GuestRegion region;
  Import      import;
  CHECK(region.memory() == nullptr);
  CHECK(region.adopt(import.blob));
  CHECK(region.memory() == &import.memory);
  CHECK_EQ(region.length(), sizeof mapping);
  CHECK(region.at(0u, sizeof mapping) == mapping);
  CHECK_EQ(import.blob.object(), address_of(mapping));
  CHECK_EQ(import.blob.wire_handle(), address_of(mapping));
  CHECK(region.invariants_hold()); }

/* One region at a time: a second would leave the first unreachable with what
 * was made over it, and the second import is left as it was. */
TEST(guest_region_adopt, a_second_import_is_refused_untouched)
{ GuestRegion region;
  Import      first;
  Import      second(mapping + 4096, 4096u);
  CHECK(region.adopt(first.blob));
  CHECK(!region.adopt(second.blob));
  CHECK_EQ(second.blob.object(), 0u);
  CHECK_EQ(second.blob.wire_handle(), 0u);
  CHECK(region.memory() == &first.memory);
  CHECK_EQ(region.length(), sizeof mapping);
  CHECK(region.invariants_hold()); }

/* Memory the source never mapped - no pointer, or nothing to map - is no
 * region at all. */
TEST(guest_region_adopt, memory_with_no_mapping_or_no_bytes_is_refused)
{ GuestRegion region;
  Import      unmapped(nullptr, 4096u);
  Import      empty(mapping, 0u);
  CHECK(!region.adopt(unmapped.blob));
  CHECK(!region.adopt(empty.blob));
  CHECK(region.memory() == nullptr);
  CHECK_EQ(region.length(), 0u);
  CHECK_EQ(unmapped.blob.object(), 0u);
  CHECK_EQ(empty.blob.object(), 0u);
  CHECK(region.at(0u, 0u) == nullptr);
  CHECK(region.invariants_hold()); }

/* Abandoning another import gives nothing back; abandoning the region's
 * leaves nothing to answer with and unnames the import. */
TEST(guest_region_adopt, only_the_region_s_own_import_gives_it_back)
{ GuestRegion region;
  Import      held;
  Import      other(mapping + 4096, 4096u);
  region.abandon(held.blob);
  CHECK(region.invariants_hold());
  CHECK(region.adopt(held.blob));
  region.abandon(other.blob);
  CHECK_EQ(region.length(), sizeof mapping);
  CHECK(region.memory() == &held.memory);
  region.abandon(held.blob);
  CHECK(region.memory() == nullptr);
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK_EQ(held.blob.object(), 0u);
  CHECK_EQ(held.blob.wire_handle(), 0u);
  CHECK(region.invariants_hold());
  /* And it takes the next one. */
  CHECK(region.adopt(other.blob));
  CHECK(region.at(0u, 1u) == mapping + 4096);
  region.abandon(other.blob); }

TEST(guest_region, answers_inside_the_mapping_it_was_given)
{ GuestRegion region;
  Import      import;
  CHECK(region.adopt(import.blob));
  CHECK(region.at(16u, 32u) == mapping + 16u);
  /* The empty ask at the very end is inside it, which is what a zero-length
   * array on the wire looks like. */
  CHECK(region.at(sizeof mapping, 0u) == mapping + sizeof mapping);
  CHECK(region.invariants_hold()); }

/* What a decoder resolved through `at` - the region's first byte to its last
 * - and nothing beside it: not the byte past the end, not one before the
 * start, and nothing at all with no region. */
TEST(guest_region, holds_only_pointers_into_the_mapping)
{ GuestRegion  region;
  std::uint8_t elsewhere = 0u;
  Import       import(mapping + 16, sizeof mapping - 32u);
  CHECK(!region.holds(mapping + 16));
  CHECK(region.adopt(import.blob));
  CHECK(region.holds(mapping + 16));
  CHECK(region.holds(mapping + sizeof mapping - 17u));
  CHECK(!region.holds(mapping + sizeof mapping - 16u));
  CHECK(!region.holds(mapping + 15));
  CHECK(!region.holds(&elsewhere));
  CHECK(!region.holds(nullptr));
  region.abandon(import.blob);
  CHECK(!region.holds(mapping + 16)); }

TEST(guest_region, refuses_anything_past_the_end)
{ GuestRegion region;
  Import      import;
  CHECK(region.adopt(import.blob));
  CHECK(region.at(0u, sizeof mapping + 1u) == nullptr);
  CHECK(region.at(sizeof mapping, 1u) == nullptr);
  CHECK(region.at(sizeof mapping + 1u, 0u) == nullptr);
  CHECK(region.at(16u, sizeof mapping) == nullptr); }

/* The numbers a guest would pick to make a bounds check pass by wrapping. */
TEST(guest_region, refuses_a_length_that_would_wrap)
{ GuestRegion region;
  Import      import;
  CHECK(region.adopt(import.blob));
  CHECK(region.at(~std::uint64_t{0}, 1u) == nullptr);
  CHECK(region.at(1u, ~std::uint64_t{0}) == nullptr);
  CHECK(region.at(~std::uint64_t{0}, ~std::uint64_t{0}) == nullptr);
  CHECK(region.at(8u, ~std::uint64_t{0} - 7u) == nullptr); }

/* A context with no region refuses every ask, which is what makes a command
 * that names one refused rather than guessed at. */
TEST(guest_region, with_no_region_it_answers_nothing)
{ GuestRegion region;
  CHECK_EQ(region.length(), 0u);
  CHECK(region.at(0u, 0u) == nullptr);
  CHECK(region.at(0u, 1u) == nullptr);
  CHECK(!region.publish(0u, 1u));
  CHECK(region.invariants_hold()); }

/* An error word or a fence value is stored whole: never outside the region,
 * never off its alignment. */
TEST(guest_region_publish, stores_an_aligned_word_inside_the_region_only)
{ GuestRegion region;
  Import      import;
  CHECK(region.adopt(import.blob));
  CHECK(region.publish(8u, 0x1122334455667788ull));
  std::uint64_t read = 0u;
  std::memcpy(&read, mapping + 8u, sizeof read);
  CHECK_EQ(read, 0x1122334455667788ull);
  CHECK(!region.publish(4u, 9u));
  CHECK(!region.publish(sizeof mapping - 4u, 9u));
  CHECK(!region.publish(sizeof mapping, 9u));
  CHECK(region.publish(sizeof mapping - 8u, 9u));
  std::memcpy(&read, mapping + sizeof mapping - 8u, sizeof read);
  CHECK_EQ(read, 9u);
  region.abandon(import.blob); }

/* A length with nothing behind it is a region that would answer bounds
 * checks for memory nobody mapped, if `at` ever stopped checking the base. */
TEST(guest_region_invariants, a_length_with_no_mapping_behind_it_breaks_them)
{ GuestRegion region;
  GuestRegionProbe(region).set_length(sizeof mapping);
  CHECK(!region.invariants_hold()); }

/* Memory held with no mapping of it is an import the region cannot reach. */
TEST(guest_region_invariants, memory_without_a_mapping_breaks_them)
{ GuestRegion region;
  Import      import;
  CHECK(region.adopt(import.blob));
  GuestRegionProbe(region).forget_base();
  CHECK(!region.invariants_hold()); }

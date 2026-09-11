/* Every branch of GpuVirtualAddressSpace and VirtualRange, the invariants after
 * every mutation, and hostile input. Constitution Principles III and IV. */
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidmm/address_space_fixture.hpp"

#include <cstddef>
#include <cstdint>

using av::Alignment;
using av::AddressSpaceFixture;
using av::ByteRange;
using av::GpuVirtualAddressSpace;
using av::GpuVirtualAddressSpaceProbe;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;
using av::VirtualRange;

static constexpr std::uint64_t kBase = AddressSpaceFixture::base;
static constexpr std::uint64_t kSize = AddressSpaceFixture::size;
static constexpr std::uint64_t kPage = AddressSpaceFixture::page;

/* Every mutation is followed by an invariant check, per the constitution. */
#define MUTATE(fixture, expression, expected)                                  \
    do { CHECK_STATUS(expression, expected);                                   \
         CHECK((fixture).space().invariants_hold()); } while (0)

/* ---------------------------------------------------------------- initialize */

TEST(address_space_initialize, rejects_an_unrepresentable_bounds)
{ MockPlatform platform; GpuVirtualAddressSpace space(platform);
  CHECK_STATUS(space.initialize(ByteRange(kBase, 0u)), Status::invalid_parameter);
  CHECK_STATUS(space.initialize(ByteRange(UINT64_MAX, kPage)),
               Status::invalid_parameter); }

TEST(address_space_initialize, rejects_an_unaligned_base_or_size)
{ MockPlatform platform; GpuVirtualAddressSpace space(platform);
  CHECK_STATUS(space.initialize(ByteRange(kBase + 1u, kSize)),
               Status::invalid_parameter);
  CHECK_STATUS(space.initialize(ByteRange(kBase, kSize + 1u)),
               Status::invalid_parameter); }

TEST(address_space_initialize, accepts_an_aligned_bounds_and_starts_empty)
{ AddressSpaceFixture fixture;
  CHECK_EQ(fixture.space().range_count(), 0u);
  CHECK(fixture.space().invariants_hold()); }

/* ------------------------------------------------------------------- reserve */

TEST(address_space_reserve, rejects_an_invalid_alignment)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(0u), out),
         Status::invalid_parameter);
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage * 3u), out),
         Status::invalid_parameter); }

TEST(address_space_reserve, rejects_an_alignment_below_a_page)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(512u), out),
         Status::invalid_parameter); }

TEST(address_space_reserve, rejects_a_zero_or_unaligned_size)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(0u, Alignment(kPage), out),
         Status::invalid_parameter);
  MUTATE(fixture, fixture.space().reserve(kPage + 1u, Alignment(kPage), out),
         Status::invalid_parameter); }

TEST(address_space_reserve, rejects_a_size_larger_than_the_space)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kSize + kPage, Alignment(kPage), out),
         Status::too_large); }

TEST(address_space_reserve, places_the_first_reservation_at_the_base)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage), out), Status::ok);
  CHECK_EQ(out, kBase);
  CHECK_EQ(fixture.space().range_count(), 1u); }

TEST(address_space_reserve, places_the_second_reservation_after_the_first)
{ AddressSpaceFixture fixture;
  const std::uint64_t first = fixture.reserve_page();
  const std::uint64_t second = fixture.reserve_page();
  CHECK_EQ(second, first + kPage); }

TEST(address_space_reserve, reuses_a_hole_left_by_release)
{ AddressSpaceFixture fixture;
  const std::uint64_t first = fixture.reserve_page();
  const std::uint64_t second = fixture.reserve_page();
  (void)fixture.reserve_page();
  MUTATE(fixture, fixture.space().release(second), Status::ok);
  std::uint64_t again = 0u;
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage), again), Status::ok);
  CHECK_EQ(again, second);
  CHECK(again != first); }

TEST(address_space_reserve, honours_an_alignment_larger_than_a_page)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  (void)fixture.reserve_page();
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage * 16u), out),
         Status::ok);
  CHECK_EQ(out % (kPage * 16u), 0u); }

TEST(address_space_reserve, does_not_rewind_the_cursor_when_alignment_skips_a_range)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  (void)fixture.reserve_page();
  (void)fixture.reserve_page();
  (void)fixture.reserve_page();
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage * 16u), out),
         Status::ok);
  CHECK_EQ(out, kBase + kPage * 16u); }

TEST(address_space_reserve, fills_the_space_exactly_then_refuses)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kSize, Alignment(kPage), out), Status::ok);
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage), out),
         Status::no_space); }

TEST(address_space_reserve, refuses_when_only_a_too_small_tail_hole_remains)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kSize - kPage, Alignment(kPage), out),
         Status::ok);
  MUTATE(fixture, fixture.space().reserve(kPage * 2u, Alignment(kPage), out),
         Status::no_space); }

TEST(address_space_reserve, refuses_when_aligning_past_a_range_leaves_the_space)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kSize - kPage, Alignment(kPage), out),
         Status::ok);
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kSize * 4u), out),
         Status::no_space); }

TEST(address_space_reserve, refuses_when_the_initial_cursor_alignment_overflows)
{ MockPlatform platform; GpuVirtualAddressSpace space(platform);
  std::uint64_t out = 0u;
  CHECK_STATUS(space.initialize(ByteRange(0xFFFFFFFFFFF00000ull, 0xF0000ull)),
               Status::ok);
  CHECK_STATUS(space.reserve(kPage, Alignment(0x8000000000000000ull), out),
               Status::no_space);
  CHECK(space.invariants_hold()); }

TEST(address_space_reserve, refuses_when_aligning_past_a_range_overflows)
{ MockPlatform platform; GpuVirtualAddressSpace space(platform);
  std::uint64_t out = 0u;
  CHECK_STATUS(space.initialize(ByteRange(0xFFFFFFFFFFF00000ull, 0xF0000ull)),
               Status::ok);
  CHECK_STATUS(space.reserve(kPage, Alignment(0x100000ull), out), Status::ok);
  CHECK_STATUS(space.reserve(kPage, Alignment(0x100000ull), out), Status::no_space);
  CHECK(space.invariants_hold()); }

TEST(address_space_reserve, propagates_allocation_failure)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  fixture.platform().fail_allocation_at(1u);
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage), out),
         Status::no_memory);
  CHECK_EQ(fixture.space().range_count(), 0u); }

TEST(address_space_lifetime, destroying_before_any_reservation_releases_nothing)
{ MockPlatform platform;
  { GpuVirtualAddressSpace space(platform);
    CHECK_STATUS(space.initialize(ByteRange(AddressSpaceFixture::base,
                                            AddressSpaceFixture::size)), Status::ok); }
  CHECK(!platform.claim(Violation::null_release)); }

TEST(address_space_lifetime, destroying_after_a_failed_first_allocation_releases_nothing)
{ MockPlatform platform;
  { GpuVirtualAddressSpace space(platform);
    CHECK_STATUS(space.initialize(ByteRange(AddressSpaceFixture::base,
                                            AddressSpaceFixture::size)), Status::ok);
    platform.fail_allocation_at(1u);
    std::uint64_t out = 0u;
    CHECK_STATUS(space.reserve(kPage, Alignment(kPage), out), Status::no_memory); }
  CHECK(!platform.claim(Violation::null_release));
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(address_space_reserve, grows_the_table_past_its_initial_capacity)
{ AddressSpaceFixture fixture;
  for (int i = 0; i < 20; i++) (void)fixture.reserve_page();
  CHECK_EQ(fixture.space().range_count(), 20u);
  GpuVirtualAddressSpaceProbe probe(fixture.space());
  CHECK(probe.capacity() >= 20u); }

TEST(address_space_reserve, clamps_table_growth_to_the_ceiling)
{ AddressSpaceFixture fixture;
  fixture.space().set_range_ceiling(20u);
  for (int i = 0; i < 17; i++) (void)fixture.reserve_page();
  GpuVirtualAddressSpaceProbe probe(fixture.space());
  CHECK_EQ(probe.capacity(), 20u); }

TEST(address_space_reserve, refuses_once_the_table_ceiling_is_reached)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  fixture.space().set_range_ceiling(16u);
  for (int i = 0; i < 16; i++) (void)fixture.reserve_page();
  MUTATE(fixture, fixture.space().reserve(kPage, Alignment(kPage), out),
         Status::no_space); }

/* ----------------------------------------------------------------------- map */

TEST(address_space_map, rejects_an_unreserved_base)
{ AddressSpaceFixture fixture;
  MUTATE(fixture, fixture.space().map(kBase, kPage, 7u, 0u), Status::not_found); }

TEST(address_space_map, rejects_a_base_that_falls_before_an_existing_range)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved - kPage, kPage, 7u, 0u),
         Status::not_found); }

TEST(address_space_map, rejects_a_size_that_does_not_match_the_reservation)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  MUTATE(fixture, fixture.space().reserve(kPage * 2u, Alignment(kPage), out),
         Status::ok);
  MUTATE(fixture, fixture.space().map(out, kPage, 7u, 0u),
         Status::invalid_parameter); }

TEST(address_space_map, rejects_object_zero_because_zero_means_unmapped)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 0u, 0u),
         Status::invalid_parameter); }

TEST(address_space_map, rejects_an_offset_that_overflows_with_the_size)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 7u, UINT64_MAX),
         Status::invalid_parameter); }

TEST(address_space_map, rejects_a_double_map)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 7u, 0u), Status::ok);
  MUTATE(fixture, fixture.space().map(reserved, kPage, 8u, 0u), Status::conflict); }

TEST(address_space_map, binds_the_object_and_offset)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 7u, 0x2000u), Status::ok);
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(reserved, object, offset), Status::ok);
  CHECK_EQ(object, 7u); CHECK_EQ(offset, 0x2000u); }

/* --------------------------------------------------------------------- unmap */

TEST(address_space_unmap, rejects_an_unknown_base)
{ AddressSpaceFixture fixture;
  MUTATE(fixture, fixture.space().unmap(kBase), Status::not_found); }

TEST(address_space_unmap, rejects_a_range_that_was_never_mapped)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().unmap(reserved), Status::invalid_state); }

TEST(address_space_unmap, clears_the_binding_and_allows_a_remap)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 7u, 0u), Status::ok);
  MUTATE(fixture, fixture.space().unmap(reserved), Status::ok);
  MUTATE(fixture, fixture.space().map(reserved, kPage, 9u, 0u), Status::ok); }

/* ------------------------------------------------------------------- release */

TEST(address_space_release, rejects_an_unknown_base)
{ AddressSpaceFixture fixture;
  MUTATE(fixture, fixture.space().release(kBase), Status::not_found); }

TEST(address_space_release, rejects_a_base_past_every_range)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().release(reserved + kPage * 4u), Status::not_found); }

TEST(address_space_release, rejects_a_base_before_every_range)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().release(reserved - kPage), Status::not_found); }

TEST(address_space_release, refuses_while_the_range_is_still_mapped)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 7u, 0u), Status::ok);
  MUTATE(fixture, fixture.space().release(reserved), Status::invalid_state); }

TEST(address_space_release, removes_the_only_range)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().release(reserved), Status::ok);
  CHECK_EQ(fixture.space().range_count(), 0u); }

TEST(address_space_release, removes_a_middle_range_and_keeps_the_order)
{ AddressSpaceFixture fixture;
  const std::uint64_t first = fixture.reserve_page();
  const std::uint64_t second = fixture.reserve_page();
  const std::uint64_t third = fixture.reserve_page();
  MUTATE(fixture, fixture.space().release(second), Status::ok);
  CHECK_EQ(fixture.space().range_count(), 2u);
  GpuVirtualAddressSpaceProbe probe(fixture.space());
  CHECK_EQ(probe.range_at(0u).span().base(), first);
  CHECK_EQ(probe.range_at(1u).span().base(), third); }

/* ----------------------------------------------------------------- translate */

TEST(address_space_translate, rejects_an_empty_space)
{ AddressSpaceFixture fixture;
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(kBase, object, offset), Status::not_found); }

TEST(address_space_translate, rejects_an_address_below_every_range)
{ AddressSpaceFixture fixture;
  const std::uint64_t first = fixture.reserve_page();
  (void)fixture.reserve_page();
  MUTATE(fixture, fixture.space().release(first), Status::ok);
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(kBase, object, offset), Status::not_found); }

TEST(address_space_translate, rejects_an_address_in_a_hole_between_ranges)
{ AddressSpaceFixture fixture;
  const std::uint64_t first = fixture.reserve_page();
  const std::uint64_t second = fixture.reserve_page();
  (void)fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(first, kPage, 1u, 0u), Status::ok);
  MUTATE(fixture, fixture.space().release(second), Status::ok);
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(second, object, offset),
               Status::not_found); }

TEST(address_space_translate, rejects_an_address_past_the_last_range)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 1u, 0u), Status::ok);
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(reserved + kPage, object, offset),
               Status::not_found); }

TEST(address_space_translate, rejects_a_reserved_but_unmapped_range)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(reserved, object, offset),
               Status::invalid_state); }

TEST(address_space_translate, adds_the_offset_within_the_range)
{ AddressSpaceFixture fixture; std::uint64_t base = 0u;
  MUTATE(fixture, fixture.space().reserve(kPage * 2u, Alignment(kPage), base),
         Status::ok);
  MUTATE(fixture, fixture.space().map(base, kPage * 2u, 7u, 0x1000u), Status::ok);
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(base + 0x800u, object, offset), Status::ok);
  CHECK_EQ(object, 7u); CHECK_EQ(offset, 0x1800u);
  CHECK_STATUS(fixture.space().translate(base + kPage * 2u - 1u, object, offset),
               Status::ok);
  CHECK_EQ(offset, 0x1000u + kPage * 2u - 1u); }

/* ---------------------------------------------------------------------- IRQL */

TEST(address_space_translate, is_callable_at_dispatch_level)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  MUTATE(fixture, fixture.space().map(reserved, kPage, 7u, 0u), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(reserved, object, offset), Status::ok);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);
  fixture.platform().set_irql(Irql::passive); }

TEST(address_space_reserve, is_rejected_at_dispatch_level)
{ AddressSpaceFixture fixture; std::uint64_t out = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.space().reserve(kPage, Alignment(kPage), out), Status::no_memory);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  CHECK(fixture.platform().claim(Violation::allocation_at_dispatch));
  fixture.platform().set_irql(Irql::passive); }

/* --------------------------------------------------------- virtual range ---- */

TEST(virtual_range, default_construction_is_unmapped)
{ const VirtualRange range;
  CHECK(!range.is_mapped());
  CHECK_EQ(range.object(), 0u);
  CHECK_EQ(range.object_offset(), 0u); }

TEST(virtual_range_translate, refuses_when_unmapped_or_out_of_span)
{ VirtualRange range{ByteRange(kBase, kPage)};
  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK(!range.translate(kBase, object, offset));
  CHECK_STATUS(range.bind(3u, 0u), Status::ok);
  CHECK(!range.translate(kBase + kPage, object, offset));
  CHECK(range.translate(kBase, object, offset));
  CHECK_EQ(object, 3u); }

TEST(virtual_range_unbind, refuses_when_not_bound)
{ VirtualRange range{ByteRange(kBase, kPage)};
  CHECK_STATUS(range.unbind(), Status::invalid_state); }

/* ---------------------------------------------------------------- invariants */

TEST(address_space_invariants, reject_a_corrupted_table)
{ AddressSpaceFixture fixture;
  const std::uint64_t first = fixture.reserve_page();
  const std::uint64_t second = fixture.reserve_page();
  GpuVirtualAddressSpaceProbe probe(fixture.space());
  GpuVirtualAddressSpace &space = fixture.space();

  probe.set_span(1u, ByteRange(first, kPage));
  CHECK(!space.invariants_hold());                       /* out of order */
  probe.set_span(1u, ByteRange(second, kPage));
  CHECK(space.invariants_hold());

  probe.set_span(0u, ByteRange(first, 0u));
  CHECK(!space.invariants_hold());                       /* empty span */
  probe.set_span(0u, ByteRange(first + 1u, kPage));
  CHECK(!space.invariants_hold());                       /* unaligned base */
  probe.set_span(0u, ByteRange(first, kPage + 1u));
  CHECK(!space.invariants_hold());                       /* unaligned size */
  probe.set_span(0u, ByteRange(first, kSize + kPage));
  CHECK(!space.invariants_hold());                       /* escapes bounds */
  probe.set_span(0u, ByteRange(first, kPage));
  CHECK(space.invariants_hold());

  probe.set_object(0u, 5u);
  CHECK(!space.invariants_hold());                       /* object without map */
  probe.set_object(0u, 0u);
  probe.set_mapped(0u, true);
  CHECK(!space.invariants_hold());                       /* map without object */
  probe.set_mapped(0u, false);
  CHECK(space.invariants_hold());

  probe.set_object_offset(0u, 8u);
  CHECK(!space.invariants_hold());                       /* offset while unmapped */
  probe.set_object_offset(0u, 0u);
  CHECK(space.invariants_hold());

  probe.set_object(0u, 1u);
  probe.set_mapped(0u, true);
  probe.set_object_offset(0u, UINT64_MAX);
  CHECK(!space.invariants_hold());                       /* offset + size wraps */
  probe.set_object_offset(0u, 0u);
  probe.set_object(0u, 0u);
  probe.set_mapped(0u, false);
  CHECK(space.invariants_hold());

  probe.set_count(probe.capacity() + 1u);
  CHECK(!space.invariants_hold());
  probe.set_count(2u);

  probe.set_ceiling(GpuVirtualAddressSpace::maximum_range_count + 1u);
  CHECK(!space.invariants_hold());
  probe.set_ceiling(1u);
  CHECK(!space.invariants_hold());                       /* capacity > ceiling */
  probe.set_ceiling(GpuVirtualAddressSpace::maximum_range_count);
  CHECK(space.invariants_hold());

  probe.set_bounds(ByteRange(kBase, 0u));
  CHECK(!space.invariants_hold());
  probe.set_bounds(ByteRange(kBase, kSize));
  CHECK(space.invariants_hold()); }

TEST(address_space_invariants, reject_a_zero_ceiling_on_an_untouched_table)
{ AddressSpaceFixture fixture;
  GpuVirtualAddressSpaceProbe probe(fixture.space());
  /* capacity is still zero, so this is the first check that can see it */
  probe.set_ceiling(0u);
  CHECK(!fixture.space().invariants_hold());
  probe.set_ceiling(GpuVirtualAddressSpace::maximum_range_count); }

TEST(address_space, reports_a_broken_invariant_from_a_mutating_call)
{ AddressSpaceFixture fixture;
  const std::uint64_t reserved = fixture.reserve_page();
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);

  GpuVirtualAddressSpaceProbe probe(fixture.space());
  probe.set_object(0u, 5u);                 /* object set without mapped_ */
  (void)fixture.space().unmap(reserved);    /* the scope guard must notice */
  /* once on entry, once on exit: the call left it broken too */
  CHECK(fixture.platform().claim(Violation::broken_invariant));
  CHECK(fixture.platform().claim(Violation::broken_invariant));

  probe.set_object(0u, 0u);
  CHECK(fixture.space().invariants_hold()); }

TEST(address_space_invariants, reject_a_capacity_with_no_table)
{ AddressSpaceFixture fixture;
  GpuVirtualAddressSpaceProbe probe(fixture.space());
  probe.set_capacity(4u);
  probe.drop_table();
  CHECK(!fixture.space().invariants_hold());
  probe.set_capacity(0u); }

TEST(address_space_invariants, hold_across_a_pseudorandom_operation_sequence)
{ AddressSpaceFixture fixture;
  std::uint64_t held[64]; std::uint32_t held_count = 0u;
  std::uint32_t rng = 0xC0FFEEu;
  for (int step = 0; step < 3000; step++) {
      rng = rng * 1664525u + 1013904223u;
      const std::uint32_t operation = (rng >> 16) & 3u;
      if (operation == 0u && held_count < 64u) {
          std::uint64_t reserved = 0u;
          const std::uint64_t size = kPage * (1u + ((rng >> 4) & 3u));
          if (fixture.space().reserve(size, Alignment(kPage), reserved) == Status::ok)
              held[held_count++] = reserved;
      } else if (held_count > 0u) {
          const std::uint32_t index = (rng >> 8) % held_count;
          if (operation == 1u) {
              GpuVirtualAddressSpaceProbe probe(fixture.space());
              (void)fixture.space().map(held[index],
                                        probe.range_at(0u).span().size(),
                                        1u + index, 0u);
          } else if (operation == 2u) {
              (void)fixture.space().unmap(held[index]);
          } else if (fixture.space().release(held[index]) == Status::ok) {
              held[index] = held[--held_count];
          }
      }
      CHECK(fixture.space().invariants_hold());   /* after EVERY mutation */
  } }

/* -------------------------------------------------- allocation failure ----- */

TEST(address_space, survives_failure_at_every_allocation_site)
{ for (std::uint32_t nth = 1u; nth <= 8u; nth++) {
      AddressSpaceFixture fixture;
      fixture.platform().fail_allocation_at(nth);
      std::uint64_t reserved = 0u;
      for (int i = 0; i < 40; i++) {
          if (fixture.space().reserve(kPage, Alignment(kPage), reserved) != Status::ok)
              break;
      }
      CHECK(fixture.space().invariants_hold());
  } }

/* ------------------------------------------------------- hostile input ----- */

namespace {

GpuVirtualAddressSpace *g_hostile_space = nullptr;

bool reject_hostile_address(const std::uint8_t *bytes, std::size_t length)
{
    std::uint64_t address = 0u;
    for (std::size_t i = 0u; i < length && i < 8u; i++)
        address |= static_cast<std::uint64_t>(bytes[i]) << (8u * i);

    std::uint32_t object = 0u; std::uint64_t offset = 0u;
    const Status status = g_hostile_space->translate(address, object, offset);
    if (status == Status::ok) CHECK(object != 0u);
    return status != Status::ok;
}

}  // namespace

TEST(address_space_translate, hostile_addresses_never_translate_out_of_bounds)
{ AddressSpaceFixture fixture; std::uint64_t base = 0u;
  MUTATE(fixture, fixture.space().reserve(kPage * 4u, Alignment(kPage), base),
         Status::ok);
  MUTATE(fixture, fixture.space().map(base, kPage * 4u, 42u, 0x10000u), Status::ok);
  g_hostile_space = &fixture.space();

  std::uint8_t valid[8];
  for (unsigned i = 0u; i < 8u; i++)
      valid[i] = static_cast<std::uint8_t>(base >> (8u * i));
  const av::HostileSweep sweep(reject_hostile_address, valid, sizeof valid);
  sweep.run();
  CHECK(sweep.cases_run() > 64u);

  std::uint32_t object = 0u; std::uint64_t offset = 0u;
  CHECK_STATUS(fixture.space().translate(0u, object, offset), Status::not_found);
  CHECK_STATUS(fixture.space().translate(UINT64_MAX, object, offset),
               Status::not_found);
  CHECK_STATUS(fixture.space().translate(base - 1u, object, offset),
               Status::not_found);
  CHECK_STATUS(fixture.space().translate(base + kPage * 4u, object, offset),
               Status::not_found);
  CHECK_STATUS(fixture.space().translate(base + kPage * 4u - 1u, object, offset),
               Status::ok);
  CHECK_EQ(offset, 0x10000u + kPage * 4u - 1u);
  g_hostile_space = nullptr; }

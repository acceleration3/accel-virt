/* Which host buffer a guest GPU virtual address falls in: every buffer
 * registered with the guest range it was given, a guest address translated to
 * the host buffer's address plus the same offset, and anything outside one
 * registered range translated to nothing. Guest addresses come off the wire,
 * so the ends of ranges and the top of the address space are the cases. */
#include "device/executor/d3d12_address_map.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12AddressMap;

namespace {

/* Most of half a megabyte: one, cleared by each test. */
D3d12AddressMap &fresh_map()
{
    static D3d12AddressMap map;
    map.clear();
    return map;
}

}  // namespace

TEST(d3d12_address_map_translate, an_address_inside_a_range_is_the_host_buffer_plus_its_offset)
{ D3d12AddressMap &map = fresh_map();
  CHECK(map.insert(0x10000u, 0x1000u, 0xA0000000u, 3u));
  CHECK(map.translate(0x10000u, 0u) == 0xA0000000u);
  CHECK(map.translate(0x10100u, 0x100u) == 0xA0000100u);
  CHECK(map.translate(0x10FFFu, 1u) == 0xA0000FFFu);   /* the last byte */
  CHECK(map.translate(0x10000u, 0x1000u) == 0xA0000000u);   /* the whole range */
  CHECK(map.translate(0x10001u, 0x1000u) == 0u);        /* one byte past its end */
  CHECK(map.translate(0x11000u, 0u) == 0u);             /* just after */
  CHECK(map.translate(0xFFFFu, 0u) == 0u);              /* just before */
  CHECK(map.invariants_hold()); }

TEST(d3d12_address_map_owner, names_the_buffer_and_the_offset_or_none)
{ D3d12AddressMap &map = fresh_map();
  CHECK(map.insert(0x2000u, 0x100u, 0x9000u, 11u));
  CHECK(map.insert(0x1000u, 0x100u, 0x8000u, 10u));
  std::uint64_t offset = 77u;
  CHECK_EQ(map.owner(0x2040u, offset), 11u);
  CHECK(offset == 0x40u);
  CHECK_EQ(map.owner(0x10FFu, offset), 10u);
  CHECK(offset == 0xFFu);
  CHECK_EQ(map.owner(0x1100u, offset), 0u);
  CHECK(offset == 0u);
  CHECK_EQ(map.owner(0u, offset), 0u); }

TEST(d3d12_address_map_insert, refuses_an_empty_wrapping_overlapping_or_nameless_range)
{ D3d12AddressMap &map = fresh_map();
  CHECK(map.insert(0x4000u, 0x1000u, 0x1u, 1u));
  CHECK(!map.insert(0x8000u, 0u, 0x1u, 2u));
  CHECK(!map.insert(~std::uint64_t{0} - 0xFFu, 0x100u, 0x1u, 2u));   /* ends at 2^64: wraps */
  CHECK(!map.insert(0x4800u, 0x10u, 0x1u, 2u));    /* inside */
  CHECK(!map.insert(0x3000u, 0x1001u, 0x1u, 2u));  /* runs into it */
  CHECK(!map.insert(0x4FFFu, 0x10u, 0x1u, 2u));    /* starts on its last byte */
  CHECK(!map.insert(0x3000u, 0x3000u, 0x1u, 2u));  /* covers it */
  CHECK(!map.insert(0x9000u, 0x10u, 0x1u, 0u));    /* id 0 names nothing */
  CHECK(map.insert(0x3000u, 0x1000u, 0x1u, 2u));   /* ends exactly where it starts */
  CHECK(map.insert(0x5000u, 0x1000u, 0x1u, 3u));   /* starts exactly where it ends */
  CHECK(map.insert(~std::uint64_t{0} - 0xFFu, 0xFFu, 0x1u, 4u));   /* the top, without wrapping */
  CHECK_EQ(map.count(), 4u);
  CHECK(map.invariants_hold()); }

TEST(d3d12_address_map_remove, forgets_one_buffer_s_range_and_keeps_the_rest_in_order)
{ D3d12AddressMap &map = fresh_map();
  for (std::uint32_t id = 1u; id <= 5u; id++) CHECK(map.insert(id * 0x1000u, 0x800u, id * 0x100000u, id));
  map.remove(3u);
  map.remove(99u);
  CHECK_EQ(map.count(), 4u);
  CHECK(map.translate(0x3000u, 0u) == 0u);
  CHECK(map.translate(0x4010u, 0u) == 0x400010u);
  CHECK(map.translate(0x1010u, 0u) == 0x100010u);
  CHECK(map.translate(0x5010u, 0u) == 0x500010u);
  CHECK(map.insert(0x3000u, 0x1000u, 0x7u, 3u));   /* its range is free again */
  CHECK(map.invariants_hold());
  map.clear();
  CHECK_EQ(map.count(), 0u);
  CHECK(map.translate(0x1010u, 0u) == 0u); }

TEST(d3d12_address_map_insert, a_full_table_refuses_the_next_range)
{ D3d12AddressMap &map = fresh_map();
  for (std::uint32_t id = 1u; id <= D3d12AddressMap::capacity; id++)
      CHECK(map.insert(std::uint64_t{id} * 0x100u, 0x10u, id, id));
  CHECK(!map.insert(0x10000000u, 0x10u, 1u, 1u));
  CHECK_EQ(map.count(), D3d12AddressMap::capacity);
  CHECK(map.translate(std::uint64_t{D3d12AddressMap::capacity} * 0x100u + 4u, 0u) ==
        D3d12AddressMap::capacity + 4u);
  CHECK(map.invariants_hold());
  map.clear(); }

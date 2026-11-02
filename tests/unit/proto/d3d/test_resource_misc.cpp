#include "harness/framework/test_registry.hpp"
#include "proto/d3d/resource_misc.hpp"

using av::ResourceMisc;

namespace {
constexpr unsigned generate_mips = 0x1u;
constexpr unsigned shared_nthandle = 0x800u;

}

TEST(resource_misc, a_shared_texture_is_tiled_unless_it_asked_for_rows)
{ const ResourceMisc tiled(ResourceMisc::shared_bit | shared_nthandle);
  CHECK(tiled.shared());
  CHECK(!tiled.rows());
  const ResourceMisc rows(ResourceMisc::shared_bit | ResourceMisc::rows_bit);
  CHECK(rows.shared());
  CHECK(rows.rows()); }

TEST(resource_misc, rows_without_sharing_is_nothing_shared)
{ const ResourceMisc lone(ResourceMisc::rows_bit | generate_mips);
  CHECK(!lone.shared());
  CHECK(!lone.rows());
  const ResourceMisc plain(generate_mips);
  CHECK(!plain.shared());
  CHECK(!plain.rows());
  CHECK_EQ(ResourceMisc::rows_bit & 0x00ffffffu, 0u); }

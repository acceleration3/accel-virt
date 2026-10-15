/* CreateResource's misc flags: D3D11's own bits, and the protocol's rows bit,
 * which means something only beside D3D11_RESOURCE_MISC_SHARED. */
#include "harness/framework/test_registry.hpp"
#include "proto/d3d/resource_misc.hpp"

using av::ResourceMisc;

namespace {

/* D3D11_RESOURCE_MISC_GENERATE_MIPS and _SHARED_NTHANDLE, which ride along. */
constexpr unsigned generate_mips = 0x1u;
constexpr unsigned shared_nthandle = 0x800u;

}  // namespace

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
  /* The rows bit is above every bit D3D11 defines, so it never reads as one. */
  CHECK_EQ(ResourceMisc::rows_bit & 0x00ffffffu, 0u); }

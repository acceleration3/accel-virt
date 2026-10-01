/* What a multisampled presented texture is resolved in: a typed format is
 * kept, a colour family's typeless head becomes the family's UNORM member, and
 * a typeless format nothing presents has none. Values are DXGI_FORMAT's
 * (dxgiformat.h). */
#include "harness/framework/test_registry.hpp"
#include "proto/d3d/dxgi_resolve_format.hpp"

using av::DxgiResolveFormat;

TEST(dxgi_resolve_format, a_typeless_colour_family_resolves_in_its_unorm_member)
{ CHECK_EQ(DxgiResolveFormat(23u).typed(), 24u);   /* R10G10B10A2 */
  CHECK_EQ(DxgiResolveFormat(27u).typed(), 28u);   /* R8G8B8A8 */
  CHECK_EQ(DxgiResolveFormat(90u).typed(), 87u);   /* B8G8R8A8 */
  CHECK_EQ(DxgiResolveFormat(92u).typed(), 88u); } /* B8G8R8X8 */

TEST(dxgi_resolve_format, a_typed_format_resolves_in_itself)
{ CHECK_EQ(DxgiResolveFormat(87u).typed(), 87u);   /* B8G8R8A8_UNORM */
  CHECK_EQ(DxgiResolveFormat(29u).typed(), 29u);   /* R8G8B8A8_UNORM_SRGB */
  CHECK_EQ(DxgiResolveFormat(91u).typed(), 91u);   /* B8G8R8A8_UNORM_SRGB: between two heads */
  CHECK_EQ(DxgiResolveFormat(24u).typed(), 24u); }

TEST(dxgi_resolve_format, other_typeless_formats_and_unknown_have_none)
{ CHECK_EQ(DxgiResolveFormat(0u).typed(), 0u);     /* UNKNOWN */
  CHECK_EQ(DxgiResolveFormat(1u).typed(), 0u);     /* R32G32B32A32_TYPELESS */
  CHECK_EQ(DxgiResolveFormat(39u).typed(), 0u);    /* R32_TYPELESS */
  CHECK_EQ(DxgiResolveFormat(44u).typed(), 0u);    /* R24G8_TYPELESS */
  CHECK_EQ(DxgiResolveFormat(97u).typed(), 0u); }  /* BC7_TYPELESS */

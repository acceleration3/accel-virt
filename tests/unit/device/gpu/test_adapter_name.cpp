#include "device/gpu/adapter_name.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gpu_identity.hpp"

#include <cstring>

using av::AdapterName;
using av::GpuIdentity;

namespace {

bool named(const char *vulkan_name, const char *expected)
{
    return std::strcmp(AdapterName(vulkan_name).text(), expected) == 0;
}

}  // namespace

TEST(adapter_name, drops_the_driver_suffix_mesa_appends)
{ CHECK(named("AMD Radeon RX 9070 XT (RADV GFX1201)", "AMD Radeon RX 9070 XT"));
  CHECK(named("AMD Radeon Graphics (RADV RAPHAEL_MENDOCINO)", "AMD Radeon Graphics"));
  CHECK(named("llvmpipe (LLVM 19.1.7, 256 bits)", "llvmpipe"));
  CHECK(named("Intel(R) Arc(tm) A770 Graphics (DG2)", "Intel(R) Arc(tm) A770 Graphics"));
  CHECK(named("Mesa (nested (groups)) ", "Mesa"));
  CHECK(named("Device  (two spaces)", "Device")); }

TEST(adapter_name, keeps_a_name_with_no_suffix_to_drop)
{ CHECK(named("NVIDIA GeForce RTX 4090", "NVIDIA GeForce RTX 4090"));
  CHECK(named("Intel(R) Arc(tm)", "Intel(R) Arc(tm)"));        /* no space before the group */
  CHECK(named("(RADV GFX1201)", "(RADV GFX1201)"));             /* nothing would be left */
  CHECK(named("   (only spaces before)", "   (only spaces before)"));
  CHECK(named("unbalanced)", "unbalanced)"));
  CHECK(named("unbalanced x)", "unbalanced x)"));
  CHECK(named("  ", "  "));
  CHECK(named("", "")); }

TEST(adapter_name, fits_the_capset_field_without_splitting_a_character)
{ char long_name[GpuIdentity::name_bytes + 8u] = {};
  std::memset(long_name, 'x', sizeof long_name - 1u);
  CHECK_EQ(std::strlen(AdapterName(long_name).text()), GpuIdentity::name_bytes - 1u);
  /* A two-byte character straddling the last byte that fits is left out whole. */
  char straddling[GpuIdentity::name_bytes + 8u] = {};
  std::memset(straddling, 'y', sizeof straddling - 1u);
  straddling[GpuIdentity::name_bytes - 2u] = static_cast<char>(0xC3u);
  straddling[GpuIdentity::name_bytes - 1u] = static_cast<char>(0xA9u);
  CHECK_EQ(std::strlen(AdapterName(straddling).text()), GpuIdentity::name_bytes - 2u);
  /* Exactly full is kept whole. */
  char full[GpuIdentity::name_bytes] = {};
  std::memset(full, 'z', sizeof full - 1u);
  CHECK(named(full, full));
  /* The suffix goes first, so a long name with one can still fit whole. */
  char suffixed[GpuIdentity::name_bytes + 16u] = {};
  std::memset(suffixed, 'w', GpuIdentity::name_bytes - 1u);
  std::memcpy(suffixed + GpuIdentity::name_bytes - 1u, " (RADV)", 8u);
  CHECK_EQ(std::strlen(AdapterName(suffixed).text()), GpuIdentity::name_bytes - 1u);
  CHECK_EQ(AdapterName(suffixed).text()[GpuIdentity::name_bytes - 2u], 'w'); }

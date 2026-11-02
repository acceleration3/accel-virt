#include "harness/framework/test_registry.hpp"
#include "icd/d3d/guest_surface_scan.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::GuestSurfaceScan;
using av::d3d::RowBand;

namespace {

constexpr std::size_t   pitch = 8u;
constexpr std::uint32_t height = 8u;

struct Surface {
    std::uint8_t pages[pitch * height];
    std::uint8_t shadow[pitch * height];
    Surface()
    {
        for (std::size_t at = 0u; at < sizeof pages; at++) pages[at] = static_cast<std::uint8_t>(at);
        std::memcpy(shadow, pages, sizeof shadow);
    }
};

}

TEST(guest_surface_scan, identical_pages_leave_an_empty_band_and_the_shadow_alone)
{ Surface surface;
  GuestSurfaceScan scan(surface.shadow, pitch, height);
  CHECK(scan.differing(surface.pages).empty());
  CHECK(std::memcmp(surface.shadow, surface.pages, sizeof surface.shadow) == 0); }

TEST(guest_surface_scan, one_differing_byte_is_a_band_of_its_row)
{ Surface surface;
  surface.pages[3u * pitch + 7u] ^= 0x40u;
  GuestSurfaceScan scan(surface.shadow, pitch, height);
  const RowBand band = scan.differing(surface.pages);
  CHECK_EQ(band.first(), 3u);
  CHECK_EQ(band.end(), 4u);
  CHECK(std::memcmp(surface.shadow, surface.pages, sizeof surface.shadow) == 0); }

TEST(guest_surface_scan, rows_two_and_five_make_a_band_of_two_through_five)
{ Surface surface;
  surface.pages[2u * pitch] ^= 0xFFu;
  surface.pages[5u * pitch + 3u] ^= 0xFFu;
  GuestSurfaceScan scan(surface.shadow, pitch, height);
  const RowBand band = scan.differing(surface.pages);
  CHECK_EQ(band.first(), 2u);
  CHECK_EQ(band.end(), 6u);
  CHECK(std::memcmp(surface.shadow + 2u * pitch, surface.pages + 2u * pitch, 4u * pitch) == 0);
  CHECK(std::memcmp(surface.shadow, surface.pages, sizeof surface.shadow) == 0); }

TEST(guest_surface_scan, the_first_and_last_rows_are_scanned)
{ Surface top;
  top.pages[0u] ^= 0x01u;
  GuestSurfaceScan top_scan(top.shadow, pitch, height);
  const RowBand at_top = top_scan.differing(top.pages);
  CHECK_EQ(at_top.first(), 0u);
  CHECK_EQ(at_top.end(), 1u);
  Surface bottom;
  bottom.pages[sizeof bottom.pages - 1u] ^= 0x01u;
  GuestSurfaceScan bottom_scan(bottom.shadow, pitch, height);
  const RowBand at_bottom = bottom_scan.differing(bottom.pages);
  CHECK_EQ(at_bottom.first(), height - 1u);
  CHECK_EQ(at_bottom.end(), height);
  CHECK(std::memcmp(bottom.shadow, bottom.pages, sizeof bottom.shadow) == 0); }

TEST(guest_surface_scan, a_second_scan_of_the_same_pages_finds_nothing)
{ Surface surface;
  surface.pages[4u * pitch + 1u] ^= 0x10u;
  GuestSurfaceScan scan(surface.shadow, pitch, height);
  CHECK(!scan.differing(surface.pages).empty());
  CHECK(scan.differing(surface.pages).empty()); }

TEST(guest_surface_scan, a_surface_of_no_rows_has_no_band)
{ Surface surface;
  GuestSurfaceScan scan(surface.shadow, pitch, 0u);
  CHECK(scan.differing(surface.pages).empty()); }

/* The scan of a guest surface's pages against its shadow (GuestSurfaceScan):
 * the band of differing rows, first through last, and the shadow brought
 * level with the pages over exactly that band. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/guest_surface_scan.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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

}  // namespace

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

namespace {

/* Pages and a shadow of `rows` rows of `row_pitch` bytes, the shadow starting
 * `shadow_offset` bytes past a sixteen-byte boundary and the pages `page_offset`. */
struct Odd {
    alignas(16) std::uint8_t pages_store[1024u];
    alignas(16) std::uint8_t shadow_store[1024u];
    std::uint8_t            *pages;
    std::uint8_t            *shadow;
    std::size_t              row_pitch;
    std::uint32_t            rows;
    Odd(std::size_t pitch_bytes, std::uint32_t row_count, std::size_t page_offset, std::size_t shadow_offset)
        : pages(pages_store + page_offset), shadow(shadow_store + shadow_offset), row_pitch(pitch_bytes), rows(row_count)
    {
        for (std::size_t at = 0u; at < pitch_bytes * row_count; at++) pages[at] = static_cast<std::uint8_t>(at * 7u + 1u);
        std::memcpy(shadow, pages, pitch_bytes * row_count);
    }
    bool shadow_equals_pages() const { return std::memcmp(shadow, pages, row_pitch * rows) == 0; }
};

}  // namespace

TEST(guest_surface_scan, an_unaligned_shadow_and_a_pitch_of_no_multiple_of_sixteen_are_scanned_exactly)
{ const std::size_t pitches[] = {1u, 15u, 17u, 31u, 63u, 65u, 100u, 130u};
  const std::size_t shadow_offsets[] = {0u, 1u, 7u, 15u};
  for (const std::size_t row_pitch : pitches) {
      for (const std::size_t shadow_offset : shadow_offsets) {
          for (std::uint32_t row = 0u; row < 5u; row++) {
              const std::size_t bytes[] = {0u, row_pitch / 2u, row_pitch - 1u};
              for (const std::size_t byte : bytes) {
                  Odd odd(row_pitch, 5u, 0u, shadow_offset);
                  odd.pages[row * row_pitch + byte] ^= 0x80u;
                  GuestSurfaceScan scan(odd.shadow, row_pitch, 5u);
                  const RowBand band = scan.differing(odd.pages);
                  CHECK_EQ(band.first(), row);
                  CHECK_EQ(band.end(), row + 1u);
                  CHECK(odd.shadow_equals_pages());
                  CHECK(scan.differing(odd.pages).empty());
              }
          }
      }
  } }

TEST(guest_surface_scan, pages_that_start_off_a_boundary_are_scanned_exactly_too)
{ for (const std::size_t page_offset : {1u, 5u, 9u}) {
      Odd odd(37u, 6u, page_offset, 3u);
      odd.pages[37u * 6u - 1u] ^= 0x01u;
      GuestSurfaceScan scan(odd.shadow, 37u, 6u);
      const RowBand band = scan.differing(odd.pages);
      CHECK_EQ(band.first(), 5u);
      CHECK_EQ(band.end(), 6u);
      CHECK(odd.shadow_equals_pages());
  } }

TEST(guest_surface_scan, the_last_byte_of_the_last_row_and_the_first_of_the_first_make_the_whole_band)
{ Odd odd(37u, 8u, 0u, 5u);
  odd.pages[0u] ^= 0x01u;
  odd.pages[37u * 8u - 1u] ^= 0x01u;
  GuestSurfaceScan scan(odd.shadow, 37u, 8u);
  const RowBand band = scan.differing(odd.pages);
  CHECK_EQ(band.first(), 0u);
  CHECK_EQ(band.end(), 8u);
  CHECK(odd.shadow_equals_pages()); }

TEST(guest_surface_scan, two_differences_make_the_band_between_them_and_the_shadow_takes_it)
{ Odd odd(37u, 8u, 0u, 5u);
  odd.pages[37u * 3u + 20u] ^= 0x01u;
  odd.pages[37u * 1u] ^= 0x01u;
  GuestSurfaceScan scan(odd.shadow, 37u, 8u);
  const RowBand band = scan.differing(odd.pages);
  CHECK_EQ(band.first(), 1u);
  CHECK_EQ(band.end(), 4u);
  CHECK(odd.shadow_equals_pages()); }

TEST(guest_surface_scan, copying_all_the_rows_makes_the_shadow_the_pages)
{ const std::size_t shadow_offsets[] = {0u, 3u, 8u};
  for (const std::size_t shadow_offset : shadow_offsets) {
      Odd odd(37u, 9u, 0u, shadow_offset);
      std::memset(odd.shadow, 0xA5, 37u * 9u);
      GuestSurfaceScan scan(odd.shadow, 37u, 9u);
      scan.copy_all(odd.pages);
      CHECK(odd.shadow_equals_pages());
  } }

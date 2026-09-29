/* DisplayModeText: the `modes` property's text becomes the modes it names, in
 * its order, the first preferred, any size a display could have; text with a
 * single thing wrong in it changes nothing at all. */
#include "device/display/display_mode_text.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

#include <cstddef>

using av::DisplayModeSet;
using av::DisplayModeText;
using av::Status;

TEST(display_mode_text, entries_become_modes_in_order_with_the_first_preferred)
{ DisplayModeSet modes;
  CHECK_STATUS(DisplayModeText(" 2560x1440, 80X80 ,1x16384").read(modes), Status::ok);
  CHECK_EQ(modes.count(), 3u);
  CHECK_EQ(modes.preferred(), 0u);
  CHECK_EQ(modes.width(0u), 2560u);
  CHECK_EQ(modes.height(0u), 1440u);
  CHECK_EQ(modes.width(1u), 80u);
  CHECK_EQ(modes.height(1u), 80u);
  CHECK_EQ(modes.width(2u), 1u);
  CHECK_EQ(modes.height(2u), 16384u);
  CHECK_STATUS(DisplayModeText("640x480").read(modes), Status::ok);
  CHECK_EQ(modes.count(), 1u);
  CHECK_EQ(modes.width(0u), 640u); }

TEST(display_mode_text, no_text_is_empty_and_reads_nothing)
{ CHECK(DisplayModeText(nullptr).empty());
  CHECK(DisplayModeText("").empty());
  CHECK(DisplayModeText("   ").empty());
  CHECK(!DisplayModeText(" 1x1").empty());
  DisplayModeSet modes;
  CHECK_STATUS(DisplayModeText(nullptr).read(modes), Status::invalid_parameter);
  CHECK_EQ(modes.count(), 0u); }

TEST(display_mode_text, text_with_anything_wrong_changes_nothing)
{ DisplayModeSet modes;
  CHECK_STATUS(DisplayModeText("1920x1080").read(modes), Status::ok);
  const char *const refused[] = {
      "1920", "1920x", "x1080", "1920x1080,", ",1920x1080", "1920x1080,,640x480", "1920 x 1080", "1920x1080p",
      "0x1080", "1920x0", "16385x100", "100x16385", "1234567x1", "1920x1080,1920x1080", "-1x5", "1920*1080",
      "1920x1080;640x480",
  };
  for (const char *text : refused) {
      CHECK_STATUS(DisplayModeText(text).read(modes), Status::invalid_parameter);
      CHECK_EQ(modes.count(), 1u);
      CHECK_EQ(modes.width(0u), 1920u);
  }
  /* More modes than the set holds is no list either. */
  char text[DisplayModeSet::max_modes * 12u] = {};
  std::size_t at = 0u;
  for (unsigned width = 100u; width < 100u + DisplayModeSet::max_modes + 1u; width++) {
      at += static_cast<std::size_t>(__builtin_snprintf(text + at, sizeof text - at, "%s%ux10", at == 0u ? "" : ",", width));
  }
  CHECK_STATUS(DisplayModeText(text).read(modes), Status::invalid_parameter);
  CHECK_EQ(modes.count(), 1u); }

#include "core/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/display/display_mode_list.hpp"
#include "harness/framework/test_registry.hpp"

using av::D3dkmdtVideoSignalInfoRecord;
using av::D3dkmdtVidPnSourceModeRecord;
using av::DisplayMode;
using av::DisplayModeList;

TEST(display_mode_list, holds_common_resolutions_up_to_1080p_in_ascending_area)
{ const DisplayModeList modes;
  CHECK_EQ(modes.count(), 11u);
  CHECK_EQ(modes.mode(0u).width(), 640u);
  CHECK_EQ(modes.mode(0u).height(), 480u);
  CHECK_EQ(modes.mode(modes.count() - 1u).width(), 1920u);
  CHECK_EQ(modes.mode(modes.count() - 1u).height(), 1080u);
  for (unsigned index = 1u; index < modes.count(); index++)
      CHECK(modes.mode(index).width() * modes.mode(index).height() >=
            modes.mode(index - 1u).width() * modes.mode(index - 1u).height()); }

TEST(display_mode_list_mode, past_the_end_is_the_first)
{ const DisplayModeList modes;
  CHECK(&modes.mode(modes.count()) == &modes.mode(0u)); }

TEST(display_mode_list_preferred, is_1280x800_as_qemu_virtio_gpu_defaults)
{ const DisplayModeList modes;
  CHECK_EQ(modes.preferred().width(), 1280u);
  CHECK_EQ(modes.preferred().height(), 800u);
  CHECK(modes.is_preferred(modes.preferred()));
  CHECK(!modes.is_preferred(modes.mode(0u))); }

TEST(display_mode_list_find, a_source_mode_in_the_list)
{ const DisplayModeList modes;
  D3dkmdtVidPnSourceModeRecord source = {};
  DisplayMode(1024u, 768u).describe(source);
  const DisplayMode *found = modes.find(source);
  CHECK(found != nullptr);
  CHECK_EQ(found != nullptr ? found->width() : 0u, 1024u);
  source.PrimSurfSizeCy = 769u;
  CHECK(modes.find(source) == nullptr); }

TEST(display_mode_list_find, a_signal_in_the_list)
{ const DisplayModeList modes;
  D3dkmdtVideoSignalInfoRecord signal = {};
  DisplayMode(1920u, 1080u).describe(signal);
  CHECK(modes.find(signal) == &modes.mode(modes.count() - 1u));
  DisplayMode(2560u, 1440u).describe(signal);
  CHECK(modes.find(signal) == nullptr); }

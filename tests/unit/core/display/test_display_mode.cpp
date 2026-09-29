#include "core/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/display/display_mode.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dkmdtVideoSignalInfoRecord;
using av::D3dkmdtVidPnSourceModeRecord;
using av::DisplayMode;

namespace {

constexpr std::int32_t  graphics = 1;         /* D3DKMDT_RMT_GRAPHICS */
constexpr std::int32_t  a8r8g8b8 = 21;        /* D3DDDIFMT_A8R8G8B8 */
constexpr std::int32_t  x8r8g8b8 = 22;
constexpr std::int32_t  srgb = 2;             /* D3DKMDT_CB_SRGB */
constexpr std::int32_t  direct = 1;           /* D3DKMDT_PVAM_DIRECT */
constexpr std::int32_t  standard_other = 255; /* D3DKMDT_VSS_OTHER */
constexpr std::uint32_t progressive = 1u;     /* D3DDDI_VSSLO_PROGRESSIVE */
constexpr std::uint64_t second = 10000000u;   /* interrupt time is in 100 ns */

}  // namespace

TEST(display_mode, has_1080p_blanking_proportions)
{ const DisplayMode mode(1920u, 1080u);
  CHECK_EQ(mode.width(), 1920u);
  CHECK_EQ(mode.height(), 1080u);
  CHECK_EQ(mode.total_width(), 2080u);
  CHECK_EQ(mode.total_height(), 1125u);
  CHECK_EQ(mode.pixel_rate(), 2080u * 1125u * 60u);
  CHECK_EQ(DisplayMode(1280u, 800u).total_height(), 833u); }

TEST(display_mode_describe, a_source_mode_is_a_32_bpp_a8r8g8b8_primary)
{ D3dkmdtVidPnSourceModeRecord source = {};
  source.Id = 7u;
  DisplayMode(1024u, 768u).describe(source);
  CHECK_EQ(source.Id, 7u);
  CHECK_EQ(source.Type, graphics);
  CHECK_EQ(source.PrimSurfSizeCx, 1024u);
  CHECK_EQ(source.PrimSurfSizeCy, 768u);
  CHECK_EQ(source.VisibleRegionSizeCx, 1024u);
  CHECK_EQ(source.VisibleRegionSizeCy, 768u);
  CHECK_EQ(source.Stride, 4096u);
  CHECK_EQ(source.PixelFormat, a8r8g8b8);
  CHECK_EQ(source.ColorBasis, srgb);
  CHECK_EQ(source.PixelValueAccessMode, direct); }

TEST(display_mode_describe, a_signal_is_progressive_at_60_hz)
{ D3dkmdtVideoSignalInfoRecord signal = {};
  DisplayMode(1920u, 1080u).describe(signal);
  CHECK_EQ(signal.VideoStandard, standard_other);
  CHECK_EQ(signal.TotalSizeCx, 2080u);
  CHECK_EQ(signal.TotalSizeCy, 1125u);
  CHECK_EQ(signal.ActiveSizeCx, 1920u);
  CHECK_EQ(signal.ActiveSizeCy, 1080u);
  CHECK_EQ(signal.VSyncFreqNumerator, 60u);
  CHECK_EQ(signal.VSyncFreqDenominator, 1u);
  CHECK_EQ(signal.HSyncFreqNumerator, 1125u * 60u);
  CHECK_EQ(signal.HSyncFreqDenominator, 1u);
  CHECK_EQ(signal.PixelRate, 2080u * 1125u * 60u);
  CHECK_EQ(signal.ScanLineOrdering, progressive); }

TEST(display_mode_matches, a_source_mode_of_its_size_and_format)
{ const DisplayMode mode(1024u, 768u);
  D3dkmdtVidPnSourceModeRecord source = {};
  mode.describe(source);
  CHECK(mode.matches(source));
  /* The format a Windows source mode had before this driver offered A8R8G8B8,
   * which dxgkrnl may still pin from its saved configuration. */
  source.PixelFormat = x8r8g8b8;
  CHECK(!mode.matches(source));
  source.PixelFormat = a8r8g8b8;
  source.PrimSurfSizeCx = 1023u;
  CHECK(!mode.matches(source));
  source.PrimSurfSizeCx = 1024u;
  source.PrimSurfSizeCy = 769u;
  CHECK(!mode.matches(source)); }

TEST(display_mode_matches, a_signal_of_its_active_size)
{ const DisplayMode mode(1280u, 720u);
  D3dkmdtVideoSignalInfoRecord signal = {};
  mode.describe(signal);
  CHECK(mode.matches(signal));
  signal.ActiveSizeCx = 1281u;
  CHECK(!mode.matches(signal));
  signal.ActiveSizeCx = 1280u;
  signal.ActiveSizeCy = 721u;
  CHECK(!mode.matches(signal)); }

TEST(display_mode_scan_line, sweeps_the_total_height_once_per_frame)
{ const DisplayMode mode(1920u, 1080u);
  CHECK_EQ(mode.scan_line(0u), 0u);
  CHECK_EQ(mode.scan_line(second / 120u), 562u);
  CHECK_EQ(mode.scan_line(163000u), 1100u);
  CHECK_EQ(mode.scan_line(second), 0u);
  CHECK_EQ(mode.scan_line(second * 3600u * 24u * 365u * 100u + second / 120u), 562u);
  CHECK(mode.scan_line(0xFFFFFFFFFFFFFFFFull) < 1125u); }

/* dxgkrnl's smallest source mode edge is 100, and an EDID's detailed timing
 * names edges and blanks in twelve bits and the clock up to 655.35 MHz. */
TEST(display_mode, what_windows_can_be_offered)
{ CHECK(av::DisplayMode(100u, 100u).offerable());
  CHECK(!av::DisplayMode(99u, 100u).offerable());
  CHECK(!av::DisplayMode(100u, 99u).offerable());
  CHECK(!av::DisplayMode(80u, 80u).offerable());
  CHECK(av::DisplayMode(3840u, 2160u).offerable());
  CHECK(av::DisplayMode(4095u, 2160u).offerable());
  CHECK(!av::DisplayMode(4096u, 2160u).offerable());
  CHECK(!av::DisplayMode(1000u, 4096u).offerable());
  CHECK(!av::DisplayMode(4095u, 4095u).offerable());
  /* A slot's default is the smallest standard mode. */
  CHECK_EQ(av::DisplayMode().width(), 640u);
  CHECK_EQ(av::DisplayMode().height(), 480u); }

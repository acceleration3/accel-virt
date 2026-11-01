#include "miniport/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "miniport/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "miniport/display/display_mode.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dkmdtVideoSignalInfoRecord;
using av::D3dkmdtVidPnSourceModeRecord;
using av::DisplayMode;

namespace {

constexpr std::int32_t  graphics = 1;         /* D3DKMDT_RMT_GRAPHICS */
constexpr std::int32_t  a8r8g8b8 = 21;        /* D3DDDIFMT_A8R8G8B8 */
constexpr std::int32_t  x8r8g8b8 = 22;
constexpr std::int32_t  a8b8g8r8 = 32;        /* D3DDDIFMT_A8B8G8R8 */
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

TEST(display_mode_describe, a_source_mode_is_a_32_bpp_primary_of_the_format_asked)
{ D3dkmdtVidPnSourceModeRecord source = {};
  source.Id = 7u;
  DisplayMode(1024u, 768u).describe(source, a8r8g8b8);
  CHECK_EQ(source.Id, 7u);
  CHECK_EQ(source.Type, graphics);
  CHECK_EQ(source.PrimSurfSizeCx, 1024u);
  CHECK_EQ(source.PrimSurfSizeCy, 768u);
  CHECK_EQ(source.VisibleRegionSizeCx, 1024u);
  CHECK_EQ(source.VisibleRegionSizeCy, 768u);
  CHECK_EQ(source.Stride, 4096u);
  CHECK_EQ(source.PixelFormat, a8r8g8b8);
  CHECK_EQ(source.ColorBasis, srgb);
  CHECK_EQ(source.PixelValueAccessMode, direct);
  DisplayMode(1024u, 768u).describe(source, a8b8g8r8);
  CHECK_EQ(source.PixelFormat, a8b8g8r8);
  CHECK_EQ(source.Stride, 4096u); }

/* The desktop's format first - cdd.dll keeps only it of the two - and the
 * red-first one DXGI lists R8G8B8A8 modes from. */
TEST(display_mode_describe, each_mode_is_offered_blue_first_then_red_first)
{ CHECK_EQ(DisplayMode::source_format_count, 2u);
  CHECK_EQ(DisplayMode::source_formats[0], a8r8g8b8);
  CHECK_EQ(DisplayMode::source_formats[1], a8b8g8r8); }

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
  mode.describe(source, a8r8g8b8);
  CHECK(mode.matches(source));
  source.PixelFormat = a8b8g8r8;
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
 * names edges and blanks in twelve bits. Its clock, up to 655.35 MHz, limits
 * the rate it can state and not the mode (edid_timing). */
TEST(display_mode, what_windows_can_be_offered)
{ CHECK(av::DisplayMode(100u, 100u).offerable());
  CHECK(!av::DisplayMode(99u, 100u).offerable());
  CHECK(!av::DisplayMode(100u, 99u).offerable());
  CHECK(!av::DisplayMode(80u, 80u).offerable());
  CHECK(av::DisplayMode(3840u, 2160u).offerable());
  CHECK(av::DisplayMode(4095u, 2160u).offerable());
  CHECK(!av::DisplayMode(4096u, 2160u).offerable());
  CHECK(!av::DisplayMode(1000u, 4096u).offerable());
  CHECK(av::DisplayMode(4095u, 4095u).offerable());
  CHECK(av::DisplayMode(2560u, 1440u, 360000u).offerable());
  /* A slot's default is the smallest standard mode. */
  CHECK_EQ(av::DisplayMode().width(), 640u);
  CHECK_EQ(av::DisplayMode().height(), 480u); }

/* A vertical blank starts where the active lines end: the line at the blank
 * is height(), the blank lines follow it, and the next frame's line 0 comes
 * a blank's worth of the period later. */
TEST(display_mode_scan_line_within, the_blank_begins_the_period_and_line_zero_follows_it)
{ const DisplayMode mode(1920u, 1080u);   /* 1125 lines, 45 of them blank */
  const std::uint64_t period = 1125u * 100u;
  CHECK_EQ(mode.scan_line_within(0u, period), 1080u);
  CHECK_EQ(mode.scan_line_within(44u * 100u, period), 1124u);
  CHECK_EQ(mode.scan_line_within(45u * 100u, period), 0u);
  CHECK_EQ(mode.scan_line_within(45u * 100u + 99u, period), 0u);
  CHECK_EQ(mode.scan_line_within(period - 1u, period), 1079u);
  CHECK_EQ(mode.scan_line_within(period, period), 1080u);            /* a whole period on: the next blank */
  CHECK_EQ(mode.scan_line_within(period + 45u * 100u, period), 0u);
  CHECK_EQ(mode.scan_line_within(12345u, 0u), 1080u); }               /* no period: always in the blank */

/* Four bytes a pixel, a part page counting whole: what a scanout's request
 * is sized from, and what the primary's allocation is asked for. */
TEST(display_mode, frame_pages_count_a_part_page_whole)
{ CHECK_EQ(DisplayMode(640u, 480u).frame_pages(), 300u);      /* exactly 300 pages */
  CHECK_EQ(DisplayMode(2560u, 1440u).frame_pages(), 3600u);
  CHECK_EQ(DisplayMode(1366u, 768u).frame_pages(), 1025u);    /* 1024.5 */
  CHECK_EQ(DisplayMode(1u, 1u).frame_pages(), 1u);
  CHECK_EQ(DisplayMode(0u, 0u).frame_pages(), 0u); }

/* The rate the device states is the mode's: exact fractions with a
 * denominator of 1000 at most, a pixel clock of the whole frame times it. */
TEST(display_mode_describe, a_signal_is_at_the_refresh_the_mode_was_made_at)
{ D3dkmdtVideoSignalInfoRecord signal = {};
  const DisplayMode            mode(2560u, 1440u, 359980u);
  CHECK_EQ(mode.refresh_millihz(), 359980u);
  mode.describe(signal);
  CHECK_EQ(signal.VSyncFreqNumerator, 17999u);
  CHECK_EQ(signal.VSyncFreqDenominator, 50u);
  CHECK_EQ(signal.HSyncFreqNumerator, 539970u);
  CHECK_EQ(signal.HSyncFreqDenominator, 1u);
  CHECK_EQ(signal.PixelRate, (2720ull * 1500ull * 359980ull + 500ull) / 1000ull);
  DisplayMode(1920u, 1080u, 144000u).describe(signal);
  CHECK_EQ(signal.VSyncFreqNumerator, 144u);
  CHECK_EQ(signal.VSyncFreqDenominator, 1u);
  CHECK_EQ(signal.HSyncFreqNumerator, 1125u * 144u);
  CHECK_EQ(signal.PixelRate, 2080ull * 1125ull * 144ull); }

/* 7680x4320 at 1000 Hz is 35.28 GHz of pixel clock and 4.5 MHz of line rate:
 * the 64-bit pixel rate holds, and no fraction wraps its 32-bit words. */
TEST(display_mode_describe, the_largest_mode_at_the_fastest_rate_overflows_nothing)
{ D3dkmdtVideoSignalInfoRecord signal = {};
  const DisplayMode            mode(7680u, 4320u, 1000000u);
  mode.describe(signal);
  CHECK_EQ(signal.PixelRate, 7840ull * 4500ull * 1000ull);
  CHECK_EQ(signal.VSyncFreqNumerator, 1000u);
  CHECK_EQ(signal.VSyncFreqDenominator, 1u);
  CHECK_EQ(signal.HSyncFreqNumerator, 4500u * 1000u);
  CHECK_EQ(signal.HSyncFreqDenominator, 1u);
  /* A numerator the 32 bits cannot hold loses precision and not range. */
  D3dkmdtVideoSignalInfoRecord tall = {};
  DisplayMode(100u, 16384u, 999999u).describe(tall);
  CHECK(tall.HSyncFreqDenominator >= 1u);
  const double lines = double(tall.HSyncFreqNumerator) / double(tall.HSyncFreqDenominator);
  CHECK(lines > 17000.0 * 999.0 && lines < 17100.0 * 1000.0); }

/* A saved configuration at another rate is not the mode the device states. */
TEST(display_mode_matches, a_signal_at_its_refresh_and_no_other)
{ const DisplayMode mode(1920u, 1080u, 144000u);
  D3dkmdtVideoSignalInfoRecord signal = {};
  mode.describe(signal);
  CHECK(mode.matches(signal));
  D3dkmdtVideoSignalInfoRecord saved = {};
  DisplayMode(1920u, 1080u).describe(saved);
  CHECK(!mode.matches(saved));
  CHECK(DisplayMode(1920u, 1080u).matches(saved)); }

/* An EDID's detailed timing stops at 655.35 MHz: a mode too fast for it is
 * stated at the fastest rate that fits, and one that fits is itself. */
TEST(display_mode, the_edid_states_the_fastest_rate_its_clock_holds)
{ const DisplayMode fits(1920u, 1080u, 240000u);
  CHECK_EQ(fits.edid_timing().refresh_millihz(), 240000u);
  const DisplayMode fast = DisplayMode(2560u, 1440u, 360000u).edid_timing();
  CHECK_EQ(fast.width(), 2560u);
  CHECK(fast.refresh_millihz() < 360000u);
  CHECK(fast.pixel_rate() / 10000u <= 65535u);
  /* And nothing slower than the clock forces: within two millihertz of a frame's worth of it. */
  CHECK(fast.pixel_rate() <= 655'350'000ull && 655'350'000ull - fast.pixel_rate() < 2ull * 2720ull * 1500ull / 1000ull);
  const DisplayMode huge = DisplayMode(7680u, 4320u, 1000000u).edid_timing();
  CHECK(huge.refresh_millihz() >= 1000u); }

TEST(display_mode, a_period_is_in_the_100_ns_interrupt_time_units)
{ CHECK_EQ(DisplayMode(1920u, 1080u).period(), second / 60u);
  CHECK_EQ(DisplayMode(1920u, 1080u, 360000u).period(), second / 360u);
  CHECK_EQ(DisplayMode(1920u, 1080u, 1000000u).period(), second / 1000u); }

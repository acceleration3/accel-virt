#include "core/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/display/display_info_response.hpp"
#include "core/display/display_mode_list.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

using av::D3dkmdtVideoSignalInfoRecord;
using av::D3dkmdtVidPnSourceModeRecord;
using av::DisplayMode;
using av::DisplayInfoResponse;
using av::DisplayModeList;

TEST(display_mode_list, holds_common_resolutions_up_to_1080p_in_ascending_area)
{ const DisplayModeList modes;
  CHECK_EQ(modes.count(), DisplayModeList::standard_count);
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
  DisplayMode(1024u, 768u).describe(source, DisplayMode::source_formats[0]);
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

namespace {

/* A display-info answer with one enabled scanout of that size. */
class Reported {
public:
    Reported(std::uint32_t scanout, std::uint32_t width, std::uint32_t height) : bytes_{}
    {
        write(0u, av::DisplayInfoResponse::response_type);
        const std::size_t at = av::DisplayInfoResponse::header_size +
                               scanout * av::DisplayInfoResponse::scanout_size;
        write(at + 8u, width);
        write(at + 12u, height);
        write(at + 16u, 1u);
        if (info_.decode(bytes_, sizeof bytes_) != av::Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "decode");
    }
    const av::DisplayInfoResponse &info() const { return info_; }

private:
    void write(std::size_t offset, std::uint32_t value)
    {
        for (unsigned byte = 0u; byte < 4u; byte++)
            bytes_[offset + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
    }

    std::uint8_t            bytes_[av::DisplayInfoResponse::encoded_size];
    av::DisplayInfoResponse info_;
};

}  // namespace

TEST(display_mode_list_follow, prefers_the_size_the_device_reports)
{ DisplayModeList modes;
  const Reported device(0u, 1600u, 900u);
  CHECK_STATUS(modes.follow(device.info(), 0u), av::Status::ok);
  CHECK_EQ(modes.preferred().width(), 1600u);
  CHECK_EQ(modes.preferred().height(), 900u);
  CHECK(modes.is_preferred(modes.preferred())); }

/* A mode larger than the display in either direction is not offered: dxgkrnl
 * would commit it and the host would scan out a rectangle it does not have. */
TEST(display_mode_list_follow, offers_only_the_modes_that_fit_the_display)
{ DisplayModeList modes;
  const Reported device(0u, 1366u, 768u);
  CHECK_STATUS(modes.follow(device.info(), 0u), av::Status::ok);
  bool saw_1366 = false;
  for (std::uint32_t index = 0u; index < modes.count(); index++) {
      CHECK(modes.mode(index).width() <= 1366u);
      CHECK(modes.mode(index).height() <= 768u);
      saw_1366 = saw_1366 || (modes.mode(index).width() == 1366u && modes.mode(index).height() == 768u);
  }
  CHECK(saw_1366);
  D3dkmdtVideoSignalInfoRecord signal = {};
  DisplayMode(1920u, 1080u).describe(signal);
  CHECK(modes.find(signal) == nullptr);
  DisplayMode(1280u, 720u).describe(signal);
  CHECK(modes.find(signal) != nullptr); }

/* A size that is nobody's standard mode is still the one the device has. */
TEST(display_mode_list_follow, adds_a_size_that_is_not_a_standard_mode)
{ DisplayModeList modes;
  const Reported device(0u, 1234u, 765u);
  CHECK_STATUS(modes.follow(device.info(), 0u), av::Status::ok);
  CHECK_EQ(modes.preferred().width(), 1234u);
  D3dkmdtVideoSignalInfoRecord signal = {};
  DisplayMode(1234u, 765u).describe(signal);
  CHECK(modes.find(signal) != nullptr);
  for (std::uint32_t index = 1u; index < modes.count(); index++)
      CHECK(modes.mode(index).width() * modes.mode(index).height() >=
            modes.mode(index - 1u).width() * modes.mode(index - 1u).height()); }

/* A display smaller than every standard mode offers just its own. */
TEST(display_mode_list_follow, offers_the_display_alone_when_nothing_standard_fits)
{ DisplayModeList modes;
  const Reported device(0u, 320u, 200u);
  CHECK_STATUS(modes.follow(device.info(), 0u), av::Status::ok);
  CHECK_EQ(modes.count(), 1u);
  CHECK_EQ(modes.mode(0u).width(), 320u);
  CHECK(modes.is_preferred(modes.mode(0u))); }

/* Nothing the device says can leave the driver with no modes to offer. */
TEST(display_mode_list_follow, keeps_the_list_a_device_says_nothing_about)
{ DisplayModeList modes;
  const Reported device(1u, 1024u, 768u);
  CHECK_STATUS(modes.follow(device.info(), 0u), av::Status::not_found);
  CHECK_EQ(modes.count(), DisplayModeList::standard_count);
  CHECK_EQ(modes.preferred().width(), 1280u);
  CHECK_STATUS(modes.follow(device.info(), DisplayInfoResponse::max_scanouts), av::Status::not_found);
  CHECK_EQ(modes.count(), DisplayModeList::standard_count); }

/* The device's own size is offered once, not twice. */
TEST(display_mode_list_follow, offers_a_standard_size_the_device_has_once)
{ DisplayModeList modes;
  const Reported device(0u, 1280u, 800u);
  CHECK_STATUS(modes.follow(device.info(), 0u), av::Status::ok);
  std::uint32_t seen = 0u;
  for (std::uint32_t index = 0u; index < modes.count(); index++)
      seen += modes.mode(index).width() == 1280u && modes.mode(index).height() == 800u ? 1u : 0u;
  CHECK_EQ(seen, 1u);
  CHECK(modes.is_preferred(modes.preferred()));
  CHECK_EQ(modes.preferred().width(), 1280u); }

/* Following twice does not grow the list. */
TEST(display_mode_list_follow, replaces_what_it_offered_before)
{ DisplayModeList modes;
  const Reported wide(0u, 1920u, 1080u);
  CHECK_STATUS(modes.follow(wide.info(), 0u), av::Status::ok);
  const std::uint32_t offered = modes.count();
  CHECK_STATUS(modes.follow(wide.info(), 0u), av::Status::ok);
  CHECK_EQ(modes.count(), offered);
  const Reported narrow(0u, 800u, 600u);
  CHECK_STATUS(modes.follow(narrow.info(), 0u), av::Status::ok);
  CHECK(modes.count() < offered);
  CHECK_EQ(modes.preferred().width(), 800u); }

namespace av {

/* The states the invariant guards, which correct code never reaches. */
struct DisplayModeListProbe {
    static void offer_nothing(DisplayModeList &modes) { modes.count_ = 0u; }
    static void offer_more_than_it_has(DisplayModeList &modes) { modes.count_ = DisplayModeList::max_modes + 1u; }
    static void prefer_past_the_end(DisplayModeList &modes) { modes.preferred_index_ = modes.count_; }
    static void put_the_largest_first(DisplayModeList &modes)
    {
        const DisplayMode first = modes.modes_[0];
        modes.modes_[0] = modes.modes_[modes.count_ - 1u];
        modes.modes_[modes.count_ - 1u] = first;
    }
};

}  // namespace av

TEST(display_mode_list_invariants, an_empty_list_a_preference_past_it_and_a_mode_out_of_order_are_detected)
{ DisplayModeList modes;
  CHECK(modes.invariants_hold());
  av::DisplayModeListProbe::prefer_past_the_end(modes);
  CHECK(!modes.invariants_hold());
  DisplayModeList empty;
  av::DisplayModeListProbe::offer_nothing(empty);
  CHECK(!empty.invariants_hold());
  DisplayModeList unordered;
  av::DisplayModeListProbe::put_the_largest_first(unordered);
  CHECK(!unordered.invariants_hold());
  DisplayModeList overflowing;
  av::DisplayModeListProbe::offer_more_than_it_has(overflowing);
  CHECK(!overflowing.invariants_hold()); }

/* ---- the host's own modes -------------------------------------------------- */

namespace {

av::DisplayModeSet offered(std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> modes, std::uint32_t preferred)
{
    av::DisplayModeSet set;
    for (const auto &mode : modes) CHECK_STATUS(set.add(mode.first, mode.second), av::Status::ok);
    CHECK_STATUS(set.prefer(preferred), av::Status::ok);
    return set;
}

}  // namespace

TEST(display_mode_list_adopt, the_hosts_modes_in_ascending_area_with_its_preferred_one)
{ DisplayModeList modes;
  CHECK_STATUS(modes.adopt(offered({{2560u, 1440u}, {100u, 100u}, {1920u, 1080u}, {1080u, 1920u}}, 0u)), av::Status::ok);
  CHECK_EQ(modes.count(), 4u);
  CHECK_EQ(modes.mode(0u).width(), 100u);
  /* The same area twice keeps the host's order. */
  CHECK_EQ(modes.mode(1u).width(), 1920u);
  CHECK_EQ(modes.mode(2u).width(), 1080u);
  CHECK_EQ(modes.mode(3u).width(), 2560u);
  CHECK_EQ(modes.preferred().width(), 2560u);
  CHECK(modes.is_preferred(modes.mode(3u)));
  CHECK_STATUS(modes.adopt(offered({{2560u, 1440u}, {100u, 100u}}, 1u)), av::Status::ok);
  CHECK_EQ(modes.preferred().width(), 100u);
  CHECK(modes.invariants_hold()); }

/* dxgkrnl refuses a source mode under 100 pixels an edge, and one refusal
 * would fail the whole source mode set - the display with it. */
TEST(display_mode_list_adopt, a_mode_windows_refuses_as_too_small_is_left_out)
{ DisplayModeList modes;
  CHECK_STATUS(modes.adopt(offered({{80u, 80u}, {1920u, 1080u}, {99u, 1080u}, {1280u, 720u}}, 0u)), av::Status::ok);
  CHECK_EQ(modes.count(), 2u);
  CHECK_EQ(modes.mode(0u).width(), 1280u);
  CHECK_EQ(modes.preferred().width(), 1920u);
  CHECK_STATUS(modes.adopt(offered({{80u, 80u}}, 0u)), av::Status::not_found);
  CHECK_EQ(modes.count(), 2u);
  CHECK(modes.invariants_hold()); }

TEST(display_mode_list_adopt, a_mode_the_edid_cannot_describe_is_left_out)
{ DisplayModeList modes;
  /* Past 4095 an edge; the preferred one among them, so the largest left is preferred. */
  CHECK_STATUS(modes.adopt(offered({{1920u, 1080u}, {4096u, 2160u}, {3840u, 2160u}, {5120u, 2880u}}, 3u)),
               av::Status::ok);
  CHECK_EQ(modes.count(), 2u);
  CHECK_EQ(modes.preferred().width(), 3840u);
  /* Nothing describable leaves the list as it was. */
  CHECK_STATUS(modes.adopt(offered({{4096u, 4096u}, {8000u, 100u}}, 0u)), av::Status::not_found);
  CHECK_EQ(modes.count(), 2u);
  CHECK_EQ(modes.preferred().width(), 3840u);
  CHECK(modes.invariants_hold()); }

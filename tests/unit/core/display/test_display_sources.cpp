#include "core/display/display_mode.hpp"
#include "core/display/display_sources.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::DisplaySources;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

TEST(display_sources_set_private_format, remembers_it_per_source)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_EQ(sources.private_format(0u), 0u);
  CHECK_STATUS(sources.set_private_format(0u, 0x1234u), Status::ok);
  CHECK_EQ(sources.private_format(0u), 0x1234u); }

TEST(display_sources_set_private_format, a_source_the_adapter_does_not_have_is_invalid)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_STATUS(sources.set_private_format(sources.count(), 1u), Status::invalid_parameter);
  CHECK_EQ(sources.private_format(sources.count()), 0u); }

TEST(display_sources_commit, remembers_the_mode_until_another_or_none)
{ MockPlatform platform; DisplaySources sources(platform);
  const av::DisplayMode first(640u, 480u), second(800u, 600u);
  CHECK(sources.committed_mode(0u) == nullptr);
  CHECK_STATUS(sources.commit(0u, &first), Status::ok);
  CHECK(sources.committed_mode(0u) == &first);
  CHECK_STATUS(sources.commit(0u, &second), Status::ok);
  CHECK(sources.committed_mode(0u) == &second);
  CHECK_STATUS(sources.commit(0u, nullptr), Status::ok);
  CHECK(sources.committed_mode(0u) == nullptr); }

TEST(display_sources_commit, a_source_the_adapter_does_not_have_is_invalid)
{ MockPlatform platform; DisplaySources sources(platform);
  const av::DisplayMode mode(640u, 480u);
  CHECK_STATUS(sources.commit(sources.count(), &mode), Status::invalid_parameter);
  CHECK(sources.committed_mode(sources.count()) == nullptr); }

TEST(display_sources_set_vsync, records_whether_vsync_interrupts_are_wanted)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(!sources.vsync_enabled());
  sources.set_vsync(true);
  CHECK(sources.vsync_enabled());
  sources.set_vsync(false);
  CHECK(!sources.vsync_enabled()); }

TEST(display_sources_set_scanout, remembers_the_primary_address_and_allocation_until_cleared)
{ MockPlatform platform; DisplaySources sources(platform);
  std::uint32_t allocation = 0u;
  CHECK_EQ(sources.scanout_address(0u), 0u);
  CHECK(sources.primary(0u) == nullptr);
  CHECK_STATUS(sources.set_scanout(0u, 0x123456789000ll, &allocation), Status::ok);
  CHECK_EQ(sources.scanout_address(0u), 0x123456789000ull);
  CHECK(sources.primary(0u) == &allocation);
  CHECK_STATUS(sources.set_scanout(0u, 0, nullptr), Status::ok);
  CHECK_EQ(sources.scanout_address(0u), 0u);
  CHECK(sources.primary(0u) == nullptr); }

TEST(display_sources_set_scanout, a_source_the_adapter_does_not_have_is_invalid)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_STATUS(sources.set_scanout(sources.count(), 0x1000, nullptr), Status::invalid_parameter);
  CHECK_EQ(sources.scanout_address(sources.count()), 0u);
  CHECK(sources.primary(sources.count()) == nullptr); }

/* The DPC reads the handle; an allocation being destroyed must not leave one
 * behind for it to follow. */
TEST(display_sources_forget_primary, clears_the_source_showing_that_allocation)
{ MockPlatform platform; DisplaySources sources(platform);
  std::uint32_t allocation = 0u;
  std::uint32_t other = 0u;
  CHECK_STATUS(sources.set_scanout(0u, 0x4000, &allocation), Status::ok);
  sources.forget_primary(&other);
  CHECK_EQ(sources.scanout_address(0u), 0x4000u);
  sources.forget_primary(&allocation);
  CHECK_EQ(sources.scanout_address(0u), 0u);
  CHECK(sources.primary(0u) == nullptr); }

TEST(display_sources_set_visible, remembers_visibility_per_source)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(!sources.visible(0u));
  CHECK_STATUS(sources.set_visible(0u, true), Status::ok);
  CHECK(sources.visible(0u));
  CHECK_STATUS(sources.set_visible(sources.count(), true), Status::invalid_parameter);
  CHECK(!sources.visible(sources.count())); }

TEST(display_sources, scanout_is_readable_and_writable_at_device_irql)
{ MockPlatform platform; DisplaySources sources(platform);
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.set_scanout(0u, 0x2000, nullptr), Status::ok);
  CHECK_EQ(sources.scanout_address(0u), 0x2000u);
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(display_sources, is_passive_only)
{ MockPlatform platform; DisplaySources sources(platform);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(sources.set_private_format(0u, 1u));
  static_cast<void>(sources.private_format(0u));
  static_cast<void>(sources.commit(0u, nullptr));
  static_cast<void>(sources.committed_mode(0u));
  sources.set_vsync(true);
  static_cast<void>(sources.vsync_enabled());
  static_cast<void>(sources.set_visible(0u, true));
  /* visible() is asked from the DPC that carries the frame, so DISPATCH is its
   * ceiling and this one is not a violation. */
  static_cast<void>(sources.visible(0u));
  platform.set_irql(Irql::passive);
  for (int call = 0; call < 6; call++) CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(!platform.claim(Violation::irql_ceiling)); }

/* The device says how many displays it has; the driver's arrays say how many it
 * can offer, and a count outside that is clamped with the value answered. */
TEST(display_sources_set_count, follows_the_device_up_to_what_the_arrays_hold)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_EQ(sources.count(), 1u);
  CHECK_STATUS(sources.set_count(2u), Status::ok);
  CHECK_EQ(sources.count(), 2u);
  CHECK_STATUS(sources.set_count(DisplaySources::max_sources), Status::ok);
  CHECK_EQ(sources.count(), DisplaySources::max_sources);
  CHECK_STATUS(sources.set_count(DisplaySources::max_sources + 1u), Status::too_large);
  CHECK_EQ(sources.count(), DisplaySources::max_sources);
  CHECK_STATUS(sources.set_count(0u), Status::invalid_parameter);
  CHECK_EQ(sources.count(), DisplaySources::max_sources);
  CHECK(sources.invariants_hold()); }

/* Every source the device reports keeps its own mode, primary and visibility. */
TEST(display_sources, keeps_each_output_apart)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_STATUS(sources.set_count(2u), Status::ok);
  const av::DisplayMode first(1280u, 800u);
  const av::DisplayMode second(1024u, 768u);
  CHECK_STATUS(sources.commit(0u, &first), Status::ok);
  CHECK_STATUS(sources.commit(1u, &second), Status::ok);
  CHECK_STATUS(sources.set_scanout(0u, 0x1000, nullptr), Status::ok);
  CHECK_STATUS(sources.set_visible(1u, true), Status::ok);
  CHECK_EQ(sources.committed_mode(0u)->width(), 1280u);
  CHECK_EQ(sources.committed_mode(1u)->width(), 1024u);
  CHECK_EQ(sources.scanout_address(0u), 0x1000u);
  CHECK_EQ(sources.scanout_address(1u), 0u);
  CHECK(!sources.visible(0u));
  CHECK(sources.visible(1u));
  CHECK_STATUS(sources.commit(2u, &first), Status::invalid_parameter); }

namespace av {

/* The state the invariant guards, which correct code never reaches. */
struct DisplaySourcesProbe {
    static void offer_nothing(DisplaySources &sources) { sources.count_ = 0u; }
    static void offer_more_than_it_holds(DisplaySources &sources)
    { sources.count_ = DisplaySources::max_sources + 1u; }
};

}  // namespace av

TEST(display_sources_invariants, no_outputs_and_more_than_it_holds_are_detected)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(sources.invariants_hold());
  av::DisplaySourcesProbe::offer_nothing(sources);
  CHECK(!sources.invariants_hold());
  DisplaySources other(platform);
  av::DisplaySourcesProbe::offer_more_than_it_holds(other);
  CHECK(!other.invariants_hold()); }

TEST(display_sources_report, writes_each_source_s_format_mode_scanout_and_visibility)
{ MockPlatform platform; DisplaySources sources(platform);
  const av::DisplayMode mode(1280u, 800u);
  std::uint32_t primary = 0u;
  CHECK_STATUS(sources.set_count(2u), Status::ok);
  CHECK_STATUS(sources.set_private_format(0u, 0x15u), Status::ok);
  CHECK_STATUS(sources.commit(0u, &mode), Status::ok);
  CHECK_STATUS(sources.set_scanout(0u, 0x7000, &primary), Status::ok);
  CHECK_STATUS(sources.set_visible(1u, true), Status::ok);
  sources.set_vsync(true);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  sources.report(writer);
  char expected[512];
  std::snprintf(expected, sizeof expected,
                "display_sources/0 count=2 vsync=true\n"
                "display_sources/0/source/0 format=21 width=1280 height=800 scanout=0x0000000000007000"
                " primary=0x%016llX visible=false\n"
                "display_sources/0/source/1 format=0 width=0 height=0 scanout=0x0000000000000000"
                " primary=0x0000000000000000 visible=true",
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&primary)));
  CHECK(std::strcmp(writer.dump(), expected) == 0); }

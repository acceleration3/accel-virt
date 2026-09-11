#include "../../src/core/display_mode.hpp"
#include "../../src/core/display_sources.hpp"
#include "../../src/core/status.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

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
  CHECK_STATUS(sources.set_private_format(DisplaySources::source_count, 1u), Status::invalid_parameter);
  CHECK_EQ(sources.private_format(DisplaySources::source_count), 0u); }

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
  CHECK_STATUS(sources.commit(DisplaySources::source_count, &mode), Status::invalid_parameter);
  CHECK(sources.committed_mode(DisplaySources::source_count) == nullptr); }

TEST(display_sources_set_vsync, records_whether_vsync_interrupts_are_wanted)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(!sources.vsync_enabled());
  sources.set_vsync(true);
  CHECK(sources.vsync_enabled());
  sources.set_vsync(false);
  CHECK(!sources.vsync_enabled()); }

TEST(display_sources, is_passive_only)
{ MockPlatform platform; DisplaySources sources(platform);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(sources.set_private_format(0u, 1u));
  static_cast<void>(sources.private_format(0u));
  static_cast<void>(sources.commit(0u, nullptr));
  static_cast<void>(sources.committed_mode(0u));
  sources.set_vsync(true);
  static_cast<void>(sources.vsync_enabled());
  platform.set_irql(Irql::passive);
  for (int call = 0; call < 5; call++) CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(!platform.claim(Violation::irql_ceiling)); }

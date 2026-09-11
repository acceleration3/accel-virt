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

TEST(display_sources, is_passive_only)
{ MockPlatform platform; DisplaySources sources(platform);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(sources.set_private_format(0u, 1u));
  static_cast<void>(sources.private_format(0u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling)); }

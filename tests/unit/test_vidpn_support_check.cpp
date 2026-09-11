#include "../../src/core/vidpn_support_check.hpp"
#include "../../src/core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "../../src/core/nt_status.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/vidpn_fixture.hpp"
#include "../harness/violation.hpp"

#include <cstdint>

using av::DxgkargIsSupportedVidPnRecord;
using av::Irql;
using av::VidPnCall;
using av::VidPnFixture;
using av::VidPnSupportCheck;
using av::Violation;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);

/* Decides for the fixture's VidPn; answers the NTSTATUS, and the verdict in `supported`. */
std::int32_t decide(VidPnFixture &fixture, void *vidpn, std::uint8_t &supported)
{
    DxgkargIsSupportedVidPnRecord request = {};
    request.hDesiredVidPn = vidpn;
    request.IsVidPnSupported = 7u;
    const std::int32_t status = VidPnSupportCheck(fixture.platform(), fixture.dxgkrnl(), request).decide().value();
    supported = request.IsVidPnSupported;
    return status;
}

}  // namespace

TEST(vidpn_support_check_decide, no_vidpn_is_supported)
{ VidPnFixture fixture;
  std::uint8_t supported = 0u;
  CHECK_EQ(decide(fixture, nullptr, supported), 0);
  CHECK_EQ(supported, 1u); }

TEST(vidpn_support_check_decide, the_one_path_or_none_is_supported)
{ VidPnFixture fixture;
  std::uint8_t supported = 0u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 1u);
  fixture.vidpn().add_path(0u, 0u);
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 1u); }

TEST(vidpn_support_check_visit, a_second_path_or_another_source_or_target_is_not_supported)
{ const std::uint32_t ends[][2] = {{1u, 0u}, {0u, 1u}};
  for (const auto &end : ends) {
      VidPnFixture fixture;
      fixture.vidpn().add_path(end[0], end[1]);
      std::uint8_t supported = 1u;
      CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
      CHECK_EQ(supported, 0u);
  }
  VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  fixture.vidpn().add_path(0u, 0u);
  std::uint8_t supported = 1u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 0u); }

TEST(vidpn_support_check_decide, a_vidpn_it_cannot_read_fails)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  fixture.vidpn().refuse(VidPnCall::get_topology, no_memory);
  std::uint8_t supported = 1u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), no_memory);
  CHECK_EQ(supported, 0u); }

TEST(vidpn_support_check, is_passive_only)
{ VidPnFixture fixture;
  DxgkargIsSupportedVidPnRecord request = {};
  VidPnSupportCheck check(fixture.platform(), fixture.dxgkrnl(), request);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(check.decide());
  static_cast<void>(check.visit(fixture.vidpn().path(0u)));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

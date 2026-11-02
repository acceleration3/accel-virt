#include "miniport/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "miniport/display/display_mode_list.hpp"
#include "miniport/display/display_sources.hpp"
#include "core/support/nt_status.hpp"
#include "miniport/vidpn/vidpn_commit.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/vidpn_fixture.hpp"

#include <cstdint>

using av::D3dkmdtVidPnSourceModeRecord;
using av::DisplayMode;
using av::DisplayModeList;
using av::DisplaySources;
using av::DxgkargCommitVidPnRecord;
using av::Irql;
using av::Status;
using av::VidPnCall;
using av::VidPnCommit;
using av::VidPnFixture;
using av::Violation;

namespace {

constexpr std::int32_t  no_memory = static_cast<std::int32_t>(0xC0000017u);
constexpr std::uint32_t invalid_parameter = 0xC000000Du;
constexpr std::uint32_t invalid_source_mode = 0xC01E0310u;
constexpr std::uint32_t path_powered_off = 2u;

D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    D3dkmdtVidPnSourceModeRecord mode = {};
    DisplayMode(width, height).describe(mode, DisplayMode::source_formats[0]);
    return mode;
}

class Commit {
public:
    Commit() : fixture_(), modes_(), sources_(fixture_.platform()), request_{}
    {
        request_.hFunctionalVidPn = fixture_.handle();
        fixture_.vidpn().add_path(0u, 0u);
    }
    av::FakeVidPn            &vidpn() { return fixture_.vidpn(); }
    VidPnFixture             &fixture() { return fixture_; }
    DisplayModeList          &modes() { return modes_; }
    DisplaySources           &sources() { return sources_; }
    DxgkargCommitVidPnRecord &request() { return request_; }
    std::uint32_t apply()
    {
        return static_cast<std::uint32_t>(
            VidPnCommit(fixture_.platform(), fixture_.dxgkrnl(), modes_, sources_, request_).apply().value());
    }

private:
    VidPnFixture             fixture_;
    DisplayModeList          modes_;
    DisplaySources           sources_;
    DxgkargCommitVidPnRecord request_;
};

}

TEST(vidpn_commit_apply, records_the_pinned_source_mode)
{ Commit commit;
  commit.vidpn().source_modes(0u).seed(source_mode(1024u, 768u), true);
  CHECK_EQ(commit.apply(), 0u);
  const DisplayMode *committed = commit.sources().committed_mode(0u);
  CHECK(committed == &commit.modes().mode(2u));
  CHECK_EQ(commit.vidpn().source_modes(0u).outstanding(), 0u); }

TEST(vidpn_commit_apply, a_mode_or_format_the_adapter_lacks_is_an_invalid_source_mode)
{ Commit size;
  size.vidpn().source_modes(0u).seed(source_mode(1000u, 700u), true);
  CHECK_EQ(size.apply(), invalid_source_mode);
  CHECK(size.sources().committed_mode(0u) == nullptr);
  Commit format;
  D3dkmdtVidPnSourceModeRecord mode = source_mode(1024u, 768u);
  mode.PixelFormat = 22;
  format.vidpn().source_modes(0u).seed(mode, true);
  CHECK_EQ(format.apply(), invalid_source_mode); }

TEST(vidpn_commit_apply, no_path_or_no_pinned_mode_leaves_the_source_without_a_mode)
{ Commit unpinned;
  unpinned.vidpn().source_modes(0u).seed(source_mode(1024u, 768u), true);
  CHECK_EQ(unpinned.apply(), 0u);
  unpinned.vidpn().source_modes(0u).clear();
  CHECK_EQ(unpinned.apply(), 0u);
  CHECK(unpinned.sources().committed_mode(0u) == nullptr);
  VidPnFixture fixture;
  DisplayModeList modes;
  DisplaySources sources(fixture.platform());
  CHECK_STATUS(sources.commit(0u, &modes.mode(0u)), Status::ok);
  DxgkargCommitVidPnRecord request = {fixture.handle(), 0u, 0, nullptr, 0u, 0u};
  CHECK_EQ(VidPnCommit(fixture.platform(), fixture.dxgkrnl(), modes, sources, request).apply().value(), 0);
  CHECK(sources.committed_mode(0u) == nullptr); }

TEST(vidpn_commit_apply, a_path_powering_off_changes_nothing)
{ Commit commit;
  commit.request().Flags = path_powered_off;
  commit.vidpn().refuse(VidPnCall::get_topology, no_memory);
  CHECK_EQ(commit.apply(), 0u); }

TEST(vidpn_commit_apply, a_source_the_adapter_lacks_is_invalid)
{ Commit commit;
  commit.request().AffectedVidPnSourceId = DisplaySources::max_sources;
  CHECK_EQ(commit.apply(), invalid_parameter); }

TEST(vidpn_commit_apply, every_refusal_fails_without_a_leak)
{ const VidPnCall calls[] = {VidPnCall::get_topology, VidPnCall::paths_from_source,
                             VidPnCall::acquire_source_mode_set, VidPnCall::acquire_pinned_mode};
  for (VidPnCall call : calls) {
      Commit commit;
      commit.vidpn().source_modes(0u).seed(source_mode(1024u, 768u), true);
      commit.vidpn().refuse(call, no_memory);
      CHECK_EQ(commit.apply(), static_cast<std::uint32_t>(no_memory));
      CHECK(commit.sources().committed_mode(0u) == nullptr);
  } }

TEST(vidpn_commit, is_passive_only)
{ Commit commit;
  commit.request().AffectedVidPnSourceId = DisplaySources::max_sources;
  VidPnCommit vidpn_commit(commit.fixture().platform(), commit.fixture().dxgkrnl(), commit.modes(), commit.sources(),
                           commit.request());
  commit.fixture().platform().set_irql(Irql::dispatch);
  static_cast<void>(vidpn_commit.apply());
  commit.fixture().platform().set_irql(Irql::passive);
  CHECK(commit.fixture().platform().claim(Violation::irql_ceiling)); }

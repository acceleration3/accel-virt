#include "../../src/core/vidpn_mode_set.hpp"
#include "../../src/core/vidpn.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/vidpn_fixture.hpp"
#include "../harness/violation.hpp"

#include <cstdint>

using av::D3dkmdtVidPnSourceModeRecord;
using av::D3dkmdtVidPnTargetModeRecord;
using av::Irql;
using av::SourceModeSet;
using av::Status;
using av::TargetModeSet;
using av::VidPn;
using av::VidPnCall;
using av::VidPnFixture;
using av::Violation;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);

D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    D3dkmdtVidPnSourceModeRecord mode = {};
    mode.PrimSurfSizeCx = width;
    mode.PrimSurfSizeCy = height;
    mode.PixelFormat = 22;
    return mode;
}

/* A fixture with its VidPn initialized. */
class Session {
public:
    Session() : fixture_(), vidpn_(fixture_.platform(), fixture_.dxgkrnl(), fixture_.handle())
    {
        CHECK_STATUS(vidpn_.initialize(), Status::ok);
    }
    VidPnFixture &fixture() { return fixture_; }
    av::FakeVidPn &fake() { return fixture_.vidpn(); }
    av::MockPlatform &platform() { return fixture_.platform(); }
    VidPn &vidpn() { return vidpn_; }

private:
    VidPnFixture fixture_;
    VidPn        vidpn_;
};

}  // namespace

TEST(vidpn_mode_set_copy_pinned, copies_the_pinned_mode_and_releases_it)
{ Session session;
  session.fake().source_modes(0u).seed(source_mode(800u, 600u), false);
  session.fake().source_modes(0u).seed(source_mode(1024u, 768u), true);
  SourceModeSet set(session.platform(), session.vidpn(), 0u);
  CHECK_STATUS(set.acquire(), Status::ok);
  D3dkmdtVidPnSourceModeRecord pinned = {};
  CHECK_STATUS(set.copy_pinned(pinned), Status::ok);
  CHECK_EQ(pinned.PrimSurfSizeCx, 1024u);
  CHECK_EQ(session.fake().source_modes(0u).outstanding(), 0u); }

TEST(vidpn_mode_set_copy_pinned, nothing_pinned_is_not_found)
{ Session session;
  session.fake().source_modes(0u).seed(source_mode(800u, 600u), false);
  SourceModeSet set(session.platform(), session.vidpn(), 0u);
  CHECK_STATUS(set.acquire(), Status::ok);
  D3dkmdtVidPnSourceModeRecord pinned = {};
  CHECK_STATUS(set.copy_pinned(pinned), Status::not_found);
  session.fake().refuse(VidPnCall::acquire_pinned_mode, no_memory);
  CHECK_STATUS(set.copy_pinned(pinned), Status::no_memory); }

TEST(vidpn_mode_set, refuses_to_use_a_set_it_does_not_hold)
{ Session session;
  SourceModeSet set(session.platform(), session.vidpn(), 0u);
  D3dkmdtVidPnSourceModeRecord mode = source_mode(640u, 480u);
  CHECK_STATUS(set.copy_pinned(mode), Status::invalid_state);
  CHECK_STATUS(set.add(mode), Status::invalid_state);
  CHECK_STATUS(set.assign(), Status::invalid_state);
  CHECK_STATUS(set.acquire(), Status::ok);
  CHECK_STATUS(set.acquire(), Status::invalid_state);
  CHECK_STATUS(set.create(), Status::invalid_state); }

TEST(vidpn_mode_set_add, fills_a_new_set_that_assign_makes_current)
{ Session session;
  session.fake().source_modes(0u).seed(source_mode(800u, 600u), true);
  { SourceModeSet set(session.platform(), session.vidpn(), 0u);
    CHECK_STATUS(set.create(), Status::ok);
    CHECK_STATUS(set.add(source_mode(1920u, 1080u)), Status::ok);
    CHECK_STATUS(set.add(source_mode(1280u, 800u)), Status::ok);
    CHECK_STATUS(set.add(source_mode(1920u, 1080u)), Status::ok);
    CHECK_STATUS(set.assign(), Status::ok);
    CHECK_STATUS(set.assign(), Status::invalid_state); }
  CHECK_EQ(session.fake().source_modes(0u).modes(), 2u);
  CHECK_EQ(session.fake().source_modes(0u).mode(1u).PrimSurfSizeCx, 1280u);
  CHECK(session.fake().source_modes(0u).mode(0u).Id != session.fake().source_modes(0u).mode(1u).Id);
  CHECK(!session.fake().source_modes(0u).pinned()); }

TEST(vidpn_mode_set_add, a_refused_mode_info_or_add_fails_without_a_leak)
{ Session session;
  SourceModeSet set(session.platform(), session.vidpn(), 0u);
  CHECK_STATUS(set.create(), Status::ok);
  session.fake().refuse(VidPnCall::add_mode, no_memory);
  CHECK_STATUS(set.add(source_mode(640u, 480u)), Status::no_memory);
  session.fake().refuse(VidPnCall::create_mode_info, no_memory);
  CHECK_STATUS(set.add(source_mode(640u, 480u)), Status::no_memory);
  session.fake().refuse(VidPnCall::create_mode_info, 0);
  CHECK_STATUS(set.add(source_mode(640u, 480u)), Status::malformed); }

TEST(vidpn_mode_set, a_refused_acquire_create_or_assign_fails_without_a_leak)
{ Session session;
  SourceModeSet acquired(session.platform(), session.vidpn(), 0u);
  session.fake().refuse(VidPnCall::acquire_source_mode_set, no_memory);
  CHECK_STATUS(acquired.acquire(), Status::no_memory);
  session.fake().refuse(VidPnCall::create_source_mode_set, no_memory);
  CHECK_STATUS(acquired.create(), Status::no_memory);
  TargetModeSet assigned(session.platform(), session.vidpn(), 0u);
  CHECK_STATUS(assigned.create(), Status::ok);
  session.fake().refuse(VidPnCall::assign_target_mode_set, no_memory);
  CHECK_STATUS(assigned.assign(), Status::no_memory); }

TEST(vidpn_mode_set, a_target_mode_set_works_the_same_way)
{ Session session;
  { av::VidPnModeSet<D3dkmdtVidPnTargetModeRecord, av::DxgkVidPnTargetModeSetInterfaceRecord> set(
        session.platform(), session.vidpn(), 0u);
    CHECK_STATUS(set.create(), Status::ok);
    D3dkmdtVidPnTargetModeRecord mode = {};
    mode.VideoSignalInfo.ActiveSizeCx = 1280u;
    mode.VideoSignalInfo.ActiveSizeCy = 800u;
    mode.WireFormatAndPreference = 1u;
    CHECK_STATUS(set.add(mode), Status::ok);
    CHECK_STATUS(set.assign(), Status::ok); }
  CHECK_EQ(session.fake().target_modes(0u).modes(), 1u);
  CHECK_EQ(session.fake().target_modes(0u).mode(0u).WireFormatAndPreference, 1u);
  TargetModeSet set(session.platform(), session.vidpn(), 0u);
  CHECK_STATUS(set.acquire(), Status::ok);
  D3dkmdtVidPnTargetModeRecord pinned = {};
  CHECK_STATUS(set.copy_pinned(pinned), Status::not_found); }

TEST(vidpn_mode_set, is_passive_only)
{ Session session;
  SourceModeSet set(session.platform(), session.vidpn(), 0u);
  D3dkmdtVidPnSourceModeRecord mode = {};
  session.platform().set_irql(Irql::dispatch);
  static_cast<void>(set.acquire());
  static_cast<void>(set.create());
  static_cast<void>(set.assign());
  static_cast<void>(set.copy_pinned(mode));
  static_cast<void>(set.add(mode));
  session.platform().set_irql(Irql::passive);
  /* Each of the five checks its ceiling; those that reach the VidPn check again. */
  unsigned claimed = 0u;
  while (session.platform().claim(Violation::irql_ceiling)) claimed++;
  CHECK_EQ(claimed, 7u); }

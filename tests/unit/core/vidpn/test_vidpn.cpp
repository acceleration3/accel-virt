#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/ddi/dxgk_vidpn_interface_records.hpp"
#include "core/vidpn/path_visitor.hpp"
#include "core/vidpn/vidpn.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/vidpn_fixture.hpp"

#include <cstdint>

using av::D3dkmdtVidPnPresentPathRecord;
using av::DxgkVidPnSourceModeSetInterfaceRecord;
using av::DxgkVidPnTargetModeSetInterfaceRecord;
using av::Irql;
using av::PathVisitor;
using av::Status;
using av::VidPn;
using av::VidPnCall;
using av::VidPnFixture;
using av::Violation;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);

class Recorder final : public PathVisitor {
public:
    explicit Recorder(Status answer) : answer_(answer), visits_(0u), last_target_(0u) {}
    Status visit(const D3dkmdtVidPnPresentPathRecord &path) override
    {
        visits_++;
        last_target_ = path.VidPnTargetId;
        return answer_;
    }
    std::uint32_t visits() const { return visits_; }
    std::uint32_t last_target() const { return last_target_; }

private:
    Status        answer_;
    std::uint32_t visits_;
    std::uint32_t last_target_;
};

}  // namespace

TEST(vidpn_initialize, reaches_the_topology_of_the_vidpn)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok); }

TEST(vidpn_initialize, a_null_vidpn_is_invalid)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), nullptr);
  CHECK_STATUS(vidpn.initialize(), Status::invalid_parameter); }

TEST(vidpn_initialize, a_refused_or_empty_topology_fails)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  fixture.vidpn().refuse(VidPnCall::get_topology, no_memory);
  CHECK_STATUS(vidpn.initialize(), Status::no_memory);
  fixture.vidpn().refuse(VidPnCall::get_topology, 0);
  CHECK_STATUS(vidpn.initialize(), Status::malformed);
  std::uint64_t paths = 0u;
  CHECK_STATUS(vidpn.source_paths(0u, paths), Status::invalid_state); }

TEST(vidpn_initialize, fails_when_dxgkrnl_has_no_vidpn_interface)
{ av::MockPlatform platform;
  av::DxgkrnlInterface dxgkrnl(platform);
  av::FakeVidPn fake;
  VidPn vidpn(platform, dxgkrnl, &fake);
  CHECK_STATUS(vidpn.initialize(), Status::invalid_state); }

TEST(vidpn, refuses_every_operation_before_initialize)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  std::uint64_t paths = 0u;
  Recorder recorder(Status::ok);
  void *set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *source_interface = nullptr;
  const DxgkVidPnTargetModeSetInterfaceRecord *target_interface = nullptr;
  CHECK_STATUS(vidpn.source_paths(0u, paths), Status::invalid_state);
  CHECK_STATUS(vidpn.for_each_path(recorder), Status::invalid_state);
  CHECK_STATUS(vidpn.update_path_support(D3dkmdtVidPnPresentPathRecord{}), Status::invalid_state);
  CHECK_STATUS(vidpn.acquire_mode_set(0u, set, source_interface), Status::invalid_state);
  CHECK_STATUS(vidpn.acquire_mode_set(0u, set, target_interface), Status::invalid_state);
  CHECK_STATUS(vidpn.create_mode_set(0u, set, source_interface), Status::invalid_state);
  CHECK_STATUS(vidpn.create_mode_set(0u, set, target_interface), Status::invalid_state);
  CHECK_STATUS(vidpn.release_mode_set(set, source_interface), Status::invalid_state);
  CHECK_STATUS(vidpn.release_mode_set(set, target_interface), Status::invalid_state);
  CHECK_STATUS(vidpn.assign_mode_set(0u, set, source_interface), Status::invalid_state);
  CHECK_STATUS(vidpn.assign_mode_set(0u, set, target_interface), Status::invalid_state); }

TEST(vidpn_source_paths, counts_the_paths_from_a_source)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  std::uint64_t paths = 9u;
  CHECK_STATUS(vidpn.source_paths(0u, paths), Status::ok);
  CHECK_EQ(paths, 1u); }

TEST(vidpn_source_paths, a_source_outside_the_topology_has_none)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  std::uint64_t paths = 9u;
  CHECK_STATUS(vidpn.source_paths(0u, paths), Status::ok);
  CHECK_EQ(paths, 0u);
  fixture.vidpn().refuse(VidPnCall::paths_from_source, no_memory);
  CHECK_STATUS(vidpn.source_paths(0u, paths), Status::no_memory); }

TEST(vidpn_for_each_path, visits_every_path_and_releases_it)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  fixture.vidpn().add_path(0u, 1u);
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  Recorder recorder(Status::ok);
  CHECK_STATUS(vidpn.for_each_path(recorder), Status::ok);
  CHECK_EQ(recorder.visits(), 2u);
  CHECK_EQ(recorder.last_target(), 1u); }

TEST(vidpn_for_each_path, an_empty_topology_visits_nothing)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  Recorder recorder(Status::ok);
  CHECK_STATUS(vidpn.for_each_path(recorder), Status::ok);
  CHECK_EQ(recorder.visits(), 0u); }

TEST(vidpn_for_each_path, the_first_failed_visit_ends_the_walk)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  fixture.vidpn().add_path(0u, 1u);
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  Recorder recorder(Status::not_supported);
  CHECK_STATUS(vidpn.for_each_path(recorder), Status::not_supported);
  CHECK_EQ(recorder.visits(), 1u); }

TEST(vidpn_for_each_path, refused_acquisitions_and_releases_fail_without_a_leak)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  fixture.vidpn().add_path(0u, 1u);
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  Recorder next_refused(Status::ok);
  fixture.vidpn().refuse(VidPnCall::acquire_next_path, no_memory);
  CHECK_STATUS(vidpn.for_each_path(next_refused), Status::no_memory);
  CHECK_EQ(next_refused.visits(), 1u);
  Recorder first_refused(Status::ok);
  fixture.vidpn().refuse(VidPnCall::acquire_first_path, no_memory);
  CHECK_STATUS(vidpn.for_each_path(first_refused), Status::no_memory);
  CHECK_EQ(first_refused.visits(), 0u);
  VidPnFixture second;
  second.vidpn().add_path(0u, 0u);
  second.vidpn().add_path(0u, 1u);
  VidPn other(second.platform(), second.dxgkrnl(), second.handle());
  CHECK_STATUS(other.initialize(), Status::ok);
  second.vidpn().refuse(VidPnCall::release_path, no_memory);
  Recorder release_refused(Status::ok);
  CHECK_STATUS(other.for_each_path(release_refused), Status::no_memory);
  CHECK_EQ(release_refused.visits(), 1u); }

TEST(vidpn_update_path_support, forwards_the_path)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  D3dkmdtVidPnPresentPathRecord path = fixture.vidpn().path(0u);
  path.ContentTransformationScalingSupport = 1u;
  CHECK_STATUS(vidpn.update_path_support(path), Status::ok);
  CHECK_EQ(fixture.vidpn().path_support_updates(), 1u);
  fixture.vidpn().refuse(VidPnCall::update_path_support, no_memory);
  CHECK_STATUS(vidpn.update_path_support(path), Status::no_memory); }

TEST(vidpn_acquire_mode_set, answers_the_current_source_and_target_sets)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  void *source_set = nullptr;
  void *target_set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *source_interface = nullptr;
  const DxgkVidPnTargetModeSetInterfaceRecord *target_interface = nullptr;
  CHECK_STATUS(vidpn.acquire_mode_set(0u, source_set, source_interface), Status::ok);
  CHECK_STATUS(vidpn.acquire_mode_set(0u, target_set, target_interface), Status::ok);
  CHECK(source_set != nullptr && source_interface != nullptr);
  CHECK(target_set != nullptr && target_interface != nullptr);
  CHECK_STATUS(vidpn.release_mode_set(source_set, source_interface), Status::ok);
  CHECK_STATUS(vidpn.release_mode_set(target_set, target_interface), Status::ok); }

TEST(vidpn_acquire_mode_set, a_refusal_or_a_missing_set_fails)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  void *set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *source_interface = nullptr;
  const DxgkVidPnTargetModeSetInterfaceRecord *target_interface = nullptr;
  fixture.vidpn().refuse(VidPnCall::acquire_source_mode_set, no_memory);
  fixture.vidpn().refuse(VidPnCall::acquire_target_mode_set, 0);
  CHECK_STATUS(vidpn.acquire_mode_set(0u, set, source_interface), Status::no_memory);
  CHECK_STATUS(vidpn.acquire_mode_set(0u, set, target_interface), Status::malformed);
  CHECK(set == nullptr); }

TEST(vidpn_create_mode_set, answers_empty_sets_to_assign)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  void *source_set = nullptr;
  void *target_set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *source_interface = nullptr;
  const DxgkVidPnTargetModeSetInterfaceRecord *target_interface = nullptr;
  CHECK_STATUS(vidpn.create_mode_set(0u, source_set, source_interface), Status::ok);
  CHECK_STATUS(vidpn.create_mode_set(0u, target_set, target_interface), Status::ok);
  CHECK_STATUS(vidpn.assign_mode_set(0u, source_set, source_interface), Status::ok);
  CHECK_STATUS(vidpn.assign_mode_set(0u, target_set, target_interface), Status::ok);
  fixture.vidpn().refuse(VidPnCall::create_source_mode_set, 0);
  fixture.vidpn().refuse(VidPnCall::create_target_mode_set, no_memory);
  CHECK_STATUS(vidpn.create_mode_set(0u, source_set, source_interface), Status::malformed);
  CHECK_STATUS(vidpn.create_mode_set(0u, target_set, target_interface), Status::no_memory); }

TEST(vidpn_assign_mode_set, a_refusal_leaves_the_set_to_release)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), fixture.handle());
  CHECK_STATUS(vidpn.initialize(), Status::ok);
  void *source_set = nullptr;
  void *target_set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *source_interface = nullptr;
  const DxgkVidPnTargetModeSetInterfaceRecord *target_interface = nullptr;
  CHECK_STATUS(vidpn.create_mode_set(0u, source_set, source_interface), Status::ok);
  CHECK_STATUS(vidpn.create_mode_set(0u, target_set, target_interface), Status::ok);
  fixture.vidpn().refuse(VidPnCall::assign_source_mode_set, no_memory);
  fixture.vidpn().refuse(VidPnCall::assign_target_mode_set, no_memory);
  CHECK_STATUS(vidpn.assign_mode_set(0u, source_set, source_interface), Status::no_memory);
  CHECK_STATUS(vidpn.assign_mode_set(0u, target_set, target_interface), Status::no_memory);
  CHECK_STATUS(vidpn.release_mode_set(source_set, source_interface), Status::ok);
  CHECK_STATUS(vidpn.release_mode_set(target_set, target_interface), Status::ok); }

TEST(vidpn, is_passive_only)
{ VidPnFixture fixture;
  VidPn vidpn(fixture.platform(), fixture.dxgkrnl(), nullptr);
  std::uint64_t paths = 0u;
  Recorder recorder(Status::ok);
  void *set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *source_interface = nullptr;
  const DxgkVidPnTargetModeSetInterfaceRecord *target_interface = nullptr;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(vidpn.initialize());
  static_cast<void>(vidpn.source_paths(0u, paths));
  static_cast<void>(vidpn.for_each_path(recorder));
  static_cast<void>(vidpn.update_path_support(D3dkmdtVidPnPresentPathRecord{}));
  static_cast<void>(vidpn.acquire_mode_set(0u, set, source_interface));
  static_cast<void>(vidpn.acquire_mode_set(0u, set, target_interface));
  static_cast<void>(vidpn.create_mode_set(0u, set, source_interface));
  static_cast<void>(vidpn.create_mode_set(0u, set, target_interface));
  static_cast<void>(vidpn.release_mode_set(set, source_interface));
  static_cast<void>(vidpn.release_mode_set(set, target_interface));
  static_cast<void>(vidpn.assign_mode_set(0u, set, source_interface));
  static_cast<void>(vidpn.assign_mode_set(0u, set, target_interface));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 12; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

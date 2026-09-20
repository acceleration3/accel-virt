/* What DXGI and DXCore read off the adapter (contracts/umd_interface.md,
 * "Adapter identity"). Every figure comes from the same two D3DKMT queries the
 * Vulkan ICD makes on the same adapter, so the two cannot disagree; a figure
 * the kernel refuses is reported as zero, logged once, and does not fail the
 * open, because an adapter with an unknown budget is still an adapter. */
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/d3d/adapter_identity.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::MockKmt;
using av::PciIdentity;
using av::RecordingLog;
using av::Status;
using av::d3d::AdapterIdentity;

namespace {

constexpr std::uint64_t some_luid = 0x0000000200001234ull;
constexpr std::uint64_t local_budget = 512ull * 1024ull * 1024ull;
constexpr std::uint64_t non_local_budget = 3ull * 1024ull * 1024ull * 1024ull;

const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);

class Fixture {
public:
    Fixture()
    {
        kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
        kmt_.set_adapter_luid(0u, some_luid);
        kmt_.set_video_memory(av::MemorySegmentGroup::local, local_budget, 0u);
        kmt_.set_video_memory(av::MemorySegmentGroup::non_local, non_local_budget, 0u);
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;

    MockKmt      &kmt() { return kmt_; }
    RecordingLog &log() { return log_; }
    /* The adapter handle MockKmt hands out for the one adapter it has. */
    std::uint32_t adapter()
    {
        std::uint32_t handles[4] = {};
        std::uint32_t found = 0u;
        if (kmt_.enumerate_adapters(handles, 4u, found) != Status::ok) return 0u;
        return found == 0u ? 0u : handles[0];
    }

private:
    MockKmt      kmt_;
    RecordingLog log_;
};

}  // namespace

TEST(adapter_identity, learns_the_luid_the_ids_and_both_budgets_from_the_kernel)
{ Fixture fixture;
  AdapterIdentity identity;
  CHECK(identity.invariants_hold());
  CHECK_STATUS(identity.learn(fixture.kmt(), fixture.adapter(), fixture.log()), Status::ok);
  CHECK_EQ(identity.luid(), some_luid);
  CHECK_EQ(identity.vendor_id(), AdapterScenario::virtio_vendor);
  CHECK_EQ(identity.device_id(), AdapterScenario::virtio_gpu_device);
  CHECK_EQ(identity.dedicated_video_memory(), local_budget);
  CHECK_EQ(identity.shared_system_memory(), non_local_budget);
  CHECK_EQ(fixture.log().lines(), 0u);
  CHECK(identity.invariants_hold()); }

/* The description is what a person sees in dxdiag: it must name this adapter,
 * not be a placeholder, and it must be NUL-terminated inside its own array. */
TEST(adapter_identity, describes_itself_by_name)
{ Fixture fixture;
  AdapterIdentity identity;
  CHECK_STATUS(identity.learn(fixture.kmt(), fixture.adapter(), fixture.log()), Status::ok);
  CHECK(std::strstr(identity.description(), "rvkgpu") != nullptr);
  CHECK(std::strlen(identity.description()) < AdapterIdentity::description_bytes); }

/* The LUID is the adapter's whole identity to DXGI and to the Vulkan loader, so
 * a kernel that will not answer it is a kernel this adapter cannot be reported
 * from at all - unlike a budget, which is a number. */
TEST(adapter_identity, a_refused_luid_fails_the_learn_and_is_logged_once)
{ Fixture fixture;
  fixture.kmt().fail_at(MockKmt::Call::adapter_luid, 1u, MockKmt::invalid_parameter);
  AdapterIdentity identity;
  CHECK_STATUS(identity.learn(fixture.kmt(), fixture.adapter(), fixture.log()), Status::not_supported);
  CHECK_EQ(identity.luid(), 0ull);
  CHECK_EQ(fixture.log().holding("adapter luid"), 1u);
  CHECK(identity.invariants_hold()); }

TEST(adapter_identity, a_refused_identity_fails_the_learn_and_is_logged_once)
{ Fixture fixture;
  const std::uint32_t adapter = fixture.adapter();
  fixture.kmt().fail_at(MockKmt::Call::query_identity, 1u, MockKmt::invalid_parameter);
  AdapterIdentity identity;
  CHECK_STATUS(identity.learn(fixture.kmt(), adapter, fixture.log()), Status::not_supported);
  CHECK_EQ(identity.vendor_id(), 0u);
  CHECK_EQ(fixture.log().holding("adapter identity"), 1u);
  CHECK(identity.invariants_hold()); }

/* Each budget separately: a driver that dropped both on one refusal would
 * report nothing where it could have reported half. */
TEST(adapter_identity, a_refused_budget_reports_zero_and_keeps_the_other)
{ for (std::uint32_t refused = 1u; refused <= 2u; refused++) {
      Fixture fixture;
      fixture.kmt().fail_at(MockKmt::Call::query_video_memory, refused, MockKmt::invalid_parameter);
      AdapterIdentity identity;
      CHECK_STATUS(identity.learn(fixture.kmt(), fixture.adapter(), fixture.log()), Status::ok);
      CHECK_EQ(identity.luid(), some_luid);
      CHECK_EQ(identity.dedicated_video_memory(), refused == 1u ? 0ull : local_budget);
      CHECK_EQ(identity.shared_system_memory(), refused == 2u ? 0ull : non_local_budget);
      CHECK_EQ(fixture.log().holding("video memory"), 1u);
      CHECK(identity.invariants_hold());
  } }

TEST(adapter_identity, reports_what_a_state_dump_shows)
{ Fixture fixture;
  AdapterIdentity identity;
  CHECK_STATUS(identity.learn(fixture.kmt(), fixture.adapter(), fixture.log()), Status::ok);
  char bytes[256] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  writer.open("identity", 0u);
  identity.report(writer);
  writer.close();
  CHECK(std::strstr(writer.dump(), "luid=0x0000000200001234") != nullptr);
  CHECK(std::strstr(writer.dump(), "dedicated=536870912") != nullptr);
  CHECK(std::strstr(writer.dump(), "shared=3221225472") != nullptr); }

/* Learning twice is what a second OpenAdapter in the same process does, and it
 * must not append to anything or double a figure. */
TEST(adapter_identity, learning_again_answers_the_same_figures)
{ Fixture fixture;
  AdapterIdentity identity;
  CHECK_STATUS(identity.learn(fixture.kmt(), fixture.adapter(), fixture.log()), Status::ok);
  CHECK_STATUS(identity.learn(fixture.kmt(), fixture.adapter(), fixture.log()), Status::ok);
  CHECK_EQ(identity.luid(), some_luid);
  CHECK_EQ(identity.dedicated_video_memory(), local_budget);
  CHECK(identity.invariants_hold()); }

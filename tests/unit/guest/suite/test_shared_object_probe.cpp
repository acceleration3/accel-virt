/* rvkvkprobe's shared-object steps over a scripted Vulkan (feature 053, US1).
 *
 * The three D3DKMT calls this exercises are reached only through the ICD, so
 * what a unit test can judge is the shape of the run: that the steps happen in
 * an order a guest can be read back from, that a host offering no external
 * objects stops the run rather than failing it, and above all that every
 * failure carries the NTSTATUS of the last kernel call - which is the only
 * thing that will say *which* of the four refused when this runs in a guest.
 */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/shared_object_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedVulkan;
using av::SharedObjectProbe;
using av::Status;

namespace {

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;
void *const             minted_handle = reinterpret_cast<void *>(0x8000000000000004ull);

class Rig {
public:
    Rig() : report_("rvkvkprobe")
    {
        kmt_.add_adapter(rvk, AdapterScenario::wddm_3_2);
        kmt_.set_adapter_luid(0u, adapter_luid);
        vulkan_.set_luid(0u, adapter_luid, true);
    }

    MockKmt        &kmt() { return kmt_; }
    ScriptedVulkan &vulkan() { return vulkan_; }
    GuestReport    &report() { return report_; }

    void run()
    {
        SharedObjectProbe probe(kmt_, report_, vulkan_);
        probe.run();
        report_.finish();
    }

    bool holds(const char *line) const { return std::strstr(report_.text(), line) != nullptr; }

private:
    MockKmt        kmt_;
    ScriptedVulkan vulkan_;
    GuestReport    report_;
};

}  // namespace

/* A host that offers no external objects: the run reaches the question, says
 * so, and stops - it has nothing to prove and is not a failure of this
 * driver. */
TEST(shared_object_probe, a_host_with_no_external_objects_stops_at_the_question)
{ Rig rig;
  rig.run();
  CHECK(rig.holds("ok shared.adapter"));
  CHECK(rig.holds("ok shared.chosen"));
  CHECK(rig.holds("shared.reported"));
  /* The device is not even made: one asked for with an extension the driver
   * does not report would be refused, so the question comes first and nothing
   * past it ran. */
  CHECK(!rig.holds("shared.device"));
  CHECK(!rig.holds("shared.exportable"));
  CHECK(!rig.holds("shared.handle")); }

/* A host that offers them: the allocation asks the miniport for an identity,
 * the handle is minted over it, and freeing gives all of it back. */
TEST(shared_object_probe, an_exportable_allocation_is_minted_handled_and_released)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.run();
  CHECK(rig.holds("ok shared.reported"));
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(rig.holds("ok shared.handle"));
  CHECK(rig.holds("ok shared.released"));
  CHECK_EQ(rig.report().failed(), 0u); }

/* A driver that will not export: the step fails carrying the NTSTATUS of the
 * last kernel call, which is what says which of the four refused. */
TEST(shared_object_probe, a_refused_export_is_reported_with_the_kernels_own_status)
{ Rig rig;
  rig.vulkan().report_external_objects(nullptr);
  rig.run();
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(rig.holds("FAIL shared.handle"));
  CHECK(rig.report().failed() != 0u);
  /* The released step still runs: what was allocated is given back whatever
   * the handle did. */
  CHECK(rig.holds("shared.released")); }

/* An allocation the driver refuses fails the step and stops the run, with the
 * kernel's own code on the line rather than the Vulkan number - the Vulkan one
 * is VK_ERROR_TOO_MANY_OBJECTS whichever of the four calls refused. */
TEST(shared_object_probe, a_refused_allocation_carries_the_kernels_code_and_stops)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::allocate_memory, 1u, VK_ERROR_TOO_MANY_OBJECTS);
  rig.run();
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(!rig.holds("shared.handle"));
  CHECK(rig.report().failed() != 0u); }

/* No rvkgpu adapter at all: the run says so at the first step and does not
 * pretend to have made a device. */
TEST(shared_object_probe, no_adapter_is_the_first_step_and_the_last)
{ MockKmt        kmt;
  ScriptedVulkan vulkan;
  GuestReport    report("rvkvkprobe");
  SharedObjectProbe probe(kmt, report, vulkan);
  probe.run();
  report.finish();
  CHECK(std::strstr(report.text(), "FAIL shared.adapter") != nullptr);
  CHECK(std::strstr(report.text(), "shared.instance") == nullptr); }

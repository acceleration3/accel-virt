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
  /* The kernel's own question is asked whatever the host GPU offers. */
  CHECK(rig.holds("shared.carrier_plain"));
  CHECK(rig.holds("shared.carrier_long"));
  CHECK(rig.holds("shared.carrier_shared"));
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

/* The diagnostic itself, which failed on its first real outing: the kernel
 * call is made inside the ICD by the ICD's own Kmt, so the probe's Kmt has
 * never heard of it and `last_code()` there is 0. The step must report the
 * *driver's* status instead, or a reader sees code=0 and concludes no kernel
 * call was involved - the opposite of the truth (feature 053). */
TEST(shared_object_probe, a_refusal_carries_the_drivers_ntstatus_and_not_the_probes)
{ Rig rig;
  constexpr std::uint32_t invalid_parameter = 0xC000000Du;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::allocate_memory, 1u, VK_ERROR_TOO_MANY_OBJECTS);
  rig.vulkan().set_kernel_status(invalid_parameter);
  rig.run();
  /* The probe's own Kmt made no call at all, so anything reading it would say
   * zero. What is on the line is the driver's. */
  CHECK_EQ(rig.kmt().last_code(), 0u);
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(rig.holds("code=0xC000000D")); }

/* And a step that passes still carries the driver's zero rather than a stale
 * number from an earlier refusal. */
TEST(shared_object_probe, a_step_that_passes_carries_no_status_at_all)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.run();
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(!rig.holds("code=0xC000000D"));
  CHECK_EQ(rig.report().failed(), 0u); }

/* The sweep is a measurement and never a verdict: most of its lengths are ones
 * no caller uses and the driver rightly refuses, so a sweep that failed the run
 * for them would fail every run for ever. What it must do is put the number on
 * the line - which is what tells dxgkrnl's refusal from the driver's. */
TEST(shared_object_probe, the_size_sweep_measures_every_length_and_fails_none_of_them)
{ Rig rig;
  rig.run();
  const char *const lengths[] = {"shared.carrier_size_48", "shared.carrier_size_56",
                                 "shared.carrier_size_64", "shared.carrier_size_72",
                                 "shared.carrier_size_80", "shared.carrier_size_88",
                                 "shared.carrier_size_96", "shared.carrier_size_128"};
  for (const char *const step : lengths)
      if (!rig.holds(step)) av::TestRegistry::instance().record_failure(__FILE__, __LINE__, step);
  /* The two this driver has shapes for are taken and measure nothing; the mock
   * answers the rest not_supported, as the miniport does. */
  CHECK(rig.holds("ok shared.carrier_size_48 0"));
  CHECK(rig.holds("ok shared.carrier_size_88 0"));
  CHECK(rig.holds("ok shared.carrier_size_64 3221225659"));
  /* And not one of them failed the run. */
  CHECK(!rig.holds("FAIL shared.carrier_size"));
  /* The one length that carries a real record rather than filler: it is a
   * length no shape is known for, so what it must show is the same refusal the
   * filler got there - the bytes are not what is being refused. */
  CHECK(rig.holds("ok shared.carrier_size_96 3221225659"));
  CHECK(rig.holds("ok shared.carrier_size_96_valid 3221225659")); }

/* The host's answer to "do you have external memory at all" is read, not
 * assumed, and every way it can go wrong stops the run at that question
 * rather than somewhere later that would be blamed on this driver. */
TEST(shared_object_probe, a_host_that_cannot_be_asked_for_its_extensions_stops_at_the_question)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_ERROR_OUT_OF_HOST_MEMORY, VK_SUCCESS);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

/* More extensions than the probe keeps room for: it will not read past its own
 * array to find out, and says the host offers nothing it can use. */
TEST(shared_object_probe, a_host_listing_more_extensions_than_there_is_room_for_is_not_read)
{ Rig rig;
  rig.vulkan().answer_extensions(SharedObjectProbe::max_extensions + 1u, VK_SUCCESS, VK_SUCCESS);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

/* A host that counts its extensions and then refuses to name them. */
TEST(shared_object_probe, a_host_that_will_not_name_its_extensions_stops_at_the_question)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_SUCCESS, VK_ERROR_OUT_OF_HOST_MEMORY);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

/* And a host with a list that simply does not hold the one name: every entry is
 * read to the end and none of them is it. */
TEST(shared_object_probe, a_host_listing_other_extensions_offers_no_external_memory)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_SUCCESS, VK_SUCCESS);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

/* A device with no memory type an export could use: the step says not_supported
 * rather than allocating from a type the host never offered. */
TEST(shared_object_probe, no_usable_memory_type_is_a_refused_export_and_not_a_guess)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().set_memory_types(0u);
  rig.run();
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(!rig.holds("shared.handle"));
  CHECK(rig.report().failed() != 0u); }

/* A host that answers success and no memory. The step passed on the VkResult
 * alone would report an allocation that does not exist, so what it judges is
 * the memory. */
TEST(shared_object_probe, an_allocate_that_answers_success_and_nothing_is_still_a_failure)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().allocate_nothing();
  rig.run();
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(!rig.holds("shared.handle"));
  CHECK(rig.report().failed() != 0u); }

/* The same of the export: success and no handle is no handle. */
TEST(shared_object_probe, an_export_that_answers_success_and_no_handle_is_a_failed_step)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().hand_out_no_handle();
  rig.run();
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(rig.holds("FAIL shared.handle"));
  CHECK(rig.report().failed() != 0u); }

/* Freeing gives the identity, the handle and the carrier back, and the step is
 * the kernel's own verdict on all three: a driver that refused any of them
 * leaves an NTSTATUS behind and the release has not happened. */
TEST(shared_object_probe, a_release_the_kernel_refused_is_reported_with_its_status)
{ Rig rig;
  constexpr std::uint32_t invalid_parameter = 0xC000000Du;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().refuse_the_release(invalid_parameter);
  rig.run();
  CHECK(rig.holds("FAIL shared.released"));
  CHECK(rig.holds("code=0xC000000D"));
  CHECK(rig.report().failed() != 0u); }

/* The carrier steps ask the kernel again rather than reusing what the first
 * step found, so each of their own refusals is a step of its own: a reader
 * must be able to tell "no adapter" from "no device on it" from "no
 * allocation on that". */
TEST(shared_object_probe, the_carrier_steps_name_their_own_adapter_and_device_refusals)
{ Rig rig;
  rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, 2u, 0xC000009Au);
  rig.run();
  CHECK(rig.holds("ok shared.adapter"));
  CHECK(rig.holds("FAIL shared.carrier_adapter"));
  CHECK(!rig.holds("shared.carrier_device"));
  CHECK(!rig.holds("shared.carrier_size_48")); }

TEST(shared_object_probe, a_device_the_kernel_will_not_make_stops_the_carrier_steps)
{ Rig rig;
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, 0xC000009Au);
  rig.run();
  CHECK(rig.holds("FAIL shared.carrier_device"));
  CHECK(!rig.holds("shared.carrier_size_48"));
  CHECK(!rig.holds("shared.carrier_plain")); }

/* The three carrier creates are separate steps because they separate three
 * things: the ordinary create, the same length through a buffer the caller
 * does not own, and the two-way one. Each must be able to fail alone. */
TEST(shared_object_probe, each_of_the_three_carrier_creates_fails_on_its_own)
{ constexpr std::uint32_t invalid_parameter = 0xC000000Du;
  { Rig rig;
    /* Nine swept lengths come first, so the ordinary create is the tenth. */
    rig.kmt().fail_at(MockKmt::Call::create_allocation, 10u, invalid_parameter);
    rig.run();
    CHECK(rig.holds("FAIL shared.carrier_plain"));
    CHECK(rig.holds("shared.carrier_long")); }
  { Rig rig;
    rig.kmt().fail_at(MockKmt::Call::create_allocation, 11u, invalid_parameter);
    rig.run();
    CHECK(rig.holds("ok shared.carrier_plain"));
    CHECK(rig.holds("FAIL shared.carrier_long"));
    CHECK(rig.holds("shared.carrier_shared")); }
  { Rig rig;
    rig.kmt().fail_at(MockKmt::Call::create_shared_allocation, 1u, invalid_parameter);
    rig.run();
    CHECK(rig.holds("ok shared.carrier_long"));
    CHECK(rig.holds("FAIL shared.carrier_shared"));
    CHECK(!rig.holds("shared.carrier_minted")); } }

/* A two-way create that answers success and mints nothing: the record comes
 * back as it went in, and the step that reads it must say so rather than
 * trusting the status. */
TEST(shared_object_probe, a_shared_create_that_mints_nothing_is_caught_by_reading_the_record)
{ Rig rig;
  rig.kmt().zero_next(MockKmt::Call::create_shared_allocation);
  rig.run();
  CHECK(rig.holds("ok shared.carrier_shared"));
  CHECK(rig.holds("FAIL shared.carrier_minted"));
  CHECK(rig.report().failed() != 0u); }

/* Every step between the adapter and the export stops the run where it failed,
 * so a report never shows a device made on a physical device nothing chose. */
TEST(shared_object_probe, each_step_up_to_the_device_stops_the_run_where_it_failed)
{ { Rig rig;
    rig.vulkan().fail(ScriptedVulkan::Call::create_instance, 1u, VK_ERROR_INITIALIZATION_FAILED);
    rig.run();
    CHECK(rig.holds("FAIL shared.instance"));
    CHECK(!rig.holds("shared.physical_device")); }
  { Rig rig;
    rig.vulkan().fail(ScriptedVulkan::Call::enumerate_physical_devices, 1u, VK_ERROR_INITIALIZATION_FAILED);
    rig.run();
    CHECK(rig.holds("FAIL shared.physical_device"));
    CHECK(!rig.holds("shared.chosen")); }
  { /* A host whose devices carry other LUIDs than this adapter's: none of them
     * is the one an export would prove anything about. */
    Rig rig;
    rig.vulkan().set_luid(0u, adapter_luid ^ 0xFFull, true);
    rig.run();
    CHECK(rig.holds("FAIL shared.chosen"));
    CHECK(!rig.holds("shared.family")); }
  { Rig rig;
    rig.vulkan().set_queue_families(0u, 0u);
    rig.run();
    CHECK(rig.holds("FAIL shared.family"));
    CHECK(!rig.holds("shared.reported")); }
  { Rig rig;
    rig.vulkan().report_external_objects(minted_handle);
    rig.vulkan().fail(ScriptedVulkan::Call::create_device, 1u, VK_ERROR_INITIALIZATION_FAILED);
    rig.run();
    CHECK(rig.holds("ok shared.reported"));
    CHECK(rig.holds("FAIL shared.device"));
    CHECK(!rig.holds("shared.exportable")); } }

/* A host that answers VK_INCOMPLETE because the probe's array was the smaller
 * of the two: the names it did get are still read, which is all the question
 * needs. */
TEST(shared_object_probe, a_partial_extension_list_is_read_as_far_as_it_goes)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_SUCCESS, VK_INCOMPLETE);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

/* The two-way create's whole point is the record that comes back, so bytes no
 * record decodes from are caught by reading them - a driver writing nonsense
 * back must not read as a minted identity. */
TEST(shared_object_probe, a_record_that_comes_back_as_nonsense_is_not_a_minted_identity)
{ Rig rig;
  rig.kmt().scribble_the_next_record();
  rig.run();
  CHECK(rig.holds("ok shared.carrier_shared"));
  CHECK(rig.holds("FAIL shared.carrier_minted"));
  CHECK(rig.report().failed() != 0u); }

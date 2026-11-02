#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/probe/shared_object_probe.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_cl.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedVulkan;
using av::SharedObjectProbe;
using av::Status;

namespace {

const PciIdentity av(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;
constexpr std::uint32_t filled_word = 0xABCD0000u;
void *const             minted_handle = reinterpret_cast<void *>(0x8000000000000004ull);

class Rig {
public:
    Rig() : report_(report_memory_, "avvkprobe")
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        kmt_.set_adapter_luid(0u, adapter_luid);
        vulkan_.set_luid(0u, adapter_luid, true);
        cl_.set_device_extensions("cl_khr_icd cl_khr_external_memory_win32 cl_khr_fp16");
    }

    MockKmt        &kmt() { return kmt_; }
    ScriptedVulkan &vulkan() { return vulkan_; }
    av::ScriptedCl &cl() { return cl_; }
    GuestReport    &report() { return report_; }
    void            without_opencl() { with_cl_ = false; }
    void            cl_reads_word(std::uint32_t word)
    {
        std::uint32_t words[av::ScriptedCl::max_shared_words] = {};
        for (std::uint32_t index = 0u; index < av::ScriptedCl::max_shared_words; index++)
            words[index] = word;
        cl_.memory_behind(minted_handle, words, av::ScriptedCl::max_shared_words);
    }
    void            cl_reads(const std::uint32_t *words, std::uint32_t count)
    { cl_.memory_behind(minted_handle, words, count); }

    void run()
    {
        SharedObjectProbe probe(kmt_, report_, vulkan_, with_cl_ ? &cl_ : nullptr);
        probe.run();
        report_.finish();
    }

    bool holds(const char *line) const { return std::strstr(report_.text(), line) != nullptr; }

private:
    bool           with_cl_ = true;
    av::ScriptedCl cl_;
    MockKmt        kmt_;
    ScriptedVulkan vulkan_;
    av::MockPlatform report_memory_;
    GuestReport    report_{report_memory_};
};

}

TEST(shared_object_probe, a_host_with_no_external_objects_stops_at_the_question)
{ Rig rig;
  rig.run();
  CHECK(rig.holds("ok shared.adapter"));
  CHECK(rig.holds("shared.carrier_plain"));
  CHECK(rig.holds("shared.carrier_long"));
  CHECK(rig.holds("shared.carrier_shared"));
  CHECK(rig.holds("ok shared.chosen"));
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device"));
  CHECK(!rig.holds("shared.exportable"));
  CHECK(!rig.holds("shared.handle")); }

TEST(shared_object_probe, an_exportable_allocation_is_minted_handled_and_released)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.run();
  CHECK(rig.holds("ok shared.reported"));
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(rig.holds("ok shared.filled"));
  CHECK(rig.holds("ok shared.handle"));
  CHECK(rig.holds("ok shared.released"));
  CHECK_EQ(rig.report().failed(), 0u); }

TEST(shared_object_probe, opencl_opens_the_handle_vulkan_exported_and_reads_what_it_wrote)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.run();
  CHECK(rig.holds("ok shared.cl_platform_found"));
  CHECK(rig.holds("ok shared.cl_device_found"));
  CHECK(rig.holds("ok shared.cl_context"));
  CHECK(rig.holds("ok shared.cl_queue"));
  CHECK(rig.holds("ok shared.cl_opened"));
  CHECK(rig.holds("ok shared.cl_read"));
  CHECK_EQ(rig.report().failed(), 0u);
  CHECK_EQ(rig.cl().live_objects(), 0u);
  CHECK_EQ(rig.cl().misuses(), 0u); }

TEST(shared_object_probe, a_driver_reporting_no_external_memory_stops_the_opencl_half_and_does_not_fail)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().set_device_extensions("cl_khr_icd cl_khr_fp16");
  rig.run();
  CHECK(rig.holds("note shared.cl_external_memory"));
  CHECK(!rig.holds("shared.cl_context"));
  CHECK_EQ(rig.report().failed(), 0u); }

TEST(shared_object_probe, an_extension_whose_name_only_starts_the_right_way_is_not_the_right_one)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().set_device_extensions("cl_khr_external_memory_win32_and_more cl_khr_external_memory");
  rig.run();
  CHECK(rig.holds("note shared.cl_external_memory"));
  CHECK(!rig.holds("shared.cl_context")); }

TEST(shared_object_probe, a_driver_that_will_not_list_its_extensions_fails_the_step_it_asked_at)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().fail_at(av::ScriptedCl::Call::device_text, 1u, av::ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_extensions"));
  CHECK(!rig.holds("shared.cl_external_memory"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, no_opencl_beside_the_process_runs_the_vulkan_half_alone)
{ Rig rig;
  rig.without_opencl();
  rig.vulkan().report_external_objects(minted_handle);
  rig.run();
  CHECK(rig.holds("ok shared.handle"));
  CHECK(!rig.holds("shared.cl_"));
  CHECK_EQ(rig.report().failed(), 0u); }

TEST(shared_object_probe, a_fill_that_could_not_happen_stops_the_opencl_half)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::bind_buffer_memory, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  rig.cl_reads_word(filled_word);
  rig.run();
  CHECK(rig.holds("FAIL shared.filled"));
  CHECK(rig.holds("ok shared.handle"));
  CHECK(!rig.holds("shared.cl_")); }

TEST(shared_object_probe, a_command_buffer_that_cannot_be_recorded_is_a_failed_fill)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::create_command_pool, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
  rig.cl_reads_word(filled_word);
  rig.run();
  CHECK(rig.holds("FAIL shared.filled"));
  CHECK(!rig.holds("shared.cl_")); }

TEST(shared_object_probe, a_fill_the_queue_never_finishes_is_a_failed_fill)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::wait_for_fence, 1u, VK_TIMEOUT);
  rig.cl_reads_word(filled_word);
  rig.run();
  CHECK(rig.holds("FAIL shared.filled"));
  CHECK(!rig.holds("shared.cl_")); }

TEST(shared_object_probe, a_buffer_the_host_will_not_make_stops_before_the_allocation)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::create_buffer, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
  rig.run();
  CHECK(rig.holds("FAIL shared.target"));
  CHECK(!rig.holds("shared.exportable")); }

TEST(shared_object_probe, no_memory_type_the_buffer_takes_is_a_refused_export)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().set_allowed_types(0u, 0u);
  rig.run();
  CHECK(rig.holds("ok shared.target"));
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(!rig.holds("shared.handle")); }

TEST(shared_object_probe, a_loader_listing_no_platforms_stops_the_opencl_half_at_the_first_step)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().list_no_platforms();
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_platform_found"));
  CHECK(!rig.holds("shared.cl_device"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, a_loader_that_refuses_to_list_platforms_fails_the_step_it_asked_at)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().fail_at(av::ScriptedCl::Call::platforms, 1u, av::ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_platform"));
  CHECK(!rig.holds("shared.cl_platform_found")); }

TEST(shared_object_probe, a_platform_with_no_device_stops_before_a_context_is_made)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().set_device_count(0u);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_device_found"));
  CHECK(!rig.holds("shared.cl_context")); }

TEST(shared_object_probe, a_platform_that_refuses_to_list_devices_fails_the_step_it_asked_at)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().fail_at(av::ScriptedCl::Call::devices, 1u, av::ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_device"));
  CHECK(!rig.holds("shared.cl_device_found")); }

TEST(shared_object_probe, a_context_the_device_refuses_stops_the_opencl_half)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().fail_at(av::ScriptedCl::Call::create_context, 1u, av::ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_context"));
  CHECK(!rig.holds("shared.cl_queue")); }

TEST(shared_object_probe, a_queue_the_device_refuses_stops_the_opencl_half)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().fail_at(av::ScriptedCl::Call::create_queue, 1u, av::ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_queue"));
  CHECK(!rig.holds("shared.cl_opened"));
  CHECK_EQ(rig.cl().live_objects(), 0u); }

TEST(shared_object_probe, a_handle_opencl_cannot_open_is_a_failed_open_and_no_read)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_opened"));
  CHECK(!rig.holds("shared.cl_read"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, memory_that_reads_back_other_bytes_is_not_the_memory_vulkan_wrote)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(0u);
  rig.run();
  CHECK(rig.holds("ok shared.cl_opened"));
  CHECK(rig.holds("FAIL shared.cl_read"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, memory_holding_only_the_first_word_is_still_not_the_right_memory)
{ Rig rig;
  const std::uint32_t words[] = {filled_word, 0u, 0u, 0u};
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads(words, 4u);
  rig.run();
  CHECK(rig.holds("FAIL shared.cl_read")); }

TEST(shared_object_probe, a_read_the_device_refuses_carries_the_devices_error)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.cl().fail_at(av::ScriptedCl::Call::read_buffer, 1u, av::ScriptedCl::refused);
  rig.run();
  CHECK(rig.holds("ok shared.cl_opened"));
  CHECK(rig.holds("FAIL shared.cl_read")); }

TEST(shared_object_probe, a_refused_export_is_reported_with_the_kernels_own_status)
{ Rig rig;
  rig.vulkan().report_external_objects(nullptr);
  rig.run();
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(rig.holds("FAIL shared.handle"));
  CHECK(rig.report().failed() != 0u);
  CHECK(rig.holds("shared.released")); }

TEST(shared_object_probe, a_refused_allocation_carries_the_kernels_code_and_stops)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::allocate_memory, 1u, VK_ERROR_TOO_MANY_OBJECTS);
  rig.run();
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(!rig.holds("shared.handle"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, no_adapter_is_the_first_step_and_the_last)
{ MockKmt        kmt;
  ScriptedVulkan vulkan;
  av::MockPlatform report_memory;
  GuestReport    report(report_memory, "avvkprobe");
  SharedObjectProbe probe(kmt, report, vulkan, nullptr);
  probe.run();
  report.finish();
  CHECK(std::strstr(report.text(), "FAIL shared.adapter") != nullptr);
  CHECK(std::strstr(report.text(), "shared.instance") == nullptr); }

TEST(shared_object_probe, a_refusal_carries_the_drivers_ntstatus_and_not_the_probes)
{ Rig rig;
  constexpr std::uint32_t invalid_parameter = 0xC000000Du;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().fail(ScriptedVulkan::Call::allocate_memory, 1u, VK_ERROR_TOO_MANY_OBJECTS);
  rig.vulkan().set_kernel_status(invalid_parameter);
  rig.run();
  CHECK_EQ(rig.kmt().last_code(), 0u);
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(rig.holds("code=0xC000000D")); }

TEST(shared_object_probe, a_step_that_passes_carries_no_status_at_all)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.cl_reads_word(filled_word);
  rig.run();
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(!rig.holds("code=0xC000000D"));
  CHECK_EQ(rig.report().failed(), 0u); }

TEST(shared_object_probe, the_size_sweep_measures_every_length_and_fails_none_of_them)
{ Rig rig;
  rig.run();
  const char *const lengths[] = {"shared.carrier_size_48", "shared.carrier_size_56",
                                 "shared.carrier_size_64", "shared.carrier_size_72",
                                 "shared.carrier_size_80", "shared.carrier_size_88",
                                 "shared.carrier_size_96", "shared.carrier_size_128"};
  for (const char *const step : lengths)
      if (!rig.holds(step)) av::TestRegistry::instance().record_failure(__FILE__, __LINE__, step);
  CHECK(rig.holds("ok shared.carrier_size_56 0"));
  CHECK(rig.holds("ok shared.carrier_size_96 0"));
  CHECK(rig.holds("ok shared.carrier_size_48 3221225659"));
  CHECK(rig.holds("ok shared.carrier_size_88 3221225659"));
  CHECK(rig.holds("ok shared.carrier_size_64 3221225659"));
  CHECK(rig.holds("ok shared.carrier_size_128 3221225659"));
  CHECK(!rig.holds("FAIL shared.carrier_size"));
  CHECK(rig.holds("ok shared.carrier_size_96_valid 0")); }

TEST(shared_object_probe, a_host_that_cannot_be_asked_for_its_extensions_stops_at_the_question)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_ERROR_OUT_OF_HOST_MEMORY, VK_SUCCESS);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

TEST(shared_object_probe, a_host_listing_more_extensions_than_there_is_room_for_is_not_read)
{ Rig rig;
  rig.vulkan().answer_extensions(SharedObjectProbe::max_extensions + 1u, VK_SUCCESS, VK_SUCCESS);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

TEST(shared_object_probe, a_host_that_will_not_name_its_extensions_stops_at_the_question)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_SUCCESS, VK_ERROR_OUT_OF_HOST_MEMORY);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

TEST(shared_object_probe, a_host_listing_other_extensions_offers_no_external_memory)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_SUCCESS, VK_SUCCESS);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

TEST(shared_object_probe, no_usable_memory_type_is_a_refused_export_and_not_a_guess)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().set_memory_types(0u);
  rig.run();
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(!rig.holds("shared.handle"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, an_allocate_that_answers_success_and_nothing_is_still_a_failure)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().allocate_nothing();
  rig.run();
  CHECK(rig.holds("FAIL shared.exportable"));
  CHECK(!rig.holds("shared.handle"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, an_export_that_answers_success_and_no_handle_is_a_failed_step)
{ Rig rig;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().hand_out_no_handle();
  rig.run();
  CHECK(rig.holds("ok shared.exportable"));
  CHECK(rig.holds("FAIL shared.handle"));
  CHECK(rig.report().failed() != 0u); }

TEST(shared_object_probe, a_release_the_kernel_refused_is_reported_with_its_status)
{ Rig rig;
  constexpr std::uint32_t invalid_parameter = 0xC000000Du;
  rig.vulkan().report_external_objects(minted_handle);
  rig.vulkan().refuse_the_release(invalid_parameter);
  rig.run();
  CHECK(rig.holds("FAIL shared.released"));
  CHECK(rig.holds("code=0xC000000D"));
  CHECK(rig.report().failed() != 0u); }

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

TEST(shared_object_probe, each_of_the_three_carrier_creates_fails_on_its_own)
{ constexpr std::uint32_t invalid_parameter = 0xC000000Du;
  { Rig rig;
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

TEST(shared_object_probe, a_shared_create_that_mints_nothing_is_caught_by_reading_the_record)
{ Rig rig;
  rig.kmt().zero_next(MockKmt::Call::create_shared_allocation);
  rig.run();
  CHECK(rig.holds("ok shared.carrier_shared"));
  CHECK(rig.holds("FAIL shared.carrier_minted"));
  CHECK(rig.report().failed() != 0u); }

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
  {
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

TEST(shared_object_probe, a_partial_extension_list_is_read_as_far_as_it_goes)
{ Rig rig;
  rig.vulkan().answer_extensions(2u, VK_SUCCESS, VK_INCOMPLETE);
  rig.run();
  CHECK(rig.holds("shared.reported"));
  CHECK(!rig.holds("shared.device")); }

TEST(shared_object_probe, a_record_that_comes_back_as_nonsense_is_not_a_minted_identity)
{ Rig rig;
  rig.kmt().scribble_the_next_record();
  rig.run();
  CHECK(rig.holds("ok shared.carrier_shared"));
  CHECK(rig.holds("FAIL shared.carrier_minted"));
  CHECK(rig.report().failed() != 0u); }

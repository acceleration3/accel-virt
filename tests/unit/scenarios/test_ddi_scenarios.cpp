#include "miniport/ddi/d3dkmdt_shared_primary_surface_data_record.hpp"
#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_flush_tlb_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "miniport/ddi/dxgk_driver_caps_record.hpp"
#include "miniport/ddi/dxgk_gpu_clock_data_record.hpp"
#include "miniport/ddi/dxgk_node_metadata_record.hpp"
#include "miniport/ddi/dxgk_open_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_pte_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "miniport/ddi/dxgkarg_close_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "miniport/ddi/dxgkarg_control_interrupt2_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_create_device_record.hpp"
#include "miniport/ddi/dxgkarg_describe_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_escape_record.hpp"
#include "miniport/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "miniport/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "miniport/ddi/dxgkarg_open_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
#include "miniport/ddi/dxgkarg_reset_engine_record.hpp"
#include "miniport/ddi/dxgkarg_reset_hw_engine_record.hpp"
#include "miniport/ddi/dxgkarg_resume_hw_engine_record.hpp"
#include "miniport/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "miniport/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "miniport/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "miniport/ddi/dxgkargcb_query_feature_support_record.hpp"
#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "miniport/driver/driver_image.hpp"
#include "core/support/status.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "miniport/display/display_info_response.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/virtio_gpu/page_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/shared_release_request.hpp"
#include "core/platform/log_budget.hpp"
#include "miniport/display/display_mode.hpp"
#include "harness/virtio/fake_display_clock.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/dxgkrnl/capability_rule.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_fdo.hpp"
#include "harness/dxgkrnl/fake_dxgmonitor.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/trace/mock_trace_host.hpp"
#include "harness/vidmm/fake_gpu_mmu.hpp"
#include "harness/vidmm/fake_vidmm.hpp"
#include "harness/vidmm/gpu_mmu_rule.hpp"
#include "harness/vidpn/fake_display_mode_manager.hpp"
#include "harness/vidpn/fake_vidpn.hpp"
#include "harness/vidsch/fake_hw_queue.hpp"
#include "harness/vidsch/fake_vidsch.hpp"
#include "proto/context/av_device_count.hpp"
#include "proto/dma/present_command.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>

using av::CapabilityRule;
using av::DdiRoutine;
using av::DdiSlot;
using av::DdiTable;
using av::DriverImage;
using av::FakeDxgkrnl;
using av::FakeDxgkrnlAdapter;
using av::FakeDxgkrnlFdo;
using av::FakeVirtioGpuFunction;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::uint32_t no_memory = 0xC0000017u;
constexpr std::uint32_t not_supported = 0xC00000BBu;
constexpr std::uint32_t invalid_device_state = 0xC0000184u;
constexpr std::uint32_t resource_flag = 1u;
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;
constexpr std::uint8_t  driver_ok_status = 0x0Fu;

int driver_object = 0;
int registry_path = 0;

std::uint32_t nt(std::int32_t status) { return static_cast<std::uint32_t>(status); }

class LoadedDriver {
public:
    LoadedDriver()
        : entered_(image_.enter(&driver_object, &registry_path, av::ImageRange(0u, 0u), nullptr)),
          loaded_allocations_(image_.platform().live_allocations()) {}
    ~LoadedDriver()
    {
        reinterpret_cast<void (*)()>(table().routine(DdiSlot::DxgkDdiUnload))();
        image_.leave();
    }

    int32_t         entered() const { return entered_; }
    std::uint32_t   loaded_allocations() const { return loaded_allocations_; }
    MockPlatform   &platform() { return image_.platform(); }
    const DdiTable &table() { return *image_.platform().registered_table(); }

private:
    DriverImage<MockPlatform, av::MockTraceHost> image_;
    int32_t                                      entered_;
    std::uint32_t                                loaded_allocations_;
};

class DisplayInfoAnswer {
public:
    DisplayInfoAnswer() : bytes_{}
    {
        write(0u, av::DisplayInfoResponse::response_type);
        write(av::DisplayInfoResponse::header_size + 8u, 1280u);
        write(av::DisplayInfoResponse::header_size + 12u, 800u);
        write(av::DisplayInfoResponse::header_size + 16u, 1u);
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::uint32_t       length() const { return sizeof bytes_; }

private:
    void write(std::size_t offset, std::uint32_t value)
    {
        for (unsigned byte = 0u; byte < 4u; byte++)
            bytes_[offset + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
    }

    std::uint8_t bytes_[av::DisplayInfoResponse::encoded_size];
};

constexpr std::uint32_t create_blob = 0x010Cu;
constexpr std::uint32_t map_blob = 0x0208u;
constexpr std::uint64_t host_region_address = 0x4000000000ull;
constexpr std::uint64_t host_region_bytes = 16u << 20u;
constexpr std::uint32_t get_capset_info = 0x0108u;
constexpr std::uint32_t get_capset = 0x0109u;
constexpr std::uint8_t  first_gpu[av::GpuIdentity::uuid_bytes] = {3};
constexpr std::uint8_t  other_gpu[av::GpuIdentity::uuid_bytes] = {4};

av::CapsetInfo host_capset(const std::uint8_t *uuid, std::uint64_t local_memory)
{
    return av::CapsetInfo(av::ProtocolVersion(av::ProtocolVersion::current_major,
                                              av::ProtocolVersion::current_minor),
                          1u, 64u, 12u, 24u, av::CapsetInfo::feature_host_visible_memory,
                          av::GpuIdentity(1u, 2u, av::GpuIdentity::discrete_gpu, uuid, local_memory, 0u, "the host's"));
}

class Device {
public:
    Device(LoadedDriver &driver, std::uint8_t index)
        : platform_(driver.platform()), dxgkrnl_(driver.platform(), function_, index),
          fdo_(driver.platform(), driver.table(), dxgkrnl_) {}

    FakeVirtioGpuFunction &function() { return function_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    FakeDxgkrnlFdo        &fdo() { return fdo_; }

    std::int32_t start_device()
    {
        platform_.on_stall(&Device::answer_display_info, this);
        const std::int32_t started = fdo_.start_device();
        platform_.on_stall(nullptr, nullptr);
        return started;
    }

    void forget_control() { control_.reset(); }

    std::int32_t resume()
    {
        platform_.on_stall(&Device::answer_display_info, this);
        const std::int32_t powered = fdo_.set_power_state(power_d0);
        platform_.on_stall(nullptr, nullptr);
        return powered;
    }

    void answer_capset_of(const std::uint8_t *uuid) { capset_ = host_capset(uuid, 0u); }
    void answer_local_memory(std::uint64_t bytes) { capset_ = host_capset(first_gpu, bytes); }
    template <typename Step>
    std::int32_t answering(Step step)
    {
        platform_.on_stall(&Device::answer_display_info, this);
        const std::int32_t status = step();
        platform_.on_stall(nullptr, nullptr);
        return status;
    }
    std::uint32_t released() const { return released_; }
    void refuse_releases() { refusing_releases_ = true; }
    std::uint64_t last_released() const { return last_released_; }
    bool created_blob() const { return created_blob_; }
    bool mapped_region() const { return mapped_region_; }

    void blank(std::uint32_t latched = 0u)
    {
        clock_.blank(latched);
        fdo_.dpc();
    }
    av::FakeDisplayClock &clock() { return clock_; }

    av::FakeVirtqueueDevice &control()
    {
        if (!control_.has_value()) {
            const av::FakeCommonConfig::Queue &queue = function_.common().queue(0u);
            control_.emplace(platform_, queue.desc, queue.driver, queue.device, queue.size);
        }
        return *control_;
    }

private:
    static std::uint32_t answer_release(void *context, const std::uint8_t *request, std::uint32_t length,
                                        std::uint8_t *answer, std::uint32_t room)
    {
        auto *const device = static_cast<Device *>(context);
        if (length >= av::SharedReleaseRequest::encoded_size) {
            std::memcpy(&device->last_released_, request + 24u, sizeof device->last_released_);
            device->released_++;
        }
        av::GpuResponse response;
        response.set(device->refusing_releases_ ? av::GpuResponse::err_unspec : av::GpuResponse::ok_nodata);
        const auto bytes = static_cast<std::uint32_t>(response.length());
        if (bytes > room) return 0u;
        std::memcpy(answer, response.bytes(), bytes);
        return bytes;
    }

    static void answer_display_info(void *context)
    {
        auto *const device = static_cast<Device *>(context);
        const std::uint32_t asked = device->control().pending_type();
        if (asked == create_blob || asked == map_blob) {
            av::GpuResponse answer;
            if (asked == map_blob) {
                device->mapped_region_ = true;
                answer.set_map_info(av::GpuResponse::map_cache_cached);
            } else {
                device->created_blob_ = true;
                answer.set(av::GpuResponse::ok_nodata);
            }
            static_cast<void>(device->control().respond_all(answer.bytes(),
                                                            static_cast<std::uint32_t>(answer.length())));
            return;
        }
        if (asked == av::PageRequest::display_clock) {
            device->clock_.answer(device->control());
            return;
        }
        if (asked == av::SharedReleaseRequest::type) {
            std::uint8_t request[av::FakeVirtqueueDevice::request_bytes] = {};
            static_cast<void>(device->control().answer_all(request, sizeof request, &Device::answer_release, device));
            return;
        }
        if (asked == get_capset_info || asked == get_capset) {
            av::GpuResponse answer;
            if (asked == get_capset_info)
                answer.set_capset_info(av::CapsetInfo::capset_id, 2u, av::CapsetInfo::encoded_size);
            else
                answer.set_capset(device->capset_);
            static_cast<void>(device->control().respond_all(answer.bytes(),
                                                            static_cast<std::uint32_t>(answer.length())));
            return;
        }
        static_cast<void>(device->control().respond_all(device->display_info_.bytes(),
                                                        device->display_info_.length()));
    }

    av::MockPlatform      &platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    FakeDxgkrnlFdo        fdo_;
    DisplayInfoAnswer     display_info_;
    av::CapsetInfo        capset_ = host_capset(first_gpu, 0u);
    bool                  created_blob_ = false;
    std::uint32_t         released_ = 0u;
    std::uint64_t         last_released_ = 0u;
    bool                  refusing_releases_ = false;
    bool                  mapped_region_ = false;
    av::FakeDisplayClock  clock_{platform_};
    std::optional<av::FakeVirtqueueDevice> control_;
};

}

TEST(ddi_scenario, the_recorded_bring_up_and_teardown)
{ LoadedDriver driver;
  CHECK_EQ(driver.entered(), 0);
  FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(&driver.table()), Status::ok);
  { Device device(driver, 0u);
    CHECK_EQ(device.fdo().add_device(), 0);
    CHECK(device.fdo().context() != nullptr);
    CHECK_EQ(device.start_device(), 0);
    CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
    CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x3200);
    CHECK_EQ(device.fdo().dxgadapter().display_caps_extension(), 0u);
    CHECK_EQ(device.fdo().sources(), 1u);
    CHECK_EQ(device.fdo().children(), 1u);
    CHECK_EQ(device.function().common().status(), driver_ok_status);
    CHECK_EQ(device.fdo().enumerate_children(), 0);
    CHECK_EQ(device.fdo().connected_children(), 1u);
    CHECK_EQ(device.fdo().interrupt(1u), 1u);
    CHECK_EQ(device.fdo().set_power_state(power_d3), 0);
    CHECK_EQ(device.function().common().status(), 0u);
    CHECK_EQ(device.fdo().set_power_state(power_d0), 0);
    CHECK_EQ(device.function().common().status(), driver_ok_status);
    CHECK_EQ(device.fdo().stop_device(), 0);
    CHECK_EQ(device.function().common().status(), 0u);
    CHECK_EQ(device.dxgkrnl().live_mappings(), 0u);
    CHECK_EQ(device.fdo().remove_device(), 0); }
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, stop_device_twice)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(device.fdo().stop_device(), 0);
  CHECK_EQ(device.fdo().stop_device(), 0);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, fifty_stop_and_start_cycles_leave_nothing_behind)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  const std::uint32_t after_first = driver.platform().live_allocations();
  for (std::uint32_t cycle = 0u; cycle < 50u; cycle++) {
      CHECK_EQ(device.fdo().stop_device(), 0);
      CHECK_EQ(device.function().common().status(), 0u);
      device.forget_control();
      CHECK_EQ(device.start_device(), 0);
      CHECK_EQ(device.function().common().status(), driver_ok_status);
  }
  CHECK_EQ(driver.platform().live_allocations(), after_first);
  CHECK_EQ(device.dxgkrnl().live_mappings(), 1u);
  CHECK_EQ(device.fdo().stop_device(), 0);
  CHECK_EQ(device.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, remove_device_without_stop_device)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  const std::int32_t remove = reinterpret_cast<std::int32_t (*)(void *)>(
      driver.table().routine(DdiSlot::DxgkDdiRemoveDevice))(device.fdo().context());
  CHECK_EQ(remove, 0);
  CHECK_EQ(device.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(device.function().common().status(), 0u);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations()); }

TEST(ddi_scenario, an_interrupt_before_start_device_finished)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().interrupt(0u), 0u);
  CHECK_EQ(device.dxgkrnl().dpcs_queued(), 0u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_failed_start_is_stopped_then_removed)
{ LoadedDriver driver;
  Device device(driver, 0u);
  device.function().common().offer_features(FakeVirtioGpuFunction::feature_edid);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(nt(device.start_device()), not_supported);
  CHECK(!device.fdo().started());
  CHECK_EQ(device.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations()); }

TEST(ddi_scenario, reset_device_at_bugcheck_then_teardown)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  const std::uint32_t allocations = driver.platform().allocations_made();
  device.fdo().reset_device();
  CHECK_EQ(device.function().common().status(), 0u);
  CHECK_EQ(driver.platform().allocations_made(), allocations);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, one_adapter_per_host_gpu)
{ LoadedDriver driver;
  Device first(driver, 0u);
  Device second(driver, 1u);
  CHECK_EQ(first.fdo().add_device(), 0);
  CHECK_EQ(second.fdo().add_device(), 0);
  CHECK(first.fdo().context() != second.fdo().context());
  CHECK_EQ(first.fdo().start_device(), 0);
  CHECK_EQ(second.fdo().start_device(), 0);
  CHECK_EQ(first.fdo().interrupt(1u), 1u);
  CHECK_EQ(second.dxgkrnl().dpcs_queued(), 0u);
  CHECK_EQ(first.fdo().remove_device(), 0);
  CHECK_EQ(second.function().common().status(), driver_ok_status);
  CHECK_EQ(second.fdo().remove_device(), 0); }

TEST(ddi_scenario, add_device_past_the_adapter_ceiling_is_no_memory)
{ LoadedDriver driver;
  FakeVirtioGpuFunction function;
  FakeDxgkrnlAdapter dxgkrnl(driver.platform(), function);
  std::deque<FakeDxgkrnlFdo> fdos;
  for (std::uint32_t i = 0u; i <= av::most_av_devices; i++) fdos.emplace_back(driver.platform(), driver.table(), dxgkrnl);
  for (std::uint32_t i = 0u; i < av::most_av_devices; i++) CHECK_EQ(fdos[i].add_device(), 0);
  FakeDxgkrnlFdo &past = fdos[av::most_av_devices];
  CHECK_EQ(nt(past.add_device()), no_memory);
  CHECK(past.context() == nullptr);
  CHECK_EQ(driver.platform().logged(), av::most_av_devices + 2u);
  CHECK(std::strstr(driver.platform().last_log(), "no free adapter slot") != nullptr); }


namespace {

constexpr std::int32_t revision_mismatch = static_cast<std::int32_t>(0xC0000059u);

std::int32_t answers_an_unknown_wddm_version(void *, const av::DxgkargQueryAdapterInfoRecord *query)
{
    if (query->Type != 1) return static_cast<std::int32_t>(0xC0000002u);
    av::DxgkDriverCapsRecord caps = {};
    caps.WDDMVersion = 0x1234;
    std::memcpy(query->pOutputData, &caps, sizeof caps);
    return 0;
}

}

TEST(ddi_scenario, every_query_adapter_info_type_answers_a_status_dxgkrnl_accepts)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  std::uint8_t input[0x250] = {}, output[0x250] = {};
  for (std::int32_t type = -1; type <= 49; type++)
      static_cast<void>(device.fdo().dxgadapter().query(device.fdo().context(), type, input, sizeof input,
                                                        output, sizeof output));
  CHECK_EQ(device.start_device(), 0);
  for (std::int32_t type = -1; type <= 49; type++)
      static_cast<void>(device.fdo().dxgadapter().query(device.fdo().context(), type, input, 0u, output, 0u));
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_refused_capability_fails_start_and_stops_the_device)
{ LoadedDriver driver;
  DdiTable table;
  for (std::uint16_t index = 1u; index < DdiTable::slot_count; index++)
      CHECK_STATUS(table.set(DdiSlot{index}, driver.table().routine(DdiSlot{index})), Status::ok);
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiQueryAdapterInfo,
                         reinterpret_cast<DdiRoutine>(&answers_an_unknown_wddm_version)), Status::ok);
  FakeVirtioGpuFunction function;
  FakeDxgkrnlAdapter dxgkrnl(driver.platform(), function);
  FakeDxgkrnlFdo fdo(driver.platform(), table, dxgkrnl);
  CHECK_EQ(fdo.add_device(), 0);
  CHECK_EQ(fdo.start_device(), revision_mismatch);
  CHECK(fdo.dxgadapter().failed_rule() == CapabilityRule::unknown_wddm_version);
  CHECK(!fdo.started());
  CHECK_EQ(function.common().status(), 0u);
  CHECK_EQ(dxgkrnl.live_mappings(), 0u);
  CHECK_EQ(fdo.remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations()); }

TEST(ddi_scenario, escape_and_debug_collection_through_the_table)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  std::uint8_t data[16] = {};
  av::DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = sizeof data;
  const auto escape_routine = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargEscapeRecord *)>(
      driver.table().routine(DdiSlot::DxgkDdiEscape));
  CHECK_EQ(nt(escape_routine(device.fdo().context(), &escape)), not_supported);
  std::uint8_t report[16] = {};
  av::DxgkargCollectDbgInfoRecord collect = {};
  collect.pBuffer = report;
  collect.BufferSize = sizeof report;
  const auto collect_routine = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargCollectDbgInfoRecord *)>(
      driver.table().routine(DdiSlot::DxgkDdiCollectDbgInfo));
  CHECK_EQ(collect_routine(device.fdo().context(), &collect), 0);
  CHECK_EQ(report[6], 1u);
  CHECK_EQ(device.fdo().remove_device(), 0); }



TEST(ddi_scenario, vidmm_accepts_the_aperture_segment_after_start)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  av::FakeVidMm vidmm(device.fdo().dxgadapter());
  CHECK_EQ(vidmm.create_segments(device.fdo().context()), 0);
  CHECK(vidmm.failed_rule() == av::SegmentRule::none);
  CHECK(vidmm.segment(0u).Size == std::uint64_t{8} << 30u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, vidmm_counts_the_host_gpu_s_memory_as_dedicated_video_memory)
{ LoadedDriver driver;
  Device device(driver, 0u);
  device.answer_local_memory(17095983104ull);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  av::FakeVidMm vidmm(device.fdo().dxgadapter());
  CHECK_EQ(vidmm.create_segments(device.fdo().context()), 0);
  CHECK(vidmm.failed_rule() == av::SegmentRule::none);
  CHECK_EQ(vidmm.answer().NbSegment, 2u);
  CHECK(vidmm.dedicated_video_memory() == 17095983104ull);
  CHECK(vidmm.dedicated_system_memory() == 0u);
  CHECK(vidmm.aperture_memory() == std::uint64_t{8} << 30u);

  CHECK_EQ(device.fdo().stop_device(), 0);
  device.forget_control();
  device.answer_local_memory(0u);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(vidmm.create_segments(device.fdo().context()), 0);
  CHECK_EQ(vidmm.answer().NbSegment, 1u);
  CHECK(vidmm.dedicated_video_memory() == 0u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, the_render_core_is_created_once_present_arrives)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(device.fdo().dxgadapter().create_render_core(), 0);
  CHECK(device.fdo().dxgadapter().render_adapter());
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
  CHECK(device.fdo().dxgadapter().missing_render_slot() == DdiSlot::Version);
  CHECK_EQ(device.fdo().remove_device(), 0); }

namespace {

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

class Runtime {
public:
    Runtime(const DdiTable &table, void *adapter, std::uint32_t width = 1024u, std::uint32_t height = 768u)
        : table_(table), adapter_(adapter), surface_{width, height, 21, 60u, 1u, 0u}, descriptor_{}, info_{},
          open_{}, device_(nullptr), frames_{} {}

    template <typename Routine>
    Routine routine(DdiSlot slot) const { return reinterpret_cast<Routine>(table_.routine(slot)); }

    std::int32_t create_primary()
    {
        av::DxgkargGetStandardAllocationDriverDataRecord describe = {};
        describe.StandardAllocationType = 1;
        describe.pCreateSurfaceData = &surface_;
        std::int32_t status = routine<std::int32_t (*)(void *, av::DxgkargGetStandardAllocationDriverDataRecord *)>(
            DdiSlot::DxgkDdiGetStandardAllocationDriverData)(adapter_, &describe);
        if (status < 0) return status;
        describe.pAllocationPrivateDriverData = descriptor_;
        status = routine<std::int32_t (*)(void *, av::DxgkargGetStandardAllocationDriverDataRecord *)>(
            DdiSlot::DxgkDdiGetStandardAllocationDriverData)(adapter_, &describe);
        if (status < 0) return status;
        info_.pPrivateDriverData = descriptor_;
        info_.PrivateDriverDataSize = describe.AllocationPrivateDriverDataSize;
        info_.Flags2 = 0xFFFFFFFFu;
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info_;
        status = routine<std::int32_t (*)(void *, av::DxgkargCreateAllocationRecord *)>(DdiSlot::DxgkDdiCreateAllocation)(
            adapter_, &create);
        if (status >= 0) kernel_handle_ = handles_.insert(info_.hAllocation);
        return status;
    }

    std::uint32_t flags2() const { return info_.Flags2; }
    void         *primary() const { return info_.hAllocation; }

    std::int32_t map_aperture(av::MockPlatform &platform, std::uint32_t pages)
    {
        for (std::uint32_t index = 0u; index < pages; index++) frames_[index] = first_frame + index * page_bytes;
        av::DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
        mapping.hAllocation = info_.hAllocation;
        mapping.SegmentId = av::ApertureSegment::segment_id;
        mapping.NumberOfPages = pages;
        mapping.pMdl = platform.attach_page_frames(frames_, pages);
        std::uint8_t                       dma[64] = {};
        av::DxgkargBuildPagingBufferRecord paging = {};
        paging.pDmaBuffer = dma;
        paging.DmaSize = sizeof dma;
        paging.Operation = 5;
        std::memcpy(paging.Reserved, &mapping, sizeof mapping);
        return routine<std::int32_t (*)(void *, av::DxgkargBuildPagingBufferRecord *)>(
            DdiSlot::DxgkDdiBuildPagingBuffer)(adapter_, &paging);
    }

    std::int32_t create_device()
    {
        av::DxgkargCreateDeviceRecord create = {};
        const std::int32_t status =
            routine<std::int32_t (*)(void *, av::DxgkargCreateDeviceRecord *)>(DdiSlot::DxgkDdiCreateDevice)(adapter_, &create);
        device_ = create.hDevice;
        return status;
    }

    std::int32_t open()
    {
        open_.hAllocation = kernel_handle_;
        av::DxgkargOpenAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pOpenAllocation = &open_;
        return routine<std::int32_t (*)(void *, av::DxgkargOpenAllocationRecord *)>(DdiSlot::DxgkDdiOpenAllocation)(device_, &request);
    }

    std::int32_t describe(av::DxgkargDescribeAllocationRecord &request)
    {
        request.hAllocation = info_.hAllocation;
        return routine<std::int32_t (*)(void *, av::DxgkargDescribeAllocationRecord *)>(DdiSlot::DxgkDdiDescribeAllocation)(
            adapter_, &request);
    }

    std::int32_t set_private_format(std::uint32_t attribute)
    {
        av::DxgkargSetDisplayPrivateDriverFormatRecord request = {};
        request.PrimaryAllocation = info_.hAllocation;
        request.PrivateDriverFormatAttribute = attribute;
        return routine<std::int32_t (*)(void *, const av::DxgkargSetDisplayPrivateDriverFormatRecord *)>(
            DdiSlot::DxgkDdiSetDisplayPrivateDriverFormat)(adapter_, &request);
    }

    std::int32_t close()
    {
        void *handle = open_.hDeviceSpecificAllocation;
        av::DxgkargCloseAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pOpenHandleList = &handle;
        return routine<std::int32_t (*)(void *, const av::DxgkargCloseAllocationRecord *)>(DdiSlot::DxgkDdiCloseAllocation)(
            device_, &request);
    }

    std::int32_t destroy_primary()
    {
        void *handle = info_.hAllocation;
        av::DxgkargDestroyAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationList = &handle;
        return routine<std::int32_t (*)(void *, const av::DxgkargDestroyAllocationRecord *)>(DdiSlot::DxgkDdiDestroyAllocation)(
            adapter_, &request);
    }

    std::int32_t destroy_device()
    { return routine<std::int32_t (*)(void *)>(DdiSlot::DxgkDdiDestroyDevice)(device_); }

private:
    const DdiTable                            &table_;
    void                                      *adapter_;
    av::D3dkmdtSharedPrimarySurfaceDataRecord  surface_;
    std::uint8_t                               descriptor_[av::AllocationDescriptor::encoded_size];
    av::DxgkAllocationInfoRecord               info_;
    av::DxgkOpenAllocationInfoRecord           open_;
    void                                      *device_;
    av::FakeHandleTable                        handles_;
    std::uint32_t                              kernel_handle_ = 0u;
    static constexpr std::uint64_t             page_bytes = 0x1000u;
    static constexpr std::uint64_t             first_frame = 0x40000000u;
    static constexpr std::uint32_t             max_pages = 1024u;
    std::uint64_t                              frames_[max_pages];
};

}

constexpr std::uint64_t display_period = 10000000u / 60u;

TEST(ddi_scenario, a_committed_source_reaches_the_host_scanout)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  void          *adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  Runtime         runtime(table, adapter, 1280u, 800u);
  CHECK_EQ(runtime.create_primary(), 0);
  CHECK_EQ(runtime.map_aperture(driver.platform(), 1000u), 0);
  av::FakeVidPn   vidpn;
  vidpn.add_path(0u, 0u);
  av::D3dkmdtVidPnSourceModeRecord pinned = {};
  av::DisplayMode(1280u, 800u).describe(pinned, av::DisplayMode::source_formats[0]);
  vidpn.source_modes(0u).seed(pinned, true);
  const av::DxgkargCommitVidPnRecord commit = {&vidpn, 0u, 0, nullptr, 0u, 0u};
  const auto commit_vidpn = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargCommitVidPnRecord *)>(
      table.routine(DdiSlot::DxgkDdiCommitVidPn));
  CHECK_EQ(commit_vidpn(adapter, &commit), 0);
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {0u, 1u, {}};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSetVidPnSourceVisibilityRecord *)>(
               table.routine(DdiSlot::DxgkDdiSetVidPnSourceVisibility))(adapter, &visible),
           0);
  av::DxgkargSetVidPnSourceAddressRecord address = {};
  address.PrimaryAddress = av::ApertureSegment::base_address;
  address.PrimarySegment = av::ApertureSegment::segment_id;
  address.hAllocation = runtime.primary();
  driver.platform().set_irql(av::Irql::isr);
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSetVidPnSourceAddressRecord *)>(
               table.routine(DdiSlot::DxgkDdiSetVidPnSourceAddress))(adapter, &address),
           0);
  driver.platform().advance_time(display_period);
  driver.platform().set_irql(av::Irql::dispatch);
  reinterpret_cast<void (*)(void *)>(table.routine(DdiSlot::DxgkDdiDpcRoutine))(adapter);
  driver.platform().set_irql(av::Irql::passive);
  av::FakeVirtqueueDevice &control = device.control();
  CHECK_EQ(control.respond_all(0x1100u), 5u);
  CHECK_EQ(control.request_type(0u), 0x02F4u);
  CHECK_EQ(control.request_type(5u), 0x0101u);
  CHECK_EQ(control.request_type(6u), 0x0106u);
  CHECK_EQ(control.request_type(7u), 0x0103u);
  CHECK_EQ(control.request_type(8u), 0x0105u);
  CHECK_EQ(control.request_type(9u), 0x0104u);
  CHECK_EQ(u32(control.request(6u), 28u), 1u);
  CHECK_EQ(u32(control.request(6u), 32u), 0x40000000u);
  CHECK_EQ(u32(control.request(6u), 40u), 1280u * 800u * 4u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_scanning_source_survives_d3_and_comes_back_at_d0)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  void           *adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  Runtime         runtime(table, adapter, 1280u, 800u);
  CHECK_EQ(runtime.create_primary(), 0);
  CHECK_EQ(runtime.map_aperture(driver.platform(), 1000u), 0);
  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  av::D3dkmdtVidPnSourceModeRecord pinned = {};
  av::DisplayMode(1280u, 800u).describe(pinned, av::DisplayMode::source_formats[0]);
  vidpn.source_modes(0u).seed(pinned, true);
  const av::DxgkargCommitVidPnRecord commit = {&vidpn, 0u, 0, nullptr, 0u, 0u};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargCommitVidPnRecord *)>(
               table.routine(DdiSlot::DxgkDdiCommitVidPn))(adapter, &commit),
           0);
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {0u, 1u, {}};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSetVidPnSourceVisibilityRecord *)>(
               table.routine(DdiSlot::DxgkDdiSetVidPnSourceVisibility))(adapter, &visible),
           0);
  av::DxgkargSetVidPnSourceAddressRecord address = {};
  address.PrimaryAddress = av::ApertureSegment::base_address;
  address.PrimarySegment = av::ApertureSegment::segment_id;
  address.hAllocation = runtime.primary();
  const auto set_address = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSetVidPnSourceAddressRecord *)>(
      table.routine(DdiSlot::DxgkDdiSetVidPnSourceAddress));
  const auto dpc = reinterpret_cast<void (*)(void *)>(table.routine(DdiSlot::DxgkDdiDpcRoutine));
  driver.platform().set_irql(av::Irql::isr);
  CHECK_EQ(set_address(adapter, &address), 0);
  driver.platform().advance_time(display_period);
  driver.platform().set_irql(av::Irql::dispatch);
  dpc(adapter);
  driver.platform().set_irql(av::Irql::passive);
  CHECK_EQ(device.control().respond_all(0x1100u), 5u);

  CHECK_EQ(device.fdo().set_power_state(power_d3), 0);
  CHECK_EQ(device.function().common().status(), 0u);
  CHECK_EQ(device.fdo().set_power_state(power_d0), 0);
  CHECK_EQ(device.function().common().status(), driver_ok_status);
  device.forget_control();
  av::FakeVirtqueueDevice &resumed = device.control();
  CHECK_EQ(resumed.respond_all(0x1100u), 2u);
  CHECK_EQ(resumed.request_type(0u), 0x02F4u);
  CHECK_EQ(resumed.request_type(1u), 0x0101u);

  driver.platform().set_irql(av::Irql::isr);
  CHECK_EQ(set_address(adapter, &address), 0);
  driver.platform().advance_time(display_period);
  driver.platform().set_irql(av::Irql::dispatch);
  dpc(adapter);
  driver.platform().set_irql(av::Irql::passive);
  static_cast<void>(resumed.respond_all(0x1100u));
  std::uint32_t created = 0u, attached = 0u, scanned = 0u;
  for (std::uint32_t index = 0u; index < resumed.requests(); index++) {
      const std::uint32_t type = resumed.request_type(index);
      created += type == 0x0101u ? 1u : 0u;
      attached += type == 0x0106u ? 1u : 0u;
      scanned += type == 0x0103u ? 1u : 0u;
  }
  CHECK_EQ(created, 1u);
  CHECK_EQ(attached, 1u);
  CHECK_EQ(scanned, 1u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, an_engine_that_stops_moving_is_reset_with_nothing_more_reported)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  void           *adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  av::DxgkargCreateDeviceRecord created = {};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, av::DxgkargCreateDeviceRecord *)>(
               table.routine(DdiSlot::DxgkDdiCreateDevice))(adapter, &created),
           0);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, av::DxgkargCreateContextRecord *)>(
               table.routine(DdiSlot::DxgkDdiCreateContext))(created.hDevice, &context),
           0);
  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSubmitCommandVirtualRecord *)>(
               table.routine(DdiSlot::DxgkDdiSubmitCommandVirtual))(adapter, &submit),
           0);

  const auto engine_status = reinterpret_cast<std::int32_t (*)(void *, av::DxgkargQueryEngineStatusRecord *)>(
      table.routine(DdiSlot::DxgkDdiQueryEngineStatus));
  driver.platform().set_irql(av::Irql::dispatch);
  av::DxgkargQueryEngineStatusRecord status = {0u, 0u, 0u};
  CHECK_EQ(engine_status(adapter, &status), 0);
  CHECK_EQ(status.EngineStatus, 1u);
  driver.platform().advance_time(20000000u);
  status.EngineStatus = 0u;
  CHECK_EQ(engine_status(adapter, &status), 0);
  CHECK_EQ(status.EngineStatus, 0u);

  const std::uint32_t before = device.dxgkrnl().interrupts();
  av::DxgkargResetEngineRecord reset = {0u, 0u, 0u};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, av::DxgkargResetEngineRecord *)>(
               table.routine(DdiSlot::DxgkDdiResetEngine))(adapter, &reset),
           0);
  CHECK_EQ(reset.LastAbortedFenceId, 1u);
  CHECK_EQ(device.dxgkrnl().interrupts(), before);
  status.EngineStatus = 0u;
  CHECK_EQ(engine_status(adapter, &status), 0);
  CHECK_EQ(status.EngineStatus, 1u);
  driver.platform().set_irql(av::Irql::passive);

  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *)>(
               table.routine(DdiSlot::DxgkDdiDestroyContext))(context.hContext),
           0);
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *)>(
               table.routine(DdiSlot::DxgkDdiDestroyDevice))(created.hDevice),
           0);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_device_that_changed_across_hibernation_refuses_its_contexts)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  void           *adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  av::DxgkargCreateDeviceRecord created = {};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, av::DxgkargCreateDeviceRecord *)>(
               table.routine(DdiSlot::DxgkDdiCreateDevice))(adapter, &created),
           0);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, av::DxgkargCreateContextRecord *)>(
               table.routine(DdiSlot::DxgkDdiCreateContext))(created.hDevice, &context),
           0);
  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  const auto submit_virtual = reinterpret_cast<std::int32_t (*)(void *,
                                                                const av::DxgkargSubmitCommandVirtualRecord *)>(
      table.routine(DdiSlot::DxgkDdiSubmitCommandVirtual));
  CHECK_EQ(submit_virtual(adapter, &submit), 0);

  CHECK_EQ(device.fdo().set_power_state(power_d3), 0);
  device.forget_control();
  device.answer_capset_of(other_gpu);
  const std::uint32_t before = device.dxgkrnl().interrupts();
  CHECK_EQ(device.resume(), 0);
  CHECK_EQ(device.function().common().status(), driver_ok_status);
  CHECK_EQ(device.dxgkrnl().interrupts(), before);

  av::DxgkargCreateContextRecord refused = {};
  refused.Flags = 0x4u;
  CHECK(reinterpret_cast<std::int32_t (*)(void *, av::DxgkargCreateContextRecord *)>(
            table.routine(DdiSlot::DxgkDdiCreateContext))(created.hDevice, &refused) < 0);
  submit.SubmissionFenceId = 2u;
  CHECK_EQ(static_cast<std::uint32_t>(submit_virtual(adapter, &submit)), 0xC000000Du);
  CHECK_EQ(device.dxgkrnl().interrupts(), before);

  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  av::D3dkmdtVidPnSourceModeRecord pinned = {};
  av::DisplayMode(1280u, 800u).describe(pinned, av::DisplayMode::source_formats[0]);
  vidpn.source_modes(0u).seed(pinned, true);
  const av::DxgkargCommitVidPnRecord commit = {&vidpn, 0u, 0, nullptr, 0u, 0u};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargCommitVidPnRecord *)>(
               table.routine(DdiSlot::DxgkDdiCommitVidPn))(adapter, &commit),
           0);

  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *)>(
               table.routine(DdiSlot::DxgkDdiDestroyContext))(context.hContext),
           0);
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *)>(
               table.routine(DdiSlot::DxgkDdiDestroyDevice))(created.hDevice),
           0);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

namespace {

class SharedRuntime {
public:
    static constexpr std::uint64_t bytes = 4u * 4096u;

    SharedRuntime(const DdiTable &table, void *adapter)
        : table_(table), adapter_(adapter), info_{}, opens_{}, devices_{}, private_{}, kernel_handle_(0u) {}

    template <typename Routine>
    Routine routine(DdiSlot slot) const { return reinterpret_cast<Routine>(table_.routine(slot)); }

    std::int32_t create(std::uint64_t record_size, std::uint32_t kind = av::SharedObjectKind::memory)
    {
        const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                                  av::SurfaceLayout(64u, 64u, 21u, 256u), bytes,
                                                  av::ScanoutTarget());
        CHECK_STATUS(descriptor.encode(private_, av::AllocationDescriptor::encoded_size), av::Status::ok);
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(kind), 0u, record_size);
        CHECK_STATUS(asking.encode(private_ + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     av::Status::ok);
        info_.pPrivateDriverData = private_;
        info_.PrivateDriverDataSize = sizeof private_;
        info_.Flags2 = 0xFFFFFFFFu;
        av::DxgkargCreateAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationInfo = &info_;
        request.Flags = resource_flag;
        const std::int32_t status =
            routine<std::int32_t (*)(void *, av::DxgkargCreateAllocationRecord *)>(DdiSlot::DxgkDdiCreateAllocation)(
                adapter_, &request);
        if (status >= 0) {
            kernel_handle_ = handles_.insert(info_.hAllocation);
            resource_ = request.hResource;
        }
        return status;
    }

    void *resource() const { return resource_; }

    std::int32_t creator_open(std::uint32_t which, std::uint64_t record_size,
                              std::uint32_t kind = av::SharedObjectKind::memory)
    {
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(kind), 0u, record_size);
        CHECK_STATUS(asking.encode(private_ + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     av::Status::ok);
        return open(which);
    }

    av::SharedObjectRecord minted() const
    {
        av::SharedObjectRecord record;
        CHECK_STATUS(record.decode(private_ + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     av::Status::ok);
        return record;
    }

    std::int32_t create_device(std::uint32_t which)
    {
        av::DxgkargCreateDeviceRecord create = {};
        const std::int32_t status =
            routine<std::int32_t (*)(void *, av::DxgkargCreateDeviceRecord *)>(DdiSlot::DxgkDdiCreateDevice)(
                adapter_, &create);
        devices_[which] = create.hDevice;
        return status;
    }

    std::int32_t open(std::uint32_t which, const std::uint8_t *carried = nullptr,
                      std::uint32_t carried_size = 0u)
    {
        opens_[which] = av::DxgkOpenAllocationInfoRecord{};
        opens_[which].hAllocation = kernel_handle_;
        opens_[which].pPrivateDriverData =
            const_cast<std::uint8_t *>(carried != nullptr ? carried : private_);
        opens_[which].PrivateDriverDataSize = carried != nullptr ? carried_size : sizeof private_;
        av::DxgkargOpenAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pOpenAllocation = &opens_[which];
        return routine<std::int32_t (*)(void *, av::DxgkargOpenAllocationRecord *)>(DdiSlot::DxgkDdiOpenAllocation)(
            devices_[which], &request);
    }

    std::int32_t close(std::uint32_t which)
    {
        void *handle = opens_[which].hDeviceSpecificAllocation;
        av::DxgkargCloseAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pOpenHandleList = &handle;
        return routine<std::int32_t (*)(void *, const av::DxgkargCloseAllocationRecord *)>(
            DdiSlot::DxgkDdiCloseAllocation)(devices_[which], &request);
    }

    std::int32_t destroy()
    {
        void *handle = info_.hAllocation;
        av::DxgkargDestroyAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationList = &handle;
        request.hResource = resource_;
        request.Flags = resource_ != nullptr ? resource_flag : 0u;
        const std::int32_t status =
            routine<std::int32_t (*)(void *, const av::DxgkargDestroyAllocationRecord *)>(
                DdiSlot::DxgkDdiDestroyAllocation)(adapter_, &request);
        if (status >= 0) resource_ = nullptr;
        return status;
    }

    std::int32_t destroy_device(std::uint32_t which)
    { return routine<std::int32_t (*)(void *)>(DdiSlot::DxgkDdiDestroyDevice)(devices_[which]); }

private:
    const DdiTable                  &table_;
    void                            *adapter_;
    av::DxgkAllocationInfoRecord     info_;
    void                            *resource_ = nullptr;
    av::DxgkOpenAllocationInfoRecord opens_[2];
    void                            *devices_[2];
    std::uint8_t                     private_[av::AllocationDescriptor::encoded_size +
                                              av::SharedObjectRecord::encoded_size];
    av::FakeHandleTable              handles_;
    std::uint32_t                    kernel_handle_;
};

}

TEST(ddi_scenario, a_shared_allocation_is_opened_by_a_second_device_and_closed_in_either_order)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { SharedRuntime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create(SharedRuntime::bytes), 0);
    CHECK_EQ(runtime.create_device(0u), 0);
    CHECK_EQ(runtime.create_device(1u), 0);
    CHECK(!runtime.minted().assigned());
    CHECK_EQ(runtime.creator_open(0u, SharedRuntime::bytes), 0);
    const av::SharedObjectRecord record = runtime.minted();
    CHECK(record.assigned());
    CHECK(record.object_id() != 0u);
    CHECK(record.adapter_id() != 0u);
    CHECK_EQ(record.size(), SharedRuntime::bytes);
    CHECK_EQ(runtime.open(1u), 0);
    CHECK_EQ(nt(runtime.destroy()), invalid_device_state);
    CHECK_EQ(runtime.close(1u), 0);
    CHECK_EQ(nt(runtime.destroy()), invalid_device_state);
    CHECK_EQ(runtime.close(0u), 0);
    CHECK_EQ(runtime.destroy(), 0);
    CHECK_EQ(runtime.destroy_device(1u), 0);
    CHECK_EQ(runtime.destroy_device(0u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, the_destroy_after_the_last_close_tells_the_device_the_shared_object_is_gone)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { SharedRuntime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create(SharedRuntime::bytes), 0);
    CHECK_EQ(runtime.create_device(0u), 0);
    CHECK_EQ(runtime.create_device(1u), 0);
    CHECK_EQ(runtime.creator_open(0u, SharedRuntime::bytes), 0);
    const std::uint64_t id = runtime.minted().object_id();
    CHECK_EQ(runtime.open(1u), 0);
    CHECK_EQ(runtime.close(0u), 0);
    CHECK_EQ(nt(device.answering([&] { return runtime.destroy(); })), invalid_device_state);
    CHECK_EQ(device.released(), 0u);
    CHECK_EQ(runtime.close(1u), 0);
    CHECK_EQ(device.answering([&] { return runtime.destroy(); }), 0);
    CHECK_EQ(device.released(), 1u);
    CHECK_EQ(device.last_released(), id);
    CHECK_EQ(runtime.destroy_device(1u), 0);
    CHECK_EQ(runtime.destroy_device(0u), 0); }
  CHECK_EQ(device.released(), 1u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_refused_release_is_logged_until_the_budget_is_spent)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  device.refuse_releases();
  std::uint32_t logged = driver.platform().logged();
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++) {
      SharedRuntime runtime(driver.table(), device.fdo().context());
      CHECK_EQ(runtime.create(SharedRuntime::bytes), 0);
      CHECK_EQ(runtime.create_device(0u), 0);
      CHECK_EQ(runtime.creator_open(0u, SharedRuntime::bytes), 0);
      CHECK_EQ(runtime.close(0u), 0);
      logged = driver.platform().logged();
      CHECK_EQ(device.answering([&] { return runtime.destroy(); }), 0);
      CHECK_EQ(device.released(), line + 1u);
      CHECK_EQ(runtime.destroy_device(0u), 0);
  }
  CHECK(std::strstr(driver.platform().last_log(), "avgpu: a shared object's release") != nullptr);
  CHECK_EQ(driver.platform().logged(), logged);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_shared_object_destroyed_after_surprise_removal_tells_nobody)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { SharedRuntime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create(SharedRuntime::bytes), 0);
    CHECK_EQ(runtime.create_device(0u), 0);
    CHECK_EQ(runtime.creator_open(0u, SharedRuntime::bytes), 0);
    CHECK_EQ(runtime.close(0u), 0);
    CHECK_EQ(device.fdo().surprise_removal(), 0);
    const std::uint32_t logged = driver.platform().logged();
    CHECK_EQ(device.answering([&] { return runtime.destroy(); }), 0);
    CHECK_EQ(device.released(), 0u);
    CHECK_EQ(driver.platform().logged(), logged);
    CHECK_EQ(runtime.destroy_device(0u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_device_destroyed_holding_a_shared_allocation_lets_go_of_it)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { SharedRuntime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create(SharedRuntime::bytes), 0);
    CHECK_EQ(runtime.create_device(0u), 0);
    CHECK_EQ(runtime.create_device(1u), 0);
    CHECK_EQ(runtime.creator_open(0u, SharedRuntime::bytes), 0);
    CHECK_EQ(runtime.open(1u), 0);
    CHECK_EQ(runtime.destroy_device(1u), 0);
    CHECK_EQ(nt(runtime.destroy()), invalid_device_state);
    CHECK_EQ(runtime.destroy_device(0u), 0);
    CHECK_EQ(runtime.destroy(), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations()); }

TEST(ddi_scenario, an_open_carrying_a_forged_record_is_refused_and_opens_nothing)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { SharedRuntime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create(SharedRuntime::bytes), 0);
    CHECK_EQ(runtime.create_device(0u), 0);
    CHECK_EQ(runtime.creator_open(0u, SharedRuntime::bytes), 0);
    const av::SharedObjectRecord real = runtime.minted();
    CHECK(real.assigned());
    CHECK_EQ(runtime.close(0u), 0);
    std::uint8_t forged[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
    const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                              av::SurfaceLayout(64u, 64u, 21u, 256u), SharedRuntime::bytes,
                                              av::ScanoutTarget());
    CHECK_STATUS(descriptor.encode(forged, av::AllocationDescriptor::encoded_size), av::Status::ok);
    struct { std::uint64_t id; std::uint64_t adapter; std::uint64_t size; std::uint32_t kind; } lies[] = {
        {real.object_id() + 1u, real.adapter_id(), SharedRuntime::bytes, av::SharedObjectKind::memory},
        {real.object_id(), real.adapter_id() ^ 2u, SharedRuntime::bytes, av::SharedObjectKind::memory},
        {real.object_id(), real.adapter_id(), SharedRuntime::bytes, av::SharedObjectKind::native_fence},
        {real.object_id(), real.adapter_id(), SharedRuntime::bytes * 2u, av::SharedObjectKind::memory},
    };
    for (const auto &lie : lies) {
        av::SharedObjectRecord record(0u, av::SharedObjectKind(lie.kind), 0u, lie.size);
        record.assign(lie.id, lie.adapter);
        CHECK_STATUS(record.encode(forged + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     av::Status::ok);
        CHECK(runtime.open(0u, forged, sizeof forged) < 0);
    }
    CHECK(runtime.open(0u, forged, av::AllocationDescriptor::encoded_size) < 0);
    CHECK_EQ(runtime.open(0u), 0);
    CHECK_EQ(runtime.close(0u), 0);
    CHECK_EQ(runtime.destroy(), 0);
    CHECK_EQ(runtime.destroy_device(0u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations()); }

TEST(ddi_scenario, a_primary_lives_from_standard_allocation_to_destruction)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { Runtime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create_primary(), 0);
    CHECK_EQ(runtime.create_device(), 0);
    CHECK_EQ(runtime.open(), 0);
    CHECK_EQ(runtime.set_private_format(0x42u), 0);
    av::DxgkargDescribeAllocationRecord described = {};
    CHECK_EQ(runtime.describe(described), 0);
    CHECK_EQ(described.Width, 1024u);
    CHECK_EQ(described.RefreshRateNumerator, 60u);
    CHECK_EQ(described.PrivateDriverFormatAttribute, 0x42u);
    CHECK_EQ(runtime.close(), 0);
    CHECK_EQ(runtime.destroy_primary(), 0);
    CHECK_EQ(runtime.destroy_device(), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, destroy_device_with_an_open_then_the_allocation)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { Runtime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create_primary(), 0);
    CHECK_EQ(runtime.create_device(), 0);
    CHECK_EQ(runtime.open(), 0);
    CHECK_EQ(nt(runtime.destroy_primary()), invalid_device_state);
    CHECK_EQ(runtime.destroy_device(), 0);
    CHECK_EQ(runtime.destroy_primary(), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations()); }

TEST(ddi_scenario, remove_device_with_devices_and_allocations_outstanding)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { Runtime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create_primary(), 0);
    CHECK_EQ(runtime.create_device(), 0);
    CHECK_EQ(runtime.open(), 0);
    CHECK_EQ(device.fdo().remove_device(), 0); }
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations()); }


TEST(ddi_scenario, render_patch_submit_and_three_fences_complete_through_the_dpc)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0u), 0);
    std::uint8_t command[32] = {0x41, 0x56, 0x53, 0x00};
    for (int packet = 0; packet < 3; packet++) {
        CHECK_EQ(vidsch.render(command, sizeof command, 8u, 0xC0002000 + static_cast<std::int64_t>(packet)), 0);
        CHECK_EQ(vidsch.submit(), 0);
    }
    CHECK_EQ(vidsch.dma()[0], 0x41u);
    CHECK_EQ(vidsch.dma()[8], 0x02u);
    CHECK_EQ(vidsch.dma()[9], 0x20u);
    CHECK_EQ(vidsch.completed(), 0u);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 3u);
    std::uint32_t fence = 0u;
    CHECK_EQ(vidsch.query_current_fence(fence), 0);
    CHECK_EQ(fence, 3u);
    CHECK_EQ(device.dxgkrnl().dpc_notifications(), 1u); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_preemption_is_reported_after_what_was_pending_and_again_with_nothing_pending)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0u), 0);
    std::uint8_t command[16] = {};
    CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
    CHECK_EQ(vidsch.submit(), 0);
    CHECK_EQ(vidsch.preempt(), 0);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 1u);
    CHECK_EQ(vidsch.preemptions(), 1u);
    CHECK_EQ(vidsch.preempt(), 0);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.preemptions(), 2u); }
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_render_that_overruns_is_refused_and_nothing_is_submitted)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0u), 0);
    std::uint8_t command[16] = {};
    CHECK_EQ(nt(vidsch.render(command, sizeof command, 9u, 0)), 0xC000000Du);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 0u); }
  CHECK_EQ(device.fdo().remove_device(), 0); }


TEST(ddi_scenario, a_timeout_is_recovered_from_twice)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0u), 0);
    std::uint8_t command[16] = {};
    for (int round = 0; round < 2; round++) {
        CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
        CHECK_EQ(vidsch.submit(), 0);
        CHECK_EQ(vidsch.reset_from_timeout(), 0);
        vidsch.run_dpc();
        CHECK_EQ(device.function().common().status(), 0u);
        CHECK_EQ(vidsch.restart_from_timeout(), 0);
        CHECK_EQ(device.function().common().status(), driver_ok_status);
        CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
        CHECK_EQ(vidsch.submit(), 0);
        vidsch.run_dpc();
        CHECK_EQ(vidsch.completed(), vidsch.submitted());
    }
    std::uint32_t fence = 0u;
    CHECK_EQ(vidsch.query_current_fence(fence), 0);
    CHECK_EQ(fence, 4u); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_mode_change_from_edid_to_commit)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  av::FakeDisplayModeManager dmm(driver.platform(), driver.table(), device.fdo().context());
  { std::uint8_t edid[128] = {};
    CHECK_EQ(dmm.read_edid(edid), 0);
    unsigned sum = 0u;
    for (std::uint8_t byte : edid) sum += byte;
    CHECK_EQ(sum % 256u, 0u);
    CHECK_EQ(edid[56] | (edid[58] >> 4u) << 8u, 1280u);
    av::FakeVidPn vidpn;
    CHECK_EQ(dmm.read_monitor_modes(vidpn), 0);
    CHECK_EQ(vidpn.monitor_modes().modes(), 5u);
    CHECK_EQ(nt(dmm.recommend_functional_vidpn()), 0xC01E0323u);
    CHECK_EQ(nt(dmm.recommend_vidpn_topology()), 0xC01E031Au);
    CHECK_EQ(dmm.set_mode(vidpn, 1024u, 768u), 0);
    CHECK(vidpn.source_modes(0u).pinned());
    CHECK_EQ(vidpn.path(0u).ContentTransformationScalingSupport, 1u);
    av::D3dkmdtVidPnPresentPathRecord path = vidpn.path(0u);
    path.ContentTransformationScaling = 1;
    path.ContentTransformationRotation = 1;
    CHECK_EQ(dmm.update_active_path(path), 0);
    CHECK_EQ(dmm.control_vsync(true), 0);
    av::DxgkargGetScanLineRecord scan = {0u, 0u, {}, 0u};
    CHECK_EQ(dmm.scan_line(scan), 0);
    CHECK(scan.ScanLine < 768u + 32u);
    CHECK_EQ(dmm.control_vsync(false), 0);
    CHECK_EQ(dmm.commit(vidpn, 2u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_second_path_is_refused_and_a_mode_off_the_list_is_never_offered)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  av::FakeDisplayModeManager dmm(driver.platform(), driver.table(), device.fdo().context());
  { av::FakeVidPn clone;
    clone.add_path(0u, 0u);
    clone.add_path(0u, 1u);
    bool supported = true;
    CHECK_EQ(dmm.is_supported(clone, supported), 0);
    CHECK(!supported);
    av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 1000u, 700u), 0x001E0310);
    av::DxgkargGetScanLineRecord scan = {0u, 0u, {}, 0u};
    CHECK_EQ(nt(dmm.scan_line(scan)), invalid_device_state); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_primary_scans_out_at_vsync_while_presents_complete)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  av::FakeDisplayModeManager dmm(driver.platform(), driver.table(), device.fdo().context());
  { av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 1280u, 800u), 0); }
  Runtime runtime(driver.table(), device.fdo().context(), 1280u, 800u);
  CHECK_EQ(runtime.create_primary(), 0);
  CHECK_EQ(dmm.show_primary(runtime.primary(), 0x0000000200000000ll), 0);
  CHECK_EQ(dmm.set_visible(true), 0);
  CHECK_EQ(dmm.control_vsync(true), 0);
  device.blank();
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(device.dxgkrnl().interrupt(1u).InterruptType, 3);
  CHECK_EQ(device.dxgkrnl().interrupt(1u).Argument3, 2u);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0u), 0);
    const av::RectRecord dirty[2] = {{0, 0, 16, 16}, {32, 32, 64, 64}};
    CHECK_EQ(vidsch.present_blt(0x0000000200000000ll, 0x0000000300000000ll, dirty, 2u), 0);
    CHECK_EQ(vidsch.dma()[av::PresentCommand::source_address_offset + 4u], 2u);
    CHECK_EQ(vidsch.dma()[av::PresentCommand::destination_address_offset + 4u], 3u);
    CHECK_EQ(vidsch.submit(), 0);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 1u); }
  CHECK_EQ(dmm.show_primary(nullptr, 0), 0);
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupt(device.dxgkrnl().interrupts() - 1u).Argument3, 0u);
  CHECK_EQ(dmm.control_vsync(false), 0);
  const std::uint32_t reported = device.dxgkrnl().interrupts();
  device.blank();
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupts(), reported);
  CHECK_EQ(dmm.control_vsync(true), 0);
  CHECK_EQ(device.fdo().stop_device(), 0);
  device.fdo().dpc();
  CHECK_EQ(device.dxgkrnl().interrupts(), reported);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, the_pointer_reaches_the_cursor_queue_and_the_legacy_surface_declines)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().PointerCaps, 7u);
  const std::uint32_t logged = driver.platform().logged();
  CHECK_EQ(device.fdo().dxgadapter().create_render_core(), 0);
  void *const adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  std::uint8_t arrow[16u * 16u * 4u];
  std::memset(arrow, 0x40, sizeof arrow);
  const av::DxgkargSetPointerShapeRecord shape = {2u, 16u, 16u, 64u, 0u, 0u, arrow, 1u, 1u};
  const auto set_shape = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSetPointerShapeRecord *)>(
      table.routine(DdiSlot::DxgkDdiSetPointerShape));
  const auto set_position = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSetPointerPositionRecord *)>(
      table.routine(DdiSlot::DxgkDdiSetPointerPosition));
  driver.platform().set_irql(av::Irql::passive);
  CHECK_EQ(set_shape(adapter, &shape), 0);
  for (std::int32_t step = 0; step < 3; step++) {
      const av::DxgkargSetPointerPositionRecord position = {0u, 100 + step, 100, 1u};
      CHECK_EQ(set_position(adapter, &position), 0);
  }
  const av::DxgkargSetPointerPositionRecord hidden = {0u, 0, 0, 0u};
  CHECK_EQ(set_position(adapter, &hidden), 0);
  const av::FakeCommonConfig::Queue &cursor_queue = device.function().common().queue(1u);
  av::FakeVirtqueueDevice cursor(driver.platform(), cursor_queue.desc, cursor_queue.driver, cursor_queue.device,
                                 cursor_queue.size);
  CHECK_EQ(cursor.respond_all(0x1100u), 4u);
  CHECK_EQ(cursor.request_type(0u), 0x0300u);
  CHECK_EQ(cursor.request_type(1u), 0x0301u);
  CHECK_EQ(cursor.request_type(2u), 0x0301u);
  CHECK_EQ(cursor.request_type(3u), 0x0300u);
  av::FakeVirtqueueDevice &control = device.control();
  CHECK_EQ(control.respond_all(0x1100u), 3u);
  CHECK_EQ(device.fdo().interrupt(2u), 1u);
  CHECK_EQ(driver.platform().logged(), logged);
  int request = 0;
  const DdiSlot inout_slots[] = {DdiSlot::DxgkDdiAcquireSwizzlingRange, DdiSlot::DxgkDdiCreateOverlay};
  for (DdiSlot slot : inout_slots)
      CHECK_EQ(nt(reinterpret_cast<std::int32_t (*)(void *, void *)>(table.routine(slot))(adapter, &request)),
               not_supported);
  const DdiSlot in_slots[] = {DdiSlot::DxgkDdiReleaseSwizzlingRange, DdiSlot::DxgkDdiSetPalette,
                              DdiSlot::DxgkDdiStopCapture};
  for (DdiSlot slot : in_slots)
      CHECK_EQ(nt(reinterpret_cast<std::int32_t (*)(void *, const void *)>(table.routine(slot))(adapter, &request)),
               not_supported);
  const DdiSlot overlay_slots[] = {DdiSlot::DxgkDdiUpdateOverlay, DdiSlot::DxgkDdiFlipOverlay};
  for (DdiSlot slot : overlay_slots)
      CHECK_EQ(nt(reinterpret_cast<std::int32_t (*)(void *, const void *)>(table.routine(slot))(nullptr, &request)),
               not_supported);
  CHECK_EQ(nt(reinterpret_cast<std::int32_t (*)(void *)>(table.routine(DdiSlot::DxgkDdiDestroyOverlay))(nullptr)),
           not_supported);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, the_committed_path_reports_no_driver_transformation_and_the_placeholders_stay_empty)
{ LoadedDriver driver;
  FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(&driver.table()), Status::ok);
  const DdiSlot skipped[] = {DdiSlot::DxgkDdiDescribePageTable, DdiSlot::DxgkDdiUpdatePageTable,
                             DdiSlot::DxgkDdiUpdatePageDirectory, DdiSlot::DxgkDdiMovePageDirectory,
                             DdiSlot::DxgkDdiSubmitRender, DdiSlot::DxgkDdiCreateAllocation2};
  for (DdiSlot slot : skipped) CHECK(driver.table().routine(slot) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  av::FakeDisplayModeManager dmm(driver.platform(), driver.table(), device.fdo().context());
  { av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 800u, 600u), 0);
    av::DxgkargQueryVidPnHwCapabilityRecord query = {&vidpn, 0u, 0u, 0xFFu, 0u};
    const auto routine = reinterpret_cast<std::int32_t (*)(void *, av::DxgkargQueryVidPnHwCapabilityRecord *)>(
        driver.table().routine(DdiSlot::DxgkDdiQueryVidPnHWCapability));
    CHECK_EQ(routine(device.fdo().context(), &query), 0);
    CHECK_EQ(query.VidPnHWCaps, 0u); }
  CHECK_EQ(device.fdo().dxgadapter().create_render_core(), 0);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, an_engine_reset_then_the_device_disappears)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(device.fdo().dxgadapter().create_render_core(), 0);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().SupportPerEngineTDR, 1u);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0u), 0);
    std::uint8_t command[16] = {};
    CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
    CHECK_EQ(vidsch.submit(), 0);
    av::DxgkargResetEngineRecord reset = {0u, 0u, 0u};
    const auto reset_engine = reinterpret_cast<std::int32_t (*)(void *, av::DxgkargResetEngineRecord *)>(
        driver.table().routine(DdiSlot::DxgkDdiResetEngine));
    CHECK_EQ(reset_engine(device.fdo().context(), &reset), 0);
    CHECK_EQ(reset.LastAbortedFenceId, 1u);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 0u);
    CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
    CHECK_EQ(vidsch.submit(), 0);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), vidsch.submitted()); }
  device.function().common().delay_reset(100000u);
  CHECK_EQ(device.fdo().surprise_removal(), 0);
  CHECK_EQ(device.fdo().interrupt(1u), 0u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, node_metadata_clock_calibration_and_vsync_state_through_the_table)
{ LoadedDriver driver;
  CHECK(driver.table().routine(DdiSlot::DxgkDdiCheckMultiPlaneOverlaySupport) == nullptr);
  CHECK(driver.table().routine(DdiSlot::DxgkDdiFormatHistoryBuffer) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  void *const adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  av::DxgkNodeMetadataRecord metadata = {};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, std::uint32_t, av::DxgkNodeMetadataRecord *)>(
               table.routine(DdiSlot::DxgkDdiGetNodeMetadata))(adapter, 0u, &metadata), 0);
  CHECK_EQ(metadata.FriendlyName[0], static_cast<unsigned>('3'));
  av::DxgkGpuClockDataRecord clock = {};
  driver.platform().set_irql(av::Irql::dispatch);
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, std::uint32_t, std::uint32_t, av::DxgkGpuClockDataRecord *)>(
               table.routine(DdiSlot::DxgkDdiCalibrateGpuClock))(adapter, 0u, 0u, &clock), 0);
  driver.platform().set_irql(av::Irql::passive);
  CHECK(clock.GpuFrequency != 0u);
  const auto control = reinterpret_cast<std::int32_t (*)(void *, av::DxgkargControlInterrupt2Record)>(
      table.routine(DdiSlot::DxgkDdiControlInterrupt2));
  CHECK_EQ(control(adapter, {3, 0}), 0);
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(control(adapter, {3, 1}), 0);
  device.blank();
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_gpu_mmu_adapter_pages_submits_by_virtual_address_and_flips_at_device_irql)
{ LoadedDriver driver;
  CHECK(driver.table().routine(DdiSlot::DxgkDdiRenderGdi) == nullptr);
  CHECK(driver.table().routine(DdiSlot::DxgkDdiMapCpuHostAperture) == nullptr);
  CHECK(driver.table().routine(DdiSlot::DxgkDdiSetVideoProtectedRegion) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  void *const adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  CHECK_EQ(device.fdo().dxgadapter().create_render_core(), 0);
  av::FakeGpuMmu mmu(device.fdo().dxgadapter());
  CHECK_EQ(mmu.query(adapter), 0);
  CHECK(mmu.supported());
  av::FakeVidMm vidmm(device.fdo().dxgadapter());
  CHECK_EQ(vidmm.create_segments(adapter), 0);
  CHECK_EQ(vidmm.segment_count(), 2u);
  CHECK_EQ(mmu.initialize_va_state(vidmm.segment_count()), 0);
  CHECK(mmu.failed_rule() == av::GpuMmuRule::none);

  av::DxgkargGetRootPageTableSizeRecord size = {512u, 0u};
  CHECK_EQ(reinterpret_cast<std::size_t (*)(void *, av::DxgkargGetRootPageTableSizeRecord *)>(
               table.routine(DdiSlot::DxgkDdiGetRootPageTableSize))(adapter, &size), 4096u);
  const auto build = reinterpret_cast<std::int32_t (*)(void *, av::DxgkargBuildPagingBufferRecord *)>(
      table.routine(DdiSlot::DxgkDdiBuildPagingBuffer));
  std::uint8_t page_table[4096] = {};
  const av::DxgkPteRecord entry = {0x1u | (2u << 5u), 0x12345u};
  av::DxgkBuildPagingBufferUpdatePageTableRecord update = {};
  update.PageTableAddress = page_table;
  update.pPageTableEntries = &entry;
  update.StartIndex = 16u;
  update.NumPageTableEntries = 1u;
  std::uint8_t paging[64] = {};
  av::DxgkargBuildPagingBufferRecord request = {};
  request.pDmaBuffer = paging;
  request.DmaSize = sizeof paging;
  request.Operation = 11;
  std::memcpy(request.Reserved, &update, sizeof update);
  CHECK_EQ(build(adapter, &request), 0);
  CHECK_EQ(page_table[128], 0x21u);
  CHECK_EQ(page_table[129], 0x50u);
  const av::DxgkBuildPagingBufferFlushTlbRecord flush = {2u, 0u, 0x7000u, nullptr, 0x10000u, 0x11000u};
  request.Operation = 12;
  std::memcpy(request.Reserved, &flush, sizeof flush);
  CHECK_EQ(build(adapter, &request), 0);
  CHECK_EQ(paging[0], 0x41u);
  CHECK_EQ(paging[4], 1u);
  CHECK(request.pDmaBuffer == paging + 40);

  { av::FakeVidSch vidsch(driver.platform(), table, device.dxgkrnl(), adapter);
    CHECK_EQ(vidsch.create_context(0x4u), 0);
    vidsch.set_root_page_table(0x7000u, 512u);
    CHECK_EQ(vidsch.submit_virtual(0x10000u), 0);
    CHECK_EQ(vidsch.submit_virtual(0x1000000000000ull), static_cast<std::int32_t>(0xC000000Du));
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 1u); }

  av::FakeDisplayModeManager dmm(driver.platform(), table, adapter);
  { av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 1280u, 800u), 0); }
  int primary = 0;
  CHECK_EQ(dmm.set_visible(true), 0);
  CHECK_EQ(dmm.control_vsync(true), 0);
  driver.platform().set_irql(av::Irql::isr);
  CHECK_EQ(dmm.show_primary(&primary, 0x0000000400000000ll), 0);
  driver.platform().set_irql(av::Irql::passive);
  const std::uint32_t reported = device.dxgkrnl().interrupts();
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupts(), reported + 1u);
  CHECK_EQ(device.dxgkrnl().interrupt(reported).Argument3, 4u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, start_declines_hdr_priority_and_the_monitor_link_is_plain)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  CHECK(table.routine(DdiSlot::DxgkDdiCheckMultiPlaneOverlaySupport3) != nullptr);
  CHECK(table.routine(DdiSlot::DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay3) != nullptr);
  CHECK(table.routine(DdiSlot::DxgkDdiPostMultiPlaneOverlayPresent) == nullptr);
  CHECK(table.routine(DdiSlot::DxgkDdiValidateUpdateAllocationProperty) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(device.fdo().dxgadapter().mode_behavior().Request, 0x1u);
  CHECK_EQ(device.fdo().dxgadapter().mode_behavior().Satisfied, 0u);
  CHECK_EQ(device.fdo().dxgadapter().mode_behavior().NotSatisfied, 0x1u);
  CHECK_EQ(device.fdo().enumerate_children(), 0);
  av::FakeDxgmonitor monitor(driver.platform(), table, device.fdo().context(), 2100u);
  CHECK_EQ(monitor.update_link_info(0u, 0u), 0);
  CHECK_EQ(monitor.capabilities(), 0u);
  CHECK_EQ(monitor.dithering_support(), 0u);
  CHECK_EQ(monitor.update_link_info(0u, 1u), 0);
  CHECK_EQ(monitor.usage_hints(), 0x3u);
  av::FakeDxgmonitor stricter(driver.platform(), table, device.fdo().context(), 2400u);
  CHECK_EQ(stricter.update_link_info(0u, 0u), 0);
  CHECK_EQ(stricter.update_link_info(1u, 0u), static_cast<std::int32_t>(0xC000000Du));
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, hardware_queues_submit_complete_through_progress_fences_and_reset)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  const DdiSlot skipped[] = {
      DdiSlot::DxgkDdiCreateHwContext, DdiSlot::DxgkDdiDestroyHwContext, DdiSlot::DxgkDdiSwitchToHwContextList,
      DdiSlot::DxgkDdiCreatePeriodicFrameNotification, DdiSlot::DxgkDdiDestroyPeriodicFrameNotification,
      DdiSlot::DxgkDdiSetTimingsFromVidPn, DdiSlot::DxgkDdiSetTargetGamma, DdiSlot::DxgkDdiSetTargetContentType,
      DdiSlot::DxgkDdiSetTargetAnalogCopyProtection, DdiSlot::DxgkDdiSetTargetAdjustedColorimetry,
      DdiSlot::DxgkDdiDisplayDetectControl, DdiSlot::DxgkDdiQueryConnectionChange,
      DdiSlot::DxgkDdiExchangePreStartInfo,
  };
  for (const DdiSlot slot : skipped) CHECK(table.routine(slot) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(device.fdo().dxgadapter().display_caps_extension(), 0u);
  void *const adapter = device.fdo().context();
  { av::FakeVidSch vidsch(driver.platform(), table, device.dxgkrnl(), adapter);
    CHECK_EQ(vidsch.create_context(0x14u), 0);
    av::FakeHwQueue queue(driver.platform(), table, adapter, vidsch.context());
    CHECK_EQ(queue.create(0x1u), 0);
    for (int packet = 0; packet < 3; packet++) CHECK_EQ(queue.submit(0x10000u, 0u), 0);
    CHECK_EQ(queue.completed(), 0u);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.fence_signals(), 1u);
    queue.observe_progress();
    CHECK_EQ(queue.completed(), 3u);
    CHECK_EQ(queue.submit(0x20000u, 0x1u), 0);
    const av::DxgkargResetHwEngineRecord reset = {0u, 0u};
    CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargResetHwEngineRecord *)>(
                 table.routine(DdiSlot::DxgkDdiResetHwEngine))(adapter, &reset), 0);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.fence_signals(), 1u);
    CHECK_EQ(queue.submit(0x10000u, 0u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_wddm_2_3_adapter_starts_and_scans_out_with_the_later_slots_null)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  const DdiSlot skipped[] = {
      DdiSlot::DxgkDdiUpdateHwContextState, DdiSlot::DxgkDdiCreateProtectedSession,
      DdiSlot::DxgkDdiDestroyProtectedSession, DdiSlot::DxgkDdiSignalMonitoredFence,
      DdiSlot::DxgkDdiPresentToHwQueue, DdiSlot::DxgkDdiValidateSubmitCommand,
      DdiSlot::DxgkDdiSetTargetAdjustedColorimetry2, DdiSlot::DxgkDdiSetTrackedWorkloadPowerLevel,
      DdiSlot::DxgkDdiSaveMemoryForHotUpdate, DdiSlot::DxgkDdiRestoreMemoryForHotUpdate,
      DdiSlot::DxgkDdiCollectDiagnosticInfo, DdiSlot::DxgkDdiControlInterrupt3,
  };
  for (const DdiSlot slot : skipped) CHECK(table.routine(slot) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x3200);
  CHECK_EQ(device.fdo().dxgadapter().create_render_core(), 0);
  av::FakeDisplayModeManager dmm(driver.platform(), table, device.fdo().context());
  { av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 1024u, 768u), 0); }
  int primary = 0;
  CHECK_EQ(dmm.show_primary(&primary, 0x0000000200000000ll), 0);
  CHECK_EQ(dmm.set_visible(true), 0);
  CHECK_EQ(dmm.control_vsync(true), 0);
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupt(device.dxgkrnl().interrupts() - 1u).Argument3, 2u);
  CHECK_EQ(dmm.set_visible(false), 0);
  CHECK_EQ(dmm.show_primary(nullptr, 0), 0);
  CHECK_EQ(dmm.control_vsync(false), 0);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_system_memory_primary_flips_immediately_and_gpu_va_presents_complete)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  const DdiSlot skipped[] = {
      DdiSlot::DxgkDdiSetSchedulingLogBuffer, DdiSlot::DxgkDdiSetupPriorityBands, DdiSlot::DxgkDdiNotifyFocusPresent,
      DdiSlot::DxgkDdiSetContextSchedulingProperties, DdiSlot::DxgkDdiSuspendContext, DdiSlot::DxgkDdiResumeContext,
      DdiSlot::DxgkDdiSetVirtualMachineData, DdiSlot::DxgkDdiBeginExclusiveAccess,
      DdiSlot::DxgkDdiEndExclusiveAccess, DdiSlot::DxgkDdiQueryDiagnosticTypesSupport,
      DdiSlot::DxgkDdiControlDiagnosticReporting,
  };
  for (const DdiSlot slot : skipped) CHECK(table.routine(slot) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x3200);
  void *const adapter = device.fdo().context();
  av::FakeDisplayModeManager dmm(driver.platform(), table, adapter);
  { av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 1280u, 800u), 0); }
  CHECK_EQ(dmm.set_visible(true), 0);
  CHECK_EQ(dmm.control_vsync(true), 0);
  int first = 0, second = 0;
  CHECK_EQ(dmm.flip(&first, 0u, 0, 0x4u), 0);
  const std::uint32_t before = device.dxgkrnl().interrupts();
  CHECK_EQ(dmm.flip(&second, 0u, 0, 0x2u), 0);
  CHECK_EQ(device.dxgkrnl().interrupts(), before);
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupt(before).InterruptType, 3);
  CHECK_EQ(device.dxgkrnl().interrupt(before).Argument2, 0u);
  CHECK_EQ(device.dxgkrnl().interrupt(before).Argument3, 0u);
  CHECK_EQ(dmm.flip(&first, 2u, 0, 0x2u), static_cast<std::int32_t>(0xC000000Du));
  CHECK_EQ(dmm.control_vsync(false), 0);
  { av::FakeVidSch vidsch(driver.platform(), table, device.dxgkrnl(), adapter);
    CHECK_EQ(vidsch.create_context(0x4u), 0);
    const av::RectRecord dirty[1] = {{0, 0, 32, 32}};
    CHECK_EQ(vidsch.present_blt_virtual(0x0000000100000000ull, 0x0000000200000000ull, dirty, 1u), 0);
    CHECK_EQ(vidsch.dma()[av::PresentCommand::source_address_offset + 4u], 1u);
    CHECK_EQ(vidsch.dma()[av::PresentCommand::destination_address_offset + 4u], 2u);
    CHECK_EQ(vidsch.dma()[68], 1u);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 1u); }
  const av::DxgkargResetHwEngineRecord reset = {0u, 0u};
  const av::DxgkargResumeHwEngineRecord resume = {0u, 0u};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargResetHwEngineRecord *)>(
               table.routine(DdiSlot::DxgkDdiResetHwEngine))(adapter, &reset), 0);
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargResumeHwEngineRecord *)>(
               table.routine(DdiSlot::DxgkDdiResumeHwEngine))(adapter, &resume), 0);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_wddm_2_9_adapter_flips_without_a_hardware_flip_queue)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  const DdiSlot skipped[] = {DdiSlot::DxgkDdiSetFlipQueueLogBuffer, DdiSlot::DxgkDdiUpdateFlipQueueLog,
                             DdiSlot::DxgkDdiCancelQueuedFlips, DdiSlot::DxgkDdiSetInterruptTargetPresentId};
  for (const DdiSlot slot : skipped) CHECK(table.routine(slot) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x3200);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().MaxHwQueuedFlips, 0u);
  av::FakeDisplayModeManager dmm(driver.platform(), table, device.fdo().context());
  { av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 1280u, 800u), 0); }
  CHECK_EQ(dmm.set_visible(true), 0);
  CHECK_EQ(dmm.control_vsync(true), 0);
  int primary = 0;
  CHECK_EQ(dmm.flip(&primary, 0u, 0, 0x2u), 0);
  const std::uint32_t before = device.dxgkrnl().interrupts();
  device.blank();
  CHECK_EQ(device.dxgkrnl().interrupts(), before + 1u);
  CHECK_EQ(dmm.control_vsync(false), 0);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_wddm_3_0_adapter_allocates_a_primary_with_the_later_slots_null)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  const DdiSlot skipped[] = {DdiSlot::DxgkDdiSetAllocationBackingStore, DdiSlot::DxgkDdiCreateCpuEvent,
                             DdiSlot::DxgkDdiDestroyCpuEvent, DdiSlot::DxgkDdiCancelFlips};
  for (const DdiSlot slot : skipped) CHECK(table.routine(slot) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x3200);
  { Runtime runtime(table, device.fdo().context());
    CHECK_EQ(runtime.create_primary(), 0);
    CHECK_EQ(runtime.flags2(), 0u);
    CHECK_EQ(runtime.destroy_primary(), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, the_doorbell_ddis_refuse_a_queue_whose_context_holds_no_page)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  Device          device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), table, device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0x14u), 0);
    av::FakeHwQueue queue(driver.platform(), table, device.fdo().context(), vidsch.context());
    CHECK_EQ(queue.create(0u), 0);

    const auto create = reinterpret_cast<std::int32_t (*)(av::DxgkargCreateDoorbellRecord *)>(
        table.routine(DdiSlot::DxgkDdiCreateDoorbell));
    av::DxgkargCreateDoorbellRecord made = {};
    made.hHwQueue = queue.handle();
    CHECK(create(&made) != 0);
    CHECK(made.hDoorbell == nullptr);
    made.PrivateDriverDataSize = 4u;
    CHECK(create(&made) != 0);
    made.PrivateDriverDataSize = 0u;
    made.Flags = 1u;
    CHECK(create(&made) != 0);

    std::uint64_t not_a_doorbell = 0u;
    av::DxgkargConnectDoorbellRecord connect = {};
    connect.hDoorbell = &not_a_doorbell;
    av::DxgkargDisconnectDoorbellRecord disconnect = {};
    disconnect.hDoorbell = &not_a_doorbell;
    av::DxgkargDestroyDoorbellRecord destroy = {};
    destroy.hDoorbell = &not_a_doorbell;
    av::Adapter &adapter = *static_cast<av::Adapter *>(device.fdo().context());
    CHECK(adapter.connect_doorbell(&connect) != 0);
    CHECK(adapter.disconnect_doorbell(&disconnect) != 0);
    CHECK(adapter.destroy_doorbell(&destroy) != 0);
    CHECK(adapter.create_doorbell(nullptr) != 0);
    CHECK(adapter.connect_doorbell(nullptr) != 0);
    CHECK(adapter.disconnect_doorbell(nullptr) != 0);
    CHECK(adapter.destroy_doorbell(nullptr) != 0);
    CHECK(adapter.notify_work_submission(nullptr) != 0);

    const auto notify = reinterpret_cast<std::int32_t (*)(const av::DxgkargNotifyWorkSubmissionRecord *)>(
        table.routine(DdiSlot::DxgkDdiNotifyWorkSubmission));
    av::DxgkargNotifyWorkSubmissionRecord submitted = {};
    submitted.hHwQueue = queue.handle();
    CHECK_EQ(notify(&submitted), 0);
    CHECK_EQ(queue.destroy(), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_wddm_3_1_adapter_starts_with_its_doorbells_and_its_native_fences)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  const DdiSlot fences[] = {
      DdiSlot::DxgkDdiCreateNativeFence, DdiSlot::DxgkDdiDestroyNativeFence, DdiSlot::DxgkDdiUpdateMonitoredValues,
      DdiSlot::DxgkDdiUpdateCurrentValuesFromCpu,
  };
  for (const DdiSlot slot : fences) CHECK(table.routine(slot) != nullptr);
  const DdiSlot skipped[] = {
      DdiSlot::DxgkDdiOpenNativeFence, DdiSlot::DxgkDdiCloseNativeFence,
      DdiSlot::DxgkDdiSetNativeFenceLogBuffer, DdiSlot::DxgkDdiUpdateNativeFenceLogs,
  };
  for (const DdiSlot slot : skipped) CHECK(table.routine(slot) == nullptr);
  const DdiSlot doorbells[] = {
      DdiSlot::DxgkDdiCreateDoorbell, DdiSlot::DxgkDdiConnectDoorbell, DdiSlot::DxgkDdiDisconnectDoorbell,
      DdiSlot::DxgkDdiDestroyDoorbell, DdiSlot::DxgkDdiNotifyWorkSubmission,
  };
  for (const DdiSlot slot : doorbells) CHECK(table.routine(slot) != nullptr);
  Device device(driver, 0u);
  device.dxgkrnl().allow_feature(av::feature_native_fence);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x3200);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().SchedulingCaps & 0x800u, 0x800u);
  { av::FakeVidSch vidsch(driver.platform(), table, device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(0x14u), 0);
    av::FakeHwQueue queue(driver.platform(), table, device.fdo().context(), vidsch.context());
    CHECK_EQ(queue.create(0u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, a_wddm_3_2_display_adapter_starts_with_the_3_2_slots_null)
{ LoadedDriver driver;
  const DdiTable &table = driver.table();
  for (std::uint32_t slot = static_cast<std::uint32_t>(DdiSlot::DxgkDdiCreateMemoryBasis);
       slot <= static_cast<std::uint32_t>(DdiSlot::DxgkDdiResetDisplayEngine); slot++)
      CHECK(table.routine(static_cast<DdiSlot>(slot)) == nullptr);
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
  CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x3200);
  std::uint32_t dirty_bits = 0u;
  CHECK_EQ(nt(device.fdo().dxgadapter().query(device.fdo().context(), 39, nullptr, 0u, &dirty_bits, sizeof dirty_bits)),
           0xC0000002u);
  CHECK_EQ(device.fdo().enumerate_children(), 0);
  av::FakeDxgmonitor monitor(driver.platform(), table, device.fdo().context(), 3200u);
  CHECK_EQ(monitor.update_link_info(0u, 0u), 0);
  av::FakeDisplayModeManager dmm(driver.platform(), table, device.fdo().context());
  { av::FakeVidPn vidpn;
    CHECK_EQ(dmm.set_mode(vidpn, 1280u, 800u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }


TEST(ddi_scenario, two_displays_get_a_source_a_child_and_a_scanout_each)
{ LoadedDriver driver;
  Device device(driver, 0u);
  device.function().device_config().set_u32(8u, 2u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK_EQ(device.fdo().sources(), 2u);
  CHECK_EQ(device.fdo().children(), 2u);
  void           *adapter = device.fdo().context();
  const DdiTable &table = driver.table();
  av::DxgkChildDescriptorRecord relations[3] = {};
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, av::DxgkChildDescriptorRecord *, std::uint32_t)>(
               table.routine(DdiSlot::DxgkDdiQueryChildRelations))(adapter, relations, sizeof relations),
           0);
  CHECK_EQ(relations[1].ChildUid, 1u);
  av::FakeVirtqueueDevice &control = device.control();
  for (std::uint32_t source = 0u; source < 2u; source++) {
      av::FakeVidPn vidpn;
      vidpn.add_path(source, source);
      av::D3dkmdtVidPnSourceModeRecord pinned = {};
      av::DisplayMode(1280u, 800u).describe(pinned, av::DisplayMode::source_formats[0]);
      vidpn.source_modes(source).seed(pinned, true);
      const av::DxgkargCommitVidPnRecord commit = {&vidpn, source, 0, nullptr, 0u, 0u};
      CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargCommitVidPnRecord *)>(
                   table.routine(DdiSlot::DxgkDdiCommitVidPn))(adapter, &commit),
               0);
      const av::DxgkargSetVidPnSourceVisibilityRecord visible = {source, 1u, {}};
      CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargSetVidPnSourceVisibilityRecord *)>(
                   table.routine(DdiSlot::DxgkDdiSetVidPnSourceVisibility))(adapter, &visible),
               0);
  }
  CHECK_EQ(control.respond_all(0x1100u), 2u);
  CHECK_EQ(control.request_type(5u), 0x0101u);
  CHECK_EQ(u32(control.request(5u), 24u), av::ScanoutResource::first_resource_id);
  CHECK_EQ(control.request_type(6u), 0x0101u);
  CHECK_EQ(u32(control.request(6u), 24u), av::ScanoutResource::first_resource_id + 1u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_device_with_host_memory_reports_the_aperture_alone)
{ LoadedDriver driver;
  Device device(driver, 0u);
  device.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(!device.created_blob());
  CHECK(!device.mapped_region());
  void           *adapter = device.fdo().context();
  const DdiTable &table = driver.table();

  std::uint32_t physical_adapter = 0u;
  av::DxgkQuerySegmentOut4Record out = {};
  av::DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 11;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &out;
  query.OutputDataSize = sizeof out;
  const auto ask = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargQueryAdapterInfoRecord *)>(
      table.routine(DdiSlot::DxgkDdiQueryAdapterInfo));
  CHECK_EQ(ask(adapter, &query), 0);
  CHECK_EQ(out.NbSegment, 1u);

  Runtime runtime(table, adapter);
  CHECK_EQ(runtime.create_primary(), 0);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_device_without_host_memory_is_the_adapter_as_it_was)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  CHECK(!device.created_blob());
  CHECK(!device.mapped_region());
  std::uint32_t physical_adapter = 0u;
  av::DxgkQuerySegmentOut4Record out = {};
  av::DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 11;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &out;
  query.OutputDataSize = sizeof out;
  CHECK_EQ(reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargQueryAdapterInfoRecord *)>(
               driver.table().routine(DdiSlot::DxgkDdiQueryAdapterInfo))(device.fdo().context(), &query),
           0);
  CHECK_EQ(out.NbSegment, 1u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_create_that_asks_for_a_resource_is_answered_one_and_gives_it_back)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.start_device(), 0);
  { SharedRuntime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create(SharedRuntime::bytes), 0);
    CHECK(runtime.resource() != nullptr);
    CHECK_EQ(runtime.create_device(0u), 0);
    CHECK_EQ(runtime.creator_open(0u, SharedRuntime::bytes), 0);
    CHECK(runtime.minted().assigned());
    CHECK_EQ(runtime.close(0u), 0);
    CHECK_EQ(runtime.destroy(), 0);
    CHECK(runtime.resource() == nullptr);
    CHECK_EQ(runtime.destroy_device(0u), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), driver.loaded_allocations());
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

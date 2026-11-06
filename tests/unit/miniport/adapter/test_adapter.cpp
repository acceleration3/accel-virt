#include "miniport/adapter/adapter.hpp"
#include "miniport/adapter/debug_report.hpp"
#include "miniport/display/display_info_response.hpp"
#include "miniport/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_child_container_id_record.hpp"
#include "miniport/ddi/dxgk_child_descriptor_record.hpp"
#include "miniport/ddi/dxgk_child_status_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "miniport/ddi/dxgk_present_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_device_descriptor_record.hpp"
#include "miniport/ddi/dxgk_display_information_record.hpp"
#include "miniport/ddi/dxgk_driver_caps_record.hpp"
#include "miniport/ddi/dxgk_gpu_clock_data_record.hpp"
#include "miniport/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "miniport/ddi/dxgk_node_metadata_record.hpp"
#include "miniport/ddi/dxgkarg_native_fence_records.hpp"
#include "miniport/ddi/dxgkddi_feature_interface_record.hpp"
#include "miniport/ddi/dxgkargcb_query_feature_support_record.hpp"
#include "miniport/scheduling/hw_queue_contexts.hpp"
#include "miniport/scheduling/hw_queue_submissions.hpp"
#include "miniport/ddi/dxgk_open_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_page_table_level_desc_record.hpp"
#include "miniport/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "miniport/ddi/dxgk_query_page_table_level_desc_in_record.hpp"
#include "miniport/ddi/dxgk_query_segment_out4_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "miniport/ddi/dxgkarg_cancel_command_record.hpp"
#include "miniport/ddi/dxgkarg_close_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "miniport/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "miniport/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_create_context_record.hpp"
#include "miniport/ddi/dxgkarg_create_device_record.hpp"
#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "miniport/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "miniport/ddi/dxgkarg_create_process_record.hpp"
#include "miniport/ddi/dxgkarg_describe_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "miniport/ddi/dxgkarg_escape_record.hpp"
#include "miniport/av_context/av_context_binding.hpp"
#include "proto/escape/host_window_record.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/escape/pages_read_record.hpp"
#include "proto/escape/trace_escape_record.hpp"
#include "miniport/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "miniport/ddi/dxgkarg_get_scan_line_record.hpp"
#include "miniport/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "miniport/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "miniport/ddi/dxgkarg_open_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_patch_record.hpp"
#include "miniport/ddi/dxgkarg_preempt_command_record.hpp"
#include "miniport/ddi/dxgkarg_present_record.hpp"
#include "device/control/control_request.hpp"
#include "device/control/present_commands.hpp"
#include "device/control/present_engine_commands.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/present/present_engine.hpp"
#include "device/present/present_frames.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/ddi/dxgkarg_query_current_fence_record.hpp"
#include "miniport/ddi/dxgkarg_query_dependent_engine_group_record.hpp"
#include "miniport/ddi/dxgkarg_query_engine_status_record.hpp"
#include "miniport/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
#include "miniport/ddi/dxgkarg_recommend_functional_vidpn_record.hpp"
#include "miniport/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "miniport/ddi/dxgkarg_recommend_vidpn_topology_record.hpp"
#include "core/platform/log_budget.hpp"
#include "miniport/ddi/dxgk_renderkm_command_record.hpp"
#include "core/trace/state_writer.hpp"
#include "miniport/ddi/dxgkarg_render_record.hpp"
#include "miniport/ddi/dxgkarg_reset_engine_record.hpp"
#include "miniport/ddi/dxgkarg_reset_hw_engine_record.hpp"
#include "miniport/ddi/dxgkarg_resume_hw_engine_record.hpp"
#include "miniport/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "miniport/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "miniport/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "miniport/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "miniport/ddi/dxgkarg_set_stable_power_state_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "harness/virtio/fake_display_clock.hpp"
#include "proto/virtio_gpu/page_request.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_address_record.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_address_with_multi_plane_overlay_record.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_address_with_multi_plane_overlay3_record.hpp"
#include "miniport/ddi/dxgkarg_check_multiplane_overlay_support3_record.hpp"
#include "miniport/ddi/dxgkarg_multiplane_overlay_caps_records.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/ddi/dxgkarg_set_vidpn_source_visibility_record.hpp"
#include "miniport/ddi/dxgkarg_submit_command_record.hpp"
#include "miniport/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "miniport/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "miniport/scheduling/execution_node.hpp"
#include "proto/escape/follow_record.hpp"
#include "proto/escape/release_record.hpp"
#include "proto/escape/next_point_record.hpp"
#include "miniport/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "miniport/ddi/linked_device_record.hpp"
#include "miniport/display/display_mode.hpp"
#include "miniport/scheduling/context.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/dma/present_command.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/escape/umd_adapter_info.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/escape/present_point_record.hpp"
#include "miniport/present/present_ticket_record.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/fake_vidpn.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/escape/wake_event_record.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/escape/tail_publish_record.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <utility>
#include <cstring>
#include <time.h>

namespace av {

struct AdapterProbe {
    explicit AdapterProbe(Adapter &target) : adapter(target) {}
    void set_started(bool started) { adapter.started_ = started; }
    void stop_device_object() { static_cast<void>(adapter.device_.object().stop()); }
    bool window_offered() const { return adapter.window_.offered(); }
    std::uint64_t scanout(std::uint32_t source) { return adapter.sources_.scanout_address(source); }
    void         *primary(std::uint32_t source) { return adapter.sources_.primary(source); }
    OverlayPlane  plane(std::uint32_t source, std::uint32_t layer)
    {
        OverlayPlane planes[OverlayPlane::max_planes];
        adapter.sources_.planes(source, planes);
        return planes[layer];
    }
    Adapter &adapter;
};

}

using av::Adapter;
using av::AdapterProbe;
using av::DxgkChildDescriptorRecord;
using av::DxgkChildStatusRecord;
using av::DxgkDeviceDescriptorRecord;
using av::DxgkDriverCapsRecord;
using av::DxgkargCollectDbgInfoExtRecord;
using av::DxgkargCollectDbgInfoRecord;
using av::DxgkargEscapeRecord;
using av::DxgkargQueryAdapterInfoRecord;
using av::GuardedBuffer;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::Irql;
using av::LinkedDeviceRecord;
using av::MockPlatform;
using av::Violation;

namespace {

constexpr std::uint32_t success = 0u;
constexpr std::uint32_t invalid_parameter = 0xC000000Du;
constexpr std::uint32_t no_memory = 0xC0000017u;
constexpr std::uint32_t not_found = 0xC0000225u;
constexpr std::uint32_t no_space = 0xC000009Au;
constexpr std::uint32_t not_supported = 0xC00000BBu;
constexpr std::uint32_t invalid_device_state = 0xC0000184u;
constexpr std::uint32_t buffer_too_small = 0xC0000023u;
constexpr std::uint32_t no_more_descriptor_data = 0xC01D0008u;
constexpr std::uint32_t not_implemented = 0xC0000002u;
constexpr std::int32_t  query_umd_private = 0;
constexpr std::int32_t  query_driver_caps = 1;
constexpr std::int32_t  query_wddm_device_caps = 29;
constexpr std::int32_t  last_query_type = 48;

constexpr std::uint32_t adapter_uid = 0xFFFFFFFFu;
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;
constexpr std::uint8_t  driver_ok_status = 0x0Fu;
constexpr std::uint8_t  failed_bit = 0x80u;
constexpr std::uint64_t display_period = 10000000u / 60u;

std::uint32_t nt(std::int32_t status) { return static_cast<std::uint32_t>(status); }

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

class DisplayInfoAnswer {
public:
    DisplayInfoAnswer() : bytes_{} { describe(0u, 1280u, 800u); }

    void describe(std::uint32_t scanout, std::uint32_t width, std::uint32_t height)
    {
        write(0u, av::DisplayInfoResponse::response_type);
        const std::size_t at = av::DisplayInfoResponse::header_size +
                               scanout * av::DisplayInfoResponse::scanout_size;
        write(at + 8u, width);
        write(at + 12u, height);
        write(at + 16u, 1u);
    }

    void disable(std::uint32_t scanout)
    {
        write(av::DisplayInfoResponse::header_size + scanout * av::DisplayInfoResponse::scanout_size + 16u, 0u);
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
constexpr std::uint32_t unmap_blob = 0x0209u;
constexpr std::uint32_t get_capset_info = 0x0108u;
constexpr std::uint32_t get_capset = 0x0109u;
constexpr std::uint32_t resource_unref = 0x0102u;
constexpr std::uint32_t ctx_create = 0x0200u;
constexpr std::uint32_t ctx_destroy = 0x0201u;
constexpr std::uint32_t submit_3d = 0x0207u;
constexpr std::uint64_t host_region_address = 0x4000000000ull;
constexpr std::uint64_t host_region_bytes = 16u << 20u;
constexpr std::uint8_t  first_gpu[av::GpuIdentity::uuid_bytes] = {3};
constexpr std::uint8_t  other_gpu[av::GpuIdentity::uuid_bytes] = {4};

av::CapsetInfo host_capset(const std::uint8_t *uuid, std::uint32_t sub_apis = 1u,
                           std::uint64_t features = av::CapsetInfo::feature_host_visible_memory,
                           std::uint32_t device_type = av::GpuIdentity::discrete_gpu)
{
    return av::CapsetInfo(av::ProtocolVersion(av::ProtocolVersion::current_major,
                                              av::ProtocolVersion::current_minor),
                          sub_apis, 64u, 12u, 24u, features,
                          av::GpuIdentity(1u, 2u, device_type, uuid, 0u, 0u, "the host's"));
}

class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), adapter_(platform_, &function_), sources_(9u), children_(9u)
    {
        host_.translate_guest_with([](void *context, std::uint64_t address, std::uint64_t length) {
            return static_cast<MockPlatform *>(context)->translate_physical(address, static_cast<std::size_t>(length));
        }, &platform_);
        CHECK_STATUS(present_engine_.initialize(), av::Status::ok);
    }
    ~Fixture() { static_cast<void>(adapter_.stop_device()); }

    std::uint32_t start(void *start_info = nullptr)
    {
        control_.reset();
        platform_.on_stall(&Fixture::answer_display_info, this);
        const std::uint32_t started = nt(adapter_.start_device(start_info, &dxgkrnl_.record(), &sources_, &children_));
        platform_.on_stall(nullptr, nullptr);
        return started;
    }

    DisplayInfoAnswer &display_info() { return display_info_; }

    enum class Answer { display_info, none, wrong_type, no_scanout };
    void answer_with(Answer answer) { answer_ = answer; }

    enum class Misanswer { none, silent, error, truncated };
    void misanswer(std::uint32_t asked, Misanswer how)
    {
        misanswered_ = asked;
        misanswer_ = how;
    }

    std::uint32_t present_copies() const { return present_copies_; }
    void answer_asks() { platform_.on_stall(&Fixture::answer_display_info, this); }

    static void answer_display_info_from(void *context) { answer_display_info(context); }

    void forget_control() { control_.reset(); }

    av::FakeVirtqueueDevice &control()
    {
        if (!control_.has_value()) {
            const av::FakeCommonConfig::Queue &queue = function_.common().queue(0u);
            control_.emplace(platform_, queue.desc, queue.driver, queue.device, queue.size);
        }
        return *control_;
    }

    void                   answer_capset_of(const std::uint8_t *uuid) { capset_ = host_capset(uuid); }
    void                   offer_opengl() { capset_ = host_capset(first_gpu, 0x3u); }
    void                   offer_present_engine()
    {
        capset_ = host_capset(first_gpu, 1u,
                              av::CapsetInfo::feature_host_visible_memory | av::CapsetInfo::feature_present_engine);
    }
    void                   offer_integrated_gpu()
    {
        capset_ = host_capset(first_gpu, 1u, av::CapsetInfo::feature_host_visible_memory,
                              av::GpuIdentity::integrated_gpu);
    }
    void                   offer_raises()
    {
        capset_ = host_capset(first_gpu, 1u,
                              av::CapsetInfo::feature_host_visible_memory | av::CapsetInfo::feature_raises);
    }
    void                   offer_present_snapshots()
    {
        capset_ = host_capset(first_gpu, 1u,
                              av::CapsetInfo::feature_host_visible_memory | av::CapsetInfo::feature_present_engine |
                                  av::CapsetInfo::feature_present_from_image);
    }
    void                   serve() { answer_display_info(this); }
    const std::uint8_t    *last_queued() const { return queued_; }
    std::uint32_t          queued_presents() const { return queued_count_; }
    void                   stop_presents() { present_engine_.detach(); }
    void                   offer_modes(const av::DisplayModeSet &modes) { offered_modes_ = modes; }
    void                   refuse_the_blob() { refuse_blob_ = true; }
    std::uint32_t          blob_maps() const { return blob_maps_; }
    std::uint32_t          blob_unmaps() const { return blob_unmaps_; }
    void                   ignore_capset() { ignore_capset_ = true; }
    av::FakeDisplayClock  &clock() { return clock_; }
    std::uint64_t          clock_page() const { return clock_.page(); }
    std::uint32_t          interrupt_every() const { return clock_.interrupt_every(); }
    void                   blank(std::uint32_t latched = 0u)
    {
        clock_.blank(latched);
        adapter_.dpc_routine();
    }
    std::uint32_t          context_requests() const { return context_requests_; }

    MockPlatform          &platform() { return platform_; }
    FakeVirtioGpuFunction &function() { return function_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    Adapter               &adapter() { return adapter_; }
    std::uint32_t          sources() const { return sources_; }
    std::uint32_t          children() const { return children_; }

private:
    static void answer_display_info(void *context)
    {
        auto *const fixture = static_cast<Fixture *>(context);
        const std::uint32_t asked = fixture->control().pending_type();
        if (asked == fixture->misanswered_ && fixture->misanswer_ != Misanswer::none) {
            if (fixture->misanswer_ == Misanswer::silent) return;
            av::GpuResponse answer;
            answer.set(av::GpuResponse::err_unspec);
            const std::uint32_t length = fixture->misanswer_ == Misanswer::truncated
                                             ? 2u
                                             : static_cast<std::uint32_t>(answer.length());
            static_cast<void>(fixture->control().respond_all(answer.bytes(), length));
            return;
        }
        if (asked == create_blob || asked == map_blob || asked == unmap_blob) {
            av::GpuResponse answer;
            if (fixture->refuse_blob_) {
                answer.set(av::GpuResponse::err_out_of_memory);
                static_cast<void>(fixture->control().respond_all(answer.bytes(),
                                                                 static_cast<std::uint32_t>(answer.length())));
                return;
            }
            if (asked == map_blob) {
                fixture->blob_maps_++;
                answer.set_map_info(av::GpuResponse::map_cache_cached);
            } else {
                fixture->blob_unmaps_ += asked == unmap_blob ? 1u : 0u;
                answer.set(av::GpuResponse::ok_nodata);
            }
            static_cast<void>(fixture->control().respond_all(answer.bytes(),
                                                             static_cast<std::uint32_t>(answer.length())));
            return;
        }
        if (asked == av::PageRequest::display_clock) {
            fixture->clock_.answer(fixture->control());
            return;
        }
        if (asked == av::PageRequest::present_engine || asked == av::QueuedPresentRequest::type) {
            static_cast<void>(fixture->control().answer_all(fixture->present_request_, sizeof fixture->present_request_,
                                                            &Fixture::answer_present_engine, fixture));
            return;
        }
        if ((asked == get_capset_info || asked == get_capset) && fixture->ignore_capset_) return;
        if (asked == get_capset_info || asked == get_capset) {
            av::GpuResponse answer;
            if (asked == get_capset_info)
                answer.set_capset_info(av::CapsetInfo::capset_id, 2u, av::CapsetInfo::encoded_size);
            else
                answer.set_capset(fixture->capset_);
            static_cast<void>(fixture->control().respond_all(answer.bytes(),
                                                             static_cast<std::uint32_t>(answer.length())));
            return;
        }
        if (asked == ctx_create || asked == ctx_destroy || asked == submit_3d || asked == resource_unref) {
            fixture->context_requests_++;
            static_cast<void>(fixture->control().respond_all(0x1100u));
            return;
        }
        if (asked == av::PresentCopyRequest::type) {
            fixture->present_copies_++;
            static_cast<void>(fixture->control().answer_all(fixture->present_request_, sizeof fixture->present_request_,
                                                            &Fixture::answer_present_copy, fixture));
            return;
        }
        if (asked == av::DisplayModeSet::request_type && fixture->offered_modes_.count() != 0u) {
            av::GpuResponse answer;
            answer.set_display_modes(fixture->offered_modes_);
            static_cast<void>(fixture->control().respond_all(answer.bytes(),
                                                             static_cast<std::uint32_t>(answer.length())));
            return;
        }
        if (fixture->answer_ == Answer::none) return;
        if (fixture->answer_ == Answer::wrong_type) {
            static_cast<void>(fixture->control().respond_all(0x1100u));
            return;
        }
        if (fixture->answer_ == Answer::no_scanout) fixture->display_info_.disable(0u);
        static_cast<void>(fixture->control().respond_all(fixture->display_info_.bytes(),
                                                         fixture->display_info_.length()));
    }

    static std::uint32_t answer_present_copy(void *context, const std::uint8_t *request, std::uint32_t length,
                                             std::uint8_t *answer, std::uint32_t room)
    {
        auto *const        fixture = static_cast<Fixture *>(context);
        av::MockRequest    mock(request, length);
        av::ControlRequest control(fixture->host_, &mock);
        static_cast<void>(fixture->present_commands_.answer(control, av::PresentCopyRequest::type));
        const std::size_t given = mock.response_size() < room ? mock.response_size() : room;
        std::memcpy(answer, mock.response_bytes(), given);
        return static_cast<std::uint32_t>(given);
    }

    static std::uint32_t answer_present_engine(void *context, const std::uint8_t *request, std::uint32_t length,
                                               std::uint8_t *answer, std::uint32_t room)
    {
        auto *const        fixture = static_cast<Fixture *>(context);
        std::uint32_t      type = 0u;
        std::memcpy(&type, request, sizeof type);
        if (type == av::QueuedPresentRequest::type) {
            const std::size_t kept = length < sizeof fixture->queued_ ? length : sizeof fixture->queued_;
            std::memcpy(fixture->queued_, request, kept);
            fixture->queued_count_++;
        }
        av::MockRequest    mock(request, length);
        av::ControlRequest control(fixture->host_, &mock);
        static_cast<void>(av::PresentEngineCommands(fixture->host_, fixture->present_engine_, av::PresentFrames()).answer(control, type));
        const std::size_t given = mock.response_size() < room ? mock.response_size() : room;
        std::memcpy(answer, mock.response_bytes(), given);
        return static_cast<std::uint32_t>(given);
    }

    MockPlatform          platform_;
    av::MockHost          host_;
    av::PresentCommands   present_commands_{host_};
    av::FenceInterrupts   present_interrupts_{host_};
    av::PresentEngine     present_engine_{host_, present_interrupts_};
    std::uint8_t          queued_[av::QueuedPresentRequest::encoded_size] = {};
    std::uint32_t         queued_count_ = 0u;
    std::uint8_t          present_request_[0x9000] = {};
    std::uint32_t         present_copies_ = 0u;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    Adapter               adapter_;
    DisplayInfoAnswer     display_info_;
    av::CapsetInfo        capset_ = host_capset(first_gpu);
    bool                  refuse_blob_ = false;
    std::uint32_t         blob_maps_ = 0u;
    std::uint32_t         blob_unmaps_ = 0u;
    std::uint32_t         context_requests_ = 0u;
    bool                  ignore_capset_ = false;
    av::FakeDisplayClock  clock_{platform_};
    av::DisplayModeSet    offered_modes_;
    Answer                answer_ = Answer::display_info;
    std::uint32_t         misanswered_ = 0u;
    Misanswer             misanswer_ = Misanswer::none;
    std::optional<av::FakeVirtqueueDevice> control_;
    std::uint32_t         sources_;
    std::uint32_t         children_;
};

class Primary {
public:
    Primary(Fixture &fixture, std::uint32_t width, std::uint32_t height, std::uint32_t spare_pages = 0u)
        : frames_{}, encoded_{}, handle_(nullptr)
    {
        const std::uint64_t frame_bytes = std::uint64_t{width} * height * 4u;
        const auto          frame_pages = static_cast<std::uint32_t>((frame_bytes + page - 1u) / page);
        const std::uint32_t pages = frame_pages + spare_pages;
        for (std::uint32_t index = 0u; index < pages; index++) frames_[index] = first_frame + index * page;
        const av::AllocationDescriptor descriptor(av::AllocationKind::shared_primary, true,
                                                  av::SurfaceLayout(width, height, 21u, width * 4u),
                                                  std::uint64_t{pages} * page, av::ScanoutTarget(0u, 60u, 1u));
        CHECK_STATUS(descriptor.encode(encoded_, sizeof encoded_), av::Status::ok);
        av::DxgkAllocationInfoRecord info = {};
        info.pPrivateDriverData = encoded_;
        info.PrivateDriverDataSize = av::AllocationDescriptor::encoded_size;
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info;
        CHECK_EQ(nt(fixture.adapter().create_allocation(&create)), success);
        handle_ = info.hAllocation;
        av::DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
        mapping.hAllocation = handle_;
        mapping.SegmentId = av::ApertureSegment::segment_id;
        mapping.NumberOfPages = pages;
        mapping.pMdl = fixture.platform().attach_page_frames(frames_, pages);
        std::uint8_t                        dma[64] = {};
        av::DxgkargBuildPagingBufferRecord paging = {};
        paging.pDmaBuffer = dma;
        paging.DmaSize = sizeof dma;
        paging.Operation = 5;
        std::memcpy(paging.Reserved, &mapping, sizeof mapping);
        CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&paging)), success);
    }

    void        *handle() const { return handle_; }
    std::int64_t address(std::uint64_t offset_in_pages = 0u) const
    {
        return av::ApertureSegment::base_address + static_cast<std::int64_t>(offset_in_pages * page);
    }

private:
    static constexpr std::uint64_t page = 0x1000u;
    static constexpr std::uint64_t first_frame = 0x40000000u;
    static constexpr std::uint32_t max_pages = 1024u;

    std::uint64_t frames_[max_pages];
    std::uint8_t  encoded_[av::AllocationDescriptor::encoded_size];
    void         *handle_;
};

class MappedSurface {
public:
    static constexpr std::uint32_t side = 8u;
    static constexpr std::uint32_t pitch = side * 4u;
    static constexpr std::uint32_t colour = 0x11223344u;

    MappedSurface(Fixture &fixture, av::FakeHandleTable &handles, void *device, av::AllocationKind kind,
                  bool shared = false)
        : platform_(fixture.platform()), block_(nullptr), frames_{}, encoded_{}, handle_(nullptr),
          open_handle_(nullptr), kmt_handle_(0u)
    {
        block_ = static_cast<std::uint8_t *>(platform_.allocate_contiguous(page));
        CHECK(block_ != nullptr);
        std::memset(block_, 0, page);
        frames_[0] = platform_.physical_address(block_);
        const av::ScanoutTarget target = kind == av::AllocationKind::shared_primary ? av::ScanoutTarget(0u, 60u, 1u)
                                                                                   : av::ScanoutTarget();
        const av::AllocationDescriptor descriptor(kind, true, av::SurfaceLayout(side, side, 21u, pitch), page,
                                                  target);
        CHECK_STATUS(descriptor.encode(encoded_, sizeof encoded_), av::Status::ok);
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, page);
        if (shared)
            CHECK_STATUS(asking.encode(encoded_ + av::AllocationDescriptor::encoded_size,
                                       av::SharedObjectRecord::encoded_size),
                         av::Status::ok);
        av::DxgkAllocationInfoRecord info = {};
        info.pPrivateDriverData = encoded_;
        info.PrivateDriverDataSize = av::AllocationDescriptor::encoded_size +
                                     (shared ? av::SharedObjectRecord::encoded_size : 0u);
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info;
        CHECK_EQ(nt(fixture.adapter().create_allocation(&create)), success);
        handle_ = info.hAllocation;
        av::DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
        mapping.hAllocation = handle_;
        mapping.SegmentId = av::ApertureSegment::segment_id;
        mapping.NumberOfPages = 1u;
        mapping.pMdl = platform_.attach_page_frames(frames_, 1u);
        std::uint8_t                       dma[64] = {};
        av::DxgkargBuildPagingBufferRecord paging = {};
        paging.pDmaBuffer = dma;
        paging.DmaSize = sizeof dma;
        paging.Operation = 5;
        std::memcpy(paging.Reserved, &mapping, sizeof mapping);
        CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&paging)), success);
        av::DxgkOpenAllocationInfoRecord opening = {};
        kmt_handle_ = handles.insert(handle_);
        opening.hAllocation = kmt_handle_;
        if (shared) {
            opening.pPrivateDriverData = encoded_;
            opening.PrivateDriverDataSize = info.PrivateDriverDataSize;
        }
        av::DxgkargOpenAllocationRecord open = {};
        open.NumAllocations = 1u;
        open.pOpenAllocation = &opening;
        CHECK_EQ(nt(fixture.adapter().open_allocation(device, &open)), success);
        open_handle_ = opening.hDeviceSpecificAllocation;
    }
    ~MappedSurface() { platform_.release_contiguous(block_, page); }
    MappedSurface(const MappedSurface &) = delete;
    MappedSurface &operator=(const MappedSurface &) = delete;

    void *open_handle() const { return open_handle_; }
    void *handle() const { return handle_; }
    std::uint32_t kmt_handle() const { return kmt_handle_; }

    void paint_all(std::uint32_t value)
    {
        for (std::uint32_t y = 0u; y < side; y++)
            for (std::uint32_t x = 0u; x < side; x++)
                std::memcpy(block_ + y * pitch + x * 4u, &value, sizeof value);
    }
    std::uint32_t count(std::uint32_t value) const
    {
        std::uint32_t found = 0u;
        for (std::uint32_t y = 0u; y < side; y++)
            for (std::uint32_t x = 0u; x < side; x++) {
                std::uint32_t pixel = 0u;
                std::memcpy(&pixel, block_ + y * pitch + x * 4u, sizeof pixel);
                found += pixel == value ? 1u : 0u;
            }
        return found;
    }

private:
    static constexpr std::uint64_t page = 0x1000u;

    MockPlatform     &platform_;
    std::uint8_t     *block_;
    std::uint64_t     frames_[1];
    std::uint8_t      encoded_[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size];
    void             *handle_;
    void             *open_handle_;
    std::uint32_t     kmt_handle_;
};

class PagesReader {
public:
    PagesReader(Fixture &fixture, std::uint32_t allocation)
        : fixture_(fixture), allocation_(allocation), bytes_{}, answer_()
    {
    }

    std::uint32_t begin(std::uint32_t wait_microseconds)
    {
        return send(av::PagesReadRecord::Action::begin, wait_microseconds);
    }
    std::uint32_t end() { return send(av::PagesReadRecord::Action::end, 0u); }
    std::uint32_t send_bytes(std::uint8_t *bytes, std::size_t length)
    {
        DxgkargEscapeRecord escape = {};
        escape.pPrivateDriverData = bytes;
        escape.PrivateDriverDataSize = static_cast<std::uint32_t>(length);
        return nt(fixture_.adapter().escape(&escape));
    }
    std::uint64_t writes() const { return answer_.writes(); }
    bool          complete() const { return answer_.complete(); }

private:
    std::uint32_t send(av::PagesReadRecord::Action action, std::uint32_t wait_microseconds)
    {
        CHECK_STATUS(av::PagesReadRecord(action, allocation_, wait_microseconds).encode(bytes_, sizeof bytes_),
                     av::Status::ok);
        const std::uint32_t status = send_bytes(bytes_, sizeof bytes_);
        if (status == success) CHECK_STATUS(answer_.read_answer(bytes_, sizeof bytes_), av::Status::ok);
        return status;
    }

    Fixture             &fixture_;
    std::uint32_t        allocation_;
    std::uint8_t         bytes_[av::PagesReadRecord::encoded_size];
    av::PagesReadRecord  answer_;
};

class StateDump {
public:
    explicit StateDump(Fixture &fixture) : text_{}
    {
        std::uint8_t data[av::TraceEscapeRecord::encoded_size + 16u * 1024u] = {};
        const av::TraceEscapeRecord request(av::TraceEscapeRecord::Kind::state, 0u, 16u * 1024u);
        CHECK_STATUS(request.encode(data, sizeof data), av::Status::ok);
        DxgkargEscapeRecord escape = {};
        escape.pPrivateDriverData = data;
        escape.PrivateDriverDataSize = sizeof data;
        CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
        av::TraceEscapeRecord answer = request;
        CHECK_STATUS(answer.accept_answer(data, sizeof data), av::Status::ok);
        std::memcpy(text_, data + av::TraceEscapeRecord::encoded_size, answer.window());
    }

    bool holds(const char *text) const { return std::strstr(text_, text) != nullptr; }

private:
    char text_[16u * 1024u + 1u];
};

}

TEST(adapter, remembers_its_physical_device_object)
{ MockPlatform platform; int pdo = 0;
  const Adapter adapter(platform, &pdo);
  CHECK(adapter.physical_device_object() == &pdo);
  CHECK(adapter.invariants_hold()); }


TEST(adapter_start_device, logs_the_step_it_reached)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "avgpu: StartDevice") == 0);
  av::DxgkChildDescriptorRecord relations[2] = {};
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations)), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "avgpu: QueryChildRelations") == 0); }

TEST(adapter_start_device, brings_the_device_to_driver_ok_and_reports_one_source_and_child)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.sources(), 1u);
  CHECK_EQ(fixture.children(), 1u);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 1u);
  CHECK_EQ(fixture.function().common().queue(0u).vector, 1u); }

TEST(adapter_start_device, a_second_start_is_invalid_device_state)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.start(), invalid_device_state);
  CHECK_EQ(fixture.dxgkrnl().maps(), 1u); }

TEST(adapter_start_device, missing_arguments_are_invalid_parameters)
{ Fixture fixture; std::uint32_t count = 0u;
  CHECK_EQ(nt(fixture.adapter().start_device(nullptr, nullptr, &count, &count)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().start_device(nullptr, &fixture.dxgkrnl().record(), nullptr, &count)),
           invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().start_device(nullptr, &fixture.dxgkrnl().record(), &count, nullptr)),
           invalid_parameter); }

TEST(adapter_start_device, each_refused_input_fails_with_its_status_and_leaves_nothing_behind)
{ struct Case { int knob; std::uint32_t expected; };
  const Case cases[] = {
      {0, no_memory},
      {1, invalid_parameter},
      {2, invalid_parameter},
      {3, no_space},
      {4, not_found},
      {5, invalid_parameter},
      {6, not_supported},
      {7, invalid_parameter},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      if (c.knob == 0) fixture.dxgkrnl().refuse_device_information(no_memory);
      if (c.knob == 1) fixture.dxgkrnl().refuse_config_read(invalid_parameter);
      if (c.knob == 2) fixture.dxgkrnl().short_config_read();
      if (c.knob == 3) fixture.dxgkrnl().refuse_map(1u, no_space);
      if (c.knob == 4) fixture.function().remove_virtio_bar_resource();
      if (c.knob == 5) fixture.function().config_space()[0x34u] = 0x20u;
      if (c.knob == 6) fixture.function().common().offer_features(FakeVirtioGpuFunction::feature_edid);
      if (c.knob == 7) fixture.dxgkrnl().set_system_memory(0);
      CHECK_EQ(fixture.start(), c.expected);
      CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
      CHECK_EQ(fixture.platform().live_allocations(), 0u);
      CHECK(fixture.adapter().invariants_hold());
  } }

TEST(adapter_start_device, a_table_older_than_wddm_3_2_is_not_supported)
{ Fixture fixture;
  fixture.dxgkrnl().record().Size = 0x100u;
  CHECK_EQ(fixture.start(), not_supported); }

TEST(adapter_start_device, a_failed_device_start_leaves_the_device_reset_and_unmapped)
{ Fixture fixture;
  fixture.function().common().set_queues(1u, 256u);
  CHECK_EQ(fixture.start(), not_found);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(fixture.platform().live_allocations(), 0u);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success); }

TEST(adapter_start_device, each_allocation_failing_in_turn_fails_cleanly_then_starts)
{ for (std::uint32_t nth = 1u; nth <= 8u; nth++) {
      Fixture fixture;
      fixture.platform().fail_allocation_at(nth);
      const std::uint32_t status = fixture.start();
      CHECK(status == success || status == no_memory);
      fixture.platform().fail_allocation_at(0u);
      if (status == no_memory) {
          CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
          CHECK_EQ(fixture.platform().live_allocations(), 0u);
          CHECK_EQ(fixture.start(), success);
      }
  } }

TEST(adapter_start_device, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.start());
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {} }


TEST(adapter_stop_device, resets_the_device_and_releases_every_mapping)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(fixture.platform().live_allocations(), 0u);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_stop_device, a_second_stop_and_a_stop_before_start_are_harmless)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success); }

TEST(adapter_stop_device, a_device_that_ignores_reset_is_reported_and_still_released)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.function().common().delay_reset(100000u);
  CHECK_EQ(nt(fixture.adapter().stop_device()), invalid_device_state);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(fixture.platform().live_allocations(), 0u); }

TEST(adapter_stop_device, restarts_after_stop)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status); }

TEST(adapter, destruction_without_stop_releases_everything)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  { FakeDxgkrnlAdapter dxgkrnl(platform, function);
    { Adapter adapter(platform, &function);
      std::uint32_t sources = 0u, children = 0u;
      CHECK_EQ(nt(adapter.start_device(nullptr, &dxgkrnl.record(), &sources, &children)), success); }
    CHECK_EQ(dxgkrnl.live_mappings(), 0u);
    CHECK_EQ(function.common().status(), 0u); }
  CHECK_EQ(platform.live_allocations(), 0u); }


TEST(adapter_set_power_state, d3_resets_the_device_and_d0_brings_it_back)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 1u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_interrupt_routine, an_interrupt_while_the_device_is_down_is_not_ours)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 0u);
  fixture.forget_control();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(adapter_set_power_state, a_child_transition_needs_nothing)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().set_power_state(0u, power_d3, 0)), success); }

TEST(adapter_set_power_state, before_start_is_invalid_device_state)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), invalid_device_state); }

TEST(adapter_set_power_state, an_unspecified_or_unknown_state_is_invalid)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, 0, 0)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, 5, 0)), invalid_parameter);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status); }

TEST(adapter_set_power_state, a_device_that_will_not_come_back_is_reported)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.function().common().set_queues(1u, 256u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), not_found);
  CHECK_EQ(fixture.platform().live_allocations(), 2u);    }


TEST(adapter_reset_device, writes_reset_at_any_irql_without_allocating)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::uint32_t allocations = fixture.platform().allocations_made();
  fixture.platform().set_irql(Irql::isr);
  fixture.adapter().reset_device();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.platform().allocations_made(), allocations);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(adapter_reset_device, before_start_touches_nothing)
{ Fixture fixture;
  fixture.adapter().reset_device();
  CHECK_EQ(fixture.function().common().status_writes(), 0u); }


TEST(adapter_interrupt_routine, a_message_interrupt_is_claimed_and_queues_a_dpc)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(1u), 1u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u); }

TEST(adapter_interrupt_routine, a_line_interrupt_is_claimed_only_with_isr_bits_set)
{ Fixture fixture;
  fixture.function().set_message_interrupts(0u);
  CHECK_EQ(fixture.start(), success);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.function().isr().raise(0x1u);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 1u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u); }

TEST(adapter_interrupt_routine, an_interrupt_before_start_is_not_claimed)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 0u); }

TEST(adapter_dpc_routine, harvests_the_control_queue_at_dispatch)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(adapter_dpc_routine, before_start_or_after_power_down_does_nothing)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  const std::uint32_t quiet = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), quiet);
  CHECK_EQ(fixture.dxgkrnl().dpc_notifications(), 1u); }

TEST(adapter_dpc_routine, a_device_lying_about_a_completion_is_failed_and_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(0u);
  const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
  used[2] = 9u;
  const std::uint32_t logged = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().logged() > logged);
  CHECK((fixture.function().common().status() & failed_bit) != 0u); }


TEST(adapter_query_child_relations, reports_one_hot_pluggable_video_output)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  DxgkChildDescriptorRecord relations[2];
  std::memset(relations, 0xEE, sizeof relations);
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations)), success);
  CHECK_EQ(relations[0].ChildDeviceType, 1);
  CHECK_EQ(relations[0].InterfaceTechnology, -1);
  CHECK_EQ(relations[0].MonitorOrientationAwareness, 1);
  CHECK_EQ(relations[0].SupportsSdtvModes, 0u);
  CHECK_EQ(relations[0].HpdAwareness, 4);
  CHECK_EQ(relations[0].AcpiUid, 0u);
  CHECK_EQ(relations[0].ChildUid, 0u);
  CHECK_EQ(relations[1].ChildDeviceType, static_cast<std::int32_t>(0xEEEEEEEEu)); }

TEST(adapter_query_child_relations, no_room_is_buffer_too_small_and_no_buffer_invalid)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  DxgkChildDescriptorRecord relations[2];
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations - 1u)), buffer_too_small);
  CHECK_EQ(nt(fixture.adapter().query_child_relations(nullptr, sizeof relations)), invalid_parameter); }

TEST(adapter_query_child_status, the_one_child_is_connected)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  DxgkChildStatusRecord status = {};
  status.Type = 1;
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 1u)), success);
  CHECK_EQ(status.HotPlugConnected, 1u); }

TEST(adapter_query_child_status, rotation_other_types_and_other_children_are_refused)
{ Fixture fixture;
  DxgkChildStatusRecord status = {};
  status.Type = 2;
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), invalid_parameter);
  status.Type = 3;
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), not_supported);
  status.Type = 1;
  status.ChildUid = 1u;
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), invalid_parameter);
  CHECK_EQ(status.HotPlugConnected, 0u);
  CHECK_EQ(nt(fixture.adapter().query_child_status(nullptr, 0u)), invalid_parameter); }

TEST(adapter_query_device_descriptor, answers_the_synthetic_monitor_edid_for_the_one_child)
{ Fixture fixture;
  std::uint8_t edid[128] = {};
  DxgkDeviceDescriptorRecord descriptor = {0u, sizeof edid, edid};
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  CHECK_EQ(descriptor.DescriptorLength, 128u);
  CHECK_EQ(edid[1], 0xFFu);
  descriptor.DescriptorOffset = 128u;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), no_more_descriptor_data); }

TEST(adapter_query_device_descriptor, another_child_or_no_descriptor_is_invalid)
{ Fixture fixture;
  std::uint8_t edid[128] = {};
  DxgkDeviceDescriptorRecord descriptor = {0u, sizeof edid, edid};
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(1u, &descriptor)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, nullptr)), invalid_parameter);
  CHECK_EQ(edid[0], 0u); }


namespace {

constexpr std::uint32_t invalid_source_mode = 0xC01E0310u;
constexpr std::uint32_t transformation_not_supported = 0xC01E0346u;
constexpr std::uint32_t gamma_ramp_not_supported = 0xC01E0348u;
constexpr std::uint32_t no_recommended_functional_vidpn = 0xC01E0323u;
constexpr std::uint32_t no_recommended_vidpn_topology = 0xC01E031Au;
constexpr std::int32_t  interrupt_crtc_vsync = 3;
constexpr std::int32_t  interrupt_dpc_log = 4;
constexpr std::int32_t  no_pivot = 5;

av::D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    av::D3dkmdtVidPnSourceModeRecord mode = {};
    av::DisplayMode(width, height).describe(mode, av::DisplayMode::source_formats[0]);
    return mode;
}

std::uint32_t commit(Fixture &fixture, std::uint32_t width, std::uint32_t height)
{
    av::FakeVidPn vidpn;
    vidpn.add_path(0u, 0u);
    vidpn.source_modes(0u).seed(source_mode(width, height), true);
    const av::DxgkargCommitVidPnRecord request = {&vidpn, 0u, 0, nullptr, 0u, 0u};
    return nt(fixture.adapter().commit_vidpn(&request));
}

}

TEST(adapter_is_supported_vidpn, answers_for_the_proposed_vidpn)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 1u);
  av::DxgkargIsSupportedVidPnRecord request = {&vidpn, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().is_supported_vidpn(&request)), success);
  CHECK_EQ(request.IsVidPnSupported, 0u);
  CHECK_EQ(nt(fixture.adapter().is_supported_vidpn(nullptr)), invalid_parameter); }

TEST(adapter_enum_vidpn_cofunc_modality, offers_the_modes_on_the_path)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  const av::DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, no_pivot, 0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(&request)), success);
  CHECK_EQ(vidpn.source_modes(0u).modes(), 5u * av::DisplayMode::source_format_count);
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(nullptr)), invalid_parameter); }

TEST(adapter_enum_vidpn_cofunc_modality, before_start_dxgkrnl_cannot_be_reached)
{ Fixture fixture;
  av::FakeVidPn vidpn;
  const av::DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, no_pivot, 0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(&request)), invalid_device_state); }

TEST(adapter_commit_vidpn, records_the_mode_scan_lines_follow)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  av::DxgkargGetScanLineRecord scan = {0u, 0u, {}, 0u};
  const std::uint64_t second = 10000000u;
  fixture.platform().advance_time(second - fixture.platform().interrupt_time() % second + 163000u);
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), success);
  CHECK_EQ(scan.ScanLine, 814u);
  CHECK_EQ(scan.InVerticalBlank, 1u);
  fixture.platform().advance_time(second - 163000u + 83333u);
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), success);
  CHECK_EQ(scan.ScanLine, 416u);
  CHECK_EQ(scan.InVerticalBlank, 0u);
  CHECK_EQ(commit(fixture, 1000u, 700u), invalid_source_mode);
  CHECK_EQ(nt(fixture.adapter().commit_vidpn(nullptr)), invalid_parameter); }

TEST(adapter_set_power_state, the_guest_keeps_asking_while_power_is_down_and_nothing_touches_the_device)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  const Primary primary(fixture, 1280u, 800u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);

  av::DxgkargSetVidPnSourceAddressRecord address = {};
  address.PrimaryAddress = primary.address();
  address.PrimarySegment = av::ApertureSegment::segment_id;
  address.hAllocation = primary.handle();
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  av::DxgkargGetScanLineRecord scan = {0u, 0u, {}, 0u};
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), success);
  const av::DxgkargSetPointerPositionRecord position = {0u, 10, 10, 0u};
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(&position)), invalid_device_state);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);

  fixture.forget_control();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_set_power_state, what_was_scanning_out_before_d3_is_scanning_out_after_d0)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  CHECK_EQ(control.respond_all(0x1100u), 1u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  av::FakeVirtqueueDevice &resumed = fixture.control();
  CHECK_EQ(resumed.respond_all(0x1100u), 2u);
  CHECK_EQ(resumed.request_type(0u), 0x02F4u);
  CHECK_EQ(resumed.request_type(1u), 0x0101u);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_set_power_state, a_source_that_was_dark_stays_dark_across_power)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  CHECK_EQ(control.respond_all(0x1100u), 0u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  const std::uint64_t resumed_at = fixture.platform().interrupt_time();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK_EQ(fixture.control().respond_all(0x1100u), 1u);
  CHECK_EQ(fixture.control().request_type(0u), 0x02F4u);
  const std::uint64_t waited = fixture.platform().interrupt_time() - resumed_at;
  CHECK(waited >= 30000000u && waited < 31000000u);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_set_power_state, a_resume_asks_an_answering_device_what_it_is)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK(fixture.control().requests() >= 2u);
  CHECK_EQ(fixture.control().request_type(0u), 0x02F4u);
  CHECK_EQ(fixture.control().request_type(1u), 0x0108u);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_commit_vidpn, the_committed_source_reaches_the_host_scanout)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {0u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visible)), success);
  const Primary primary(fixture, 1280u, 800u);
  av::DxgkargSetVidPnSourceAddressRecord address = {};
  address.PrimaryAddress = primary.address();
  address.PrimarySegment = av::ApertureSegment::segment_id;
  address.hAllocation = primary.handle();
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
  fixture.platform().advance_time(display_period);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  const std::uint32_t after_commit = control.respond_all(0x1100u);
  CHECK_EQ(after_commit, 5u);
  CHECK_EQ(control.request_type(0u), 0x02F4u);
  CHECK_EQ(control.request_type(5u), 0x0101u);
  CHECK_EQ(control.request_type(6u), 0x0106u);
  CHECK_EQ(control.request_type(7u), 0x0103u);
  CHECK_EQ(control.request_type(8u), 0x0105u);
  CHECK_EQ(control.request_type(9u), 0x0104u);
  fixture.platform().advance_time(display_period);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(control.respond_all(0x1100u), 0u); }

TEST(adapter_dpc_routine, tells_dxgkrnl_of_the_blank_before_it_refreshes_the_scanout)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {0u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visible)), success);
  const Primary primary(fixture, 1280u, 800u);
  av::DxgkargSetVidPnSourceAddressRecord address = {};
  address.PrimaryAddress = primary.address();
  address.PrimarySegment = av::ApertureSegment::segment_id;
  address.hAllocation = primary.handle();
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  struct Seen {
      av::FakeVirtqueueDevice *control;
      std::int32_t             reported;
      std::uint16_t            sent_at_vsync;
      std::uint16_t            sent_at_notify;
  } seen = {&fixture.control(), -1, 0xFFFFu, 0xFFFFu};
  fixture.dxgkrnl().on_interrupt(+[](void *context, const av::DxgkargcbNotifyInterruptDataRecord &data) {
      auto *const watch = static_cast<Seen *>(context);
      watch->reported = data.InterruptType;
      watch->sent_at_vsync = watch->control->available_index();
  }, &seen);
  fixture.dxgkrnl().on_notify_dpc(+[](void *context) {
      auto *const watch = static_cast<Seen *>(context);
      watch->sent_at_notify = watch->control->available_index();
  }, &seen);
  const std::uint16_t before = fixture.control().available_index();
  fixture.platform().advance_time(display_period);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.blank();
  fixture.platform().set_irql(Irql::passive);
  fixture.dxgkrnl().on_interrupt(nullptr, nullptr);
  fixture.dxgkrnl().on_notify_dpc(nullptr, nullptr);
  CHECK_EQ(seen.reported, interrupt_crtc_vsync);
  CHECK_EQ(seen.sent_at_vsync, before);
  CHECK_EQ(seen.sent_at_notify, before);
  CHECK(fixture.control().available_index() != before); }

TEST(adapter_dpc_routine, a_refused_scanout_refresh_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(commit(fixture, 640u, 480u), success);
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {0u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visible)), success);
  const Primary primary(fixture, 640u, 480u, 200u);
  const std::uint32_t logged = fixture.platform().logged();
  bool refused = false;
  for (std::uint32_t frame = 0u; frame < 200u && !refused; frame++) {
      av::DxgkargSetVidPnSourceAddressRecord address = {};
      address.PrimaryAddress = primary.address(frame);
      address.PrimarySegment = av::ApertureSegment::segment_id;
      address.hAllocation = primary.handle();
      fixture.platform().set_irql(Irql::isr);
      CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
      fixture.platform().advance_time(display_period);
      fixture.platform().set_irql(Irql::dispatch);
      fixture.adapter().dpc_routine();
      fixture.platform().set_irql(Irql::passive);
      refused = fixture.platform().logged() != logged;
  }
  CHECK(refused);
  CHECK(std::strcmp(fixture.platform().last_log(), "avgpu: a scanout refresh was refused in the DPC") == 0);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  av::DxgkargSetVidPnSourceAddressRecord moved = {};
  moved.PrimaryAddress = primary.address(199u);
  moved.PrimarySegment = av::ApertureSegment::segment_id;
  moved.hAllocation = primary.handle();
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&moved)), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), spent); }

TEST(adapter_commit_vidpn, a_refused_scanout_resource_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  bool refused = false;
  for (std::uint32_t attempt = 0u; attempt < 200u && !refused; attempt++) {
      const std::uint32_t width = (attempt % 2u) == 0u ? 1280u : 1024u;
      const std::uint32_t height = (attempt % 2u) == 0u ? 800u : 768u;
      const std::uint32_t before = fixture.platform().logged();
      CHECK_EQ(commit(fixture, width, height), success);
      refused = fixture.platform().logged() > before + 1u;
  }
  CHECK(refused); }

TEST(adapter_commit_vidpn, a_path_that_is_gone_scans_out_nothing)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  CHECK_EQ(control.respond_all(0x1100u), 1u);
  av::FakeVidPn empty;
  const av::DxgkargCommitVidPnRecord request = {&empty, 0u, 0, nullptr, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().commit_vidpn(&request)), success);
  CHECK_EQ(control.respond_all(0x1100u), 0u);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_get_scan_line, needs_a_committed_mode_on_the_one_target)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargGetScanLineRecord scan = {0u, 0u, {}, 0u};
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), invalid_device_state);
  CHECK_EQ(commit(fixture, 640u, 480u), success);
  scan.VidPnTargetId = 1u;
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_scan_line(nullptr)), invalid_parameter); }

TEST(adapter_update_active_vidpn_present_path, accepts_identity_transformation_only)
{ Fixture fixture;
  av::D3dkmdtVidPnPresentPathRecord path = {};
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 1;
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), success);
  const std::int32_t scalings[] = {0, 2, 5, 254, 255};
  for (const std::int32_t scaling : scalings) {
      path.ContentTransformationScaling = scaling;
      CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), transformation_not_supported);
  }
  path.ContentTransformationScaling = 1;
  const std::int32_t rotations[] = {0, 2, 4, 5, 254, 255};
  for (const std::int32_t rotation : rotations) {
      path.ContentTransformationRotation = rotation;
      CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), transformation_not_supported);
  } }

TEST(adapter_update_active_vidpn_present_path, refuses_every_gamma_ramp_it_cannot_program)
{ Fixture fixture;
  av::D3dkmdtVidPnPresentPathRecord path = {};
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 1;
  path.GammaRampType = 1;
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), success);
  std::uint16_t levels[3][256] = {};
  for (std::int32_t ramp = 2; ramp <= 5; ramp++) {
      path.GammaRampType = ramp;
      path.GammaRampData = levels;
      path.GammaRampDataSize = sizeof levels;
      CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), gamma_ramp_not_supported);
  } }

TEST(adapter_update_active_vidpn_present_path, a_path_the_adapter_lacks_is_invalid)
{ Fixture fixture;
  av::D3dkmdtVidPnPresentPathRecord path = {};
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 1;
  path.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), invalid_parameter);
  path.VidPnSourceId = 0u;
  path.VidPnTargetId = 1u;
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(nullptr)), invalid_parameter); }

TEST(adapter_recommend_monitor_modes, fills_the_monitor_mode_set)
{ Fixture fixture;
  av::FakeVidPn vidpn;
  const av::DxgkargRecommendMonitorModesRecord request = {0u, 0u, vidpn.monitor_mode_set(), &vidpn.monitor_interface()};
  CHECK_EQ(nt(fixture.adapter().recommend_monitor_modes(&request)), success);
  CHECK_EQ(vidpn.monitor_modes().modes(), 11u);
  CHECK_EQ(nt(fixture.adapter().recommend_monitor_modes(nullptr)), invalid_parameter); }

TEST(adapter, recommend_functional_vidpn_and_recommend_vidpn_topology_decline)
{ Fixture fixture;
  const av::DxgkargRecommendFunctionalVidPnRecord functional = {};
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  CHECK_EQ(nt(fixture.adapter().recommend_functional_vidpn(&functional)), no_recommended_functional_vidpn);
  CHECK_EQ(nt(fixture.adapter().recommend_vidpn_topology(&topology)), no_recommended_vidpn_topology); }

TEST(adapter_control_interrupt, vsync_runs_between_enable_and_disable)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.clock_page() != 0u);
  CHECK_EQ(fixture.interrupt_every(), av::DeviceVblank::sweep_every);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  CHECK_EQ(fixture.interrupt_every(), 1u);
  fixture.blank();
  fixture.blank();
  fixture.adapter().dpc_routine();
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).InterruptType, interrupt_crtc_vsync);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 0u)), success);
  CHECK_EQ(fixture.interrupt_every(), av::DeviceVblank::sweep_every);
  fixture.blank();
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_dpc_log, 1u)), not_supported); }

TEST(adapter_control_interrupt, a_device_that_refuses_the_clock_is_said_to_and_still_starts)
{ Fixture fixture;
  fixture.clock().refuse();
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.platform().logged_line("avgpu: the device refused the display clock, status 0xC00000BB"));
  CHECK_EQ(fixture.clock_page(), 0u); }

TEST(adapter_control_interrupt, before_start_there_is_no_vsync)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), invalid_device_state);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 0u)), success); }

TEST(adapter_stop_device, stops_the_vsync)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  fixture.adapter().dpc_routine();
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u); }

TEST(adapter_set_vidpn_source_address, the_vsync_reports_the_primary_it_names)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.PrimarySegment = 1u;
  request.PrimaryAddress = 0x7000000000ll;
  request.hAllocation = &primary;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0x70u);
  request.hAllocation = nullptr;
  request.PrimarySegment = 9u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).Argument3, 0u); }

TEST(adapter_set_vidpn_source_address, an_immediate_flip_is_not_reported_and_shows_at_the_next_vsync)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.PrimarySegment = 1u;
  request.PrimaryAddress = 0x5000000000ll;
  request.hAllocation = &primary;
  request.Flags = 0x2u;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0x50u); }

TEST(adapter_set_vidpn_source_address, a_primary_in_system_memory_names_segment_zero)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.hAllocation = &primary;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument2, 0u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0u); }

TEST(adapter_set_vidpn_source_address, another_segment_or_source_is_invalid)
{ Fixture fixture;
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.hAllocation = &primary;
  request.PrimarySegment = 2u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), invalid_parameter);
  request.PrimarySegment = 1u;
  request.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(nullptr)), invalid_parameter); }

TEST(adapter_set_vidpn_source_address_with_multi_plane_overlay, shows_the_base_plane_and_reports_multiplane_vsyncs)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  int primary = 0, overlay = 0;
  av::DxgkMultiplaneOverlayPlaneRecord planes[2] = {};
  planes[0].LayerIndex = 1u;
  planes[0].Enabled = 1u;
  planes[0].AllocationSegment = 1u;
  planes[0].AllocationAddress = 0x9000000000ll;
  planes[0].hAllocation = &overlay;
  planes[1].LayerIndex = 0u;
  planes[1].Enabled = 1u;
  planes[1].AllocationSegment = 1u;
  planes[1].AllocationAddress = 0x6000000000ll;
  planes[1].hAllocation = &primary;
  planes[1].PlaneAttributes[1] = 7u;
  planes[1].PlaneAttributes[21] = 3u;
  av::DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord request = {};
  request.PlaneCount = 2u;
  request.pPlanes = planes;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  fixture.blank();
  const av::DxgkargcbNotifyInterruptDataRecord base = fixture.dxgkrnl().interrupt(0u);
  CHECK_EQ(base.InterruptType, 7);
  CHECK_EQ(base.Argument1, 1u);
  CHECK_EQ(base.Argument2, 1u);
  const auto *const layer = reinterpret_cast<const av::DxgkMultiplaneOverlayVsyncInfoRecord *>(
      static_cast<std::uintptr_t>(base.UnionRemainder[0]) |
      (static_cast<std::uintptr_t>(base.UnionRemainder[1]) << 32u));
  CHECK(layer != nullptr);
  if (layer != nullptr) {
      CHECK_EQ(layer->LayerIndex, 0u);
      CHECK_EQ(layer->Enabled, 1u);
      CHECK_EQ(layer->PhysicalAddress, 0x6000000000ll);
      CHECK_EQ(layer->PlaneAttributes[1], 7u);
      CHECK_EQ(layer->PlaneAttributes[21], 3u);
  }
  request.PlaneCount = 1u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), success);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).InterruptType, 7);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).Argument2, 0u);
  request.PlaneCount = 0u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), success);
  request.PlaneCount = 2u;
  planes[1].Enabled = 0u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), success);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).InterruptType, 7);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).Argument2, 0u);
  av::DxgkargSetVidPnSourceAddressRecord plain = {};
  plain.PrimarySegment = 1u;
  plain.PrimaryAddress = 0x5000000000ll;
  plain.hAllocation = &primary;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&plain)), success);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(3u).InterruptType, 3);
  CHECK_EQ(fixture.dxgkrnl().interrupt(3u).Argument3, 0x50u);
  planes[1].Enabled = 1u;
  planes[1].AllocationSegment = 2u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), invalid_parameter);
  request.pPlanes = nullptr;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), invalid_parameter); }

TEST(adapter_set_vidpn_source_address_with_multi_plane_overlay3, records_every_plane_and_reports_each_present_id)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  int primary = 0, overlay = 0;
  av::DxgkPrimaryContextDataRecord base_view = {};
  base_view.hAllocation = &primary;
  base_view.SegmentId = 1u;
  base_view.SegmentAddress = 0x6000000000ll;
  av::DxgkPrimaryContextDataRecord overlay_view = {};
  overlay_view.hAllocation = &overlay;
  overlay_view.SegmentId = 1u;
  overlay_view.SegmentAddress = 0x9000000000ll;
  av::DxgkPrimaryContextDataRecord *base_views[] = {&base_view};
  av::DxgkPrimaryContextDataRecord *overlay_views[] = {&overlay_view};
  av::DxgkMultiplaneOverlayPlane3Record planes[2] = {};
  planes[0].LayerIndex = 1u;
  planes[0].PresentId = 77u;
  planes[0].InputFlags = av::plane_input_enabled;
  planes[0].ContextCount = 1u;
  planes[0].ppContextData = overlay_views;
  planes[0].PlaneAttributes.DstRect = {100, 50, 420, 290};
  planes[1].LayerIndex = 0u;
  planes[1].PresentId = 41u;
  planes[1].InputFlags = av::plane_input_enabled;
  planes[1].ContextCount = 1u;
  planes[1].ppContextData = base_views;
  av::DxgkMultiplaneOverlayPlane3Record *plane_list[] = {&planes[0], &planes[1]};
  av::DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record request = {};
  request.PlaneCount = 2u;
  request.ppPlanes = plane_list;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay3(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(AdapterProbe(fixture.adapter()).scanout(0u), 0x6000000000ull);
  CHECK(AdapterProbe(fixture.adapter()).primary(0u) == &primary);
  CHECK(AdapterProbe(fixture.adapter()).plane(0u, 1u).allocation() == &overlay);
  CHECK_EQ(AdapterProbe(fixture.adapter()).plane(0u, 1u).destination().right, 420);
  fixture.blank();
  const av::DxgkargcbNotifyInterruptDataRecord shown = fixture.dxgkrnl().interrupt(0u);
  CHECK_EQ(shown.InterruptType, 10);
  CHECK_EQ(shown.Argument1, 1u);
  CHECK_EQ(shown.Argument2, 2u);
  const auto *const layers = reinterpret_cast<const av::DxgkMultiplaneOverlayVsyncInfo2Record *>(
      static_cast<std::uintptr_t>(shown.UnionRemainder[0]) |
      (static_cast<std::uintptr_t>(shown.UnionRemainder[1]) << 32u));
  CHECK(layers != nullptr);
  if (layers != nullptr) {
      CHECK_EQ(layers[0].LayerIndex, 0u);
      CHECK_EQ(layers[0].PresentId, 41u);
      CHECK_EQ(layers[1].LayerIndex, 1u);
      CHECK_EQ(layers[1].PresentId, 77u);
  }
  planes[1].InputFlags = 0u;
  planes[1].PresentId = 42u;
  av::DxgkMultiplaneOverlayPlane3Record *base_only[] = {&planes[1]};
  request.PlaneCount = 1u;
  request.ppPlanes = base_only;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay3(&request)), success);
  CHECK_EQ(AdapterProbe(fixture.adapter()).scanout(0u), 0u);
  CHECK(AdapterProbe(fixture.adapter()).primary(0u) == nullptr);
  CHECK(AdapterProbe(fixture.adapter()).plane(0u, 1u).allocation() == &overlay);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).InterruptType, 10);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).Argument2, 2u);
  planes[1].InputFlags = av::plane_input_enabled;
  base_view.SegmentId = 2u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay3(&request)), success);
  CHECK(AdapterProbe(fixture.adapter()).primary(0u) == nullptr);
  request.ppPlanes = nullptr;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay3(&request)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay3(nullptr)), invalid_parameter); }

TEST(adapter_overlay_ddis, answer_caps_post_composition_and_checks_for_the_adapter_s_sources)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargGetMultiPlaneOverlayCapsRecord caps = {};
  CHECK_EQ(nt(fixture.adapter().get_multi_plane_overlay_caps(&caps)), success);
  CHECK_EQ(caps.MaxPlanes, 1u);
  caps.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().get_multi_plane_overlay_caps(&caps)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_multi_plane_overlay_caps(nullptr)), invalid_parameter);
  av::DxgkargGetPostCompositionCapsRecord post = {};
  CHECK_EQ(nt(fixture.adapter().get_post_composition_caps(&post)), success);
  CHECK_EQ(post.MaxStretchFactor, 0x3F800000u);
  CHECK_EQ(nt(fixture.adapter().get_post_composition_caps(nullptr)), invalid_parameter);
  av::DxgkargCheckMultiPlaneOverlaySupport3Record check = {};
  CHECK_EQ(nt(fixture.adapter().check_multi_plane_overlay_support3(&check)), success);
  CHECK_EQ(check.Supported, 1);
  CHECK_EQ(nt(fixture.adapter().check_multi_plane_overlay_support3(nullptr)), invalid_parameter);
  av::DxgkMultiplaneOverlayPlaneWithSource2Record second = {};
  second.LayerIndex = 1u;
  av::DxgkMultiplaneOverlayPlaneWithSource2Record *planes[] = {&second};
  check.PlaneCount = 1u;
  check.ppPlanes = planes;
  CHECK_EQ(nt(fixture.adapter().check_multi_plane_overlay_support3(&check)), success);
  CHECK_EQ(check.Supported, 0);
  CHECK(fixture.platform().logged_line("overlay refused, plane 0 of 1 layer 1")); }

TEST(adapter_set_vidpn_source_visibility, records_it_for_the_source)
{ Fixture fixture;
  av::DxgkargSetVidPnSourceVisibilityRecord request = {0u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&request)), success);
  request.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&request)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(nullptr)), invalid_parameter); }

TEST(adapter_present, needs_a_live_context_then_presents)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint8_t dma[128] = {};
  av::D3dddiPatchLocationListRecord patches[2] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 2u;
  present.Flags = 1u;
  CHECK_EQ(nt(fixture.adapter().present(&fixture, &present)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(dma[0], 0x41u);
  CHECK_EQ(patches[1].AllocationIndex, 2u);
  CHECK(fixture.platform().logged_line("avgpu: present unmoved list flags 0x00000001 sources 0 destinations 0"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_blt_between_two_opened_allocations_moves_its_pixels)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  av::DxgkAllocationListRecord list[2] = {};
  list[0].hDeviceSpecificAllocation = source.open_handle();
  list[1].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t                      dma[512] = {};
  av::D3dddiPatchLocationListRecord  patches[8] = {};
  av::DxgkargPresentRecord          present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 8u;
  present.Flags = 1u;
  present.pAllocationList = list;
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.present_copies(), 1u);
  CHECK(!fixture.platform().logged_line("would not move"));

  const StateDump dump(fixture);
  CHECK(dump.holds("present_rows=8"));
  CHECK(dump.holds("presents_unmoved=0"));
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);

  destination.paint_all(0u);
  const av::RectRecord changed[2] = {{0, 0, side, 1}, {0, 2, side, 3}};
  present.SubRectCnt = 2u;
  present.pDstSubRects = changed;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(destination.count(MappedSurface::colour), 2u * MappedSurface::side);
  present.SubRectCnt = 0u;
  present.pDstSubRects = nullptr;

  present.DstRect = {0, 0, side, side / 2};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unmoved surfaces flags 0x00000001 sources 1 destinations 1"));
  CHECK(fixture.platform().logged_line("avgpu: present surfaces reachable source 1 kind 2 destination 1 kind 1 from 8x8 into 8x4"));

  present.DstRect = {0, 0, side, side};
  list[1].hDeviceSpecificAllocation = &fixture;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unmoved handles flags 0x00000001 sources 1 destinations 1"));

  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, an_application_s_blt_on_a_virtual_context_moves_its_pixels)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  destination.paint_all(0u);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t             dma[512] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
  CHECK(!fixture.platform().logged_line("avgpu: present unmoved"));
  CHECK_EQ(fixture.present_copies(), 1u);
  const StateDump dump(fixture);
  CHECK(dump.holds("presents_unmoved=0"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, each_pass_of_a_present_moves_the_sub_rectangles_it_wrote)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  destination.paint_all(0u);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  const av::RectRecord rows[] = {{0, 0, side, 1}, {0, 3, side, 4}};
  std::uint8_t             dma[av::PresentCommand::header_size + av::PresentCommand::sub_rectangle_size] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = av::PresentCommand::header_size;
  present.Flags = 1u;
  present.pAllocationList = info;
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  present.SubRectCnt = 2u;
  present.pDstSubRects = rows;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), 0xC01E0001u);
  CHECK_EQ(fixture.present_copies(), 0u);
  present.DmaSize = sizeof dma;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), 0xC01E0001u);
  CHECK_EQ(present.MultipassOffset, 1u);
  CHECK_EQ(fixture.present_copies(), 1u);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side);
  present.pDmaBuffer = dma;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(fixture.present_copies(), 2u);
  CHECK_EQ(destination.count(MappedSurface::colour), 2u * MappedSurface::side);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_queued_blt_holds_its_packet_until_the_engine_has_moved_the_pixels)
{ Fixture fixture;
  fixture.offer_present_engine();
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.platform().logged_line("moved by the device's present engine"));
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  destination.paint_all(0u);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t             dma[512] = {};
  std::uint8_t             private_data[av::PresentTicketRecord::encoded_size] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pDmaBufferPrivateData = private_data;
  present.DmaBufferPrivateDataSize = sizeof private_data;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(fixture.present_copies(), 0u);
  CHECK_EQ(destination.count(MappedSurface::colour), 0u);
  CHECK(present.pDmaBufferPrivateData == private_data + sizeof private_data);
  CHECK_EQ(present.DmaBufferPrivateDataSize, 0u);
  av::PresentTicketRecord record;
  CHECK_STATUS(record.decode(private_data, sizeof private_data), av::Status::ok);
  CHECK(record.ticket() == 1u);

  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.pDmaBufferPrivateData = private_data;
  submit.DmaBufferPrivateDataSize = present.DmaBufferPrivateDataSize;
  submit.DmaBufferUmdPrivateDataSize = sizeof private_data;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);

  fixture.serve();
  const timespec pause = {0, 100000};
  for (std::uint32_t waited = 0u; waited < 20000u && destination.count(MappedSurface::colour) == 0u; waited++)
      nanosleep(&pause, nullptr);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
  for (std::uint32_t waited = 0u; waited < 20000u && fixture.dxgkrnl().interrupts() == before; waited++) {
      fixture.platform().set_irql(Irql::dispatch);
      fixture.adapter().dpc_routine();
      fixture.platform().set_irql(Irql::passive);
      nanosleep(&pause, nullptr);
  }
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).FenceId, 1u);
  const StateDump parked(fixture);
  CHECK(parked.holds("presents_ticketed=1 presents_parked=1"));

  const av::PresentTicketRecord forged[] = {av::PresentTicketRecord(record.epoch() + 1u, 1u),
                                            av::PresentTicketRecord(record.epoch(), 2u)};
  std::uint32_t fence = 2u;
  for (const av::PresentTicketRecord &named : forged) {
      CHECK_STATUS(named.encode(private_data, sizeof private_data), av::Status::ok);
      submit.SubmissionFenceId = fence;
      CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
      fixture.platform().set_irql(Irql::dispatch);
      fixture.adapter().dpc_routine();
      fixture.platform().set_irql(Irql::passive);
      CHECK_EQ(fixture.dxgkrnl().interrupt(fixture.dxgkrnl().interrupts() - 1u).FenceId, fence);
      fence++;
  }
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_reader_of_presented_pages_waits_for_the_present_moving_into_them)
{ Fixture fixture;
  fixture.offer_present_engine();
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  destination.paint_all(0u);
  PagesReader reader(fixture, destination.kmt_handle());

  CHECK_EQ(reader.begin(0u), success);
  CHECK_EQ(reader.writes(), 0u);
  CHECK(reader.complete());
  CHECK_EQ(reader.end(), success);
  CHECK_EQ(reader.writes(), 0u);
  CHECK(reader.complete());

  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t             dma[512] = {};
  std::uint8_t             private_data[av::PresentTicketRecord::encoded_size] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pDmaBufferPrivateData = private_data;
  present.DmaBufferPrivateDataSize = sizeof private_data;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(destination.count(MappedSurface::colour), 0u);

  CHECK_EQ(reader.begin(0u), success);
  CHECK_EQ(reader.writes(), 2u);
  CHECK(!reader.complete());
  CHECK(fixture.platform().logged_line("avgpu: a reader of presented pages waited out its time"));
  const StateDump waited(fixture);
  CHECK(waited.holds("present_waiters/0 busy=false waiting=0 waits=1 timeouts=1"));
  CHECK_EQ(reader.end(), success);
  CHECK_EQ(reader.writes(), 2u);
  CHECK(reader.complete());

  fixture.serve();
  const timespec pause = {0, 100000};
  for (std::uint32_t waited_for = 0u; waited_for < 20000u && destination.count(MappedSurface::colour) == 0u;
       waited_for++)
      nanosleep(&pause, nullptr);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  bool settled = false;
  for (std::uint32_t polled = 0u; polled < 20000u && !settled; polled++) {
      CHECK_EQ(reader.begin(0u), success);
      settled = reader.complete();
      if (!settled) nanosleep(&pause, nullptr);
  }
  CHECK(settled);
  CHECK_EQ(reader.writes(), 2u);

  PagesReader unknown(fixture, destination.kmt_handle() + 1000u);
  CHECK_EQ(unknown.begin(0u), invalid_parameter);
  CHECK_EQ(unknown.end(), invalid_parameter);
  PagesReader none(fixture, 0u);
  CHECK_EQ(none.begin(0u), invalid_parameter);
  std::uint8_t bytes[av::PagesReadRecord::encoded_size + 1u] = {};
  CHECK_STATUS(av::PagesReadRecord(av::PagesReadRecord::Action::begin, destination.kmt_handle(), 0u)
                   .encode(bytes, av::PagesReadRecord::encoded_size),
               av::Status::ok);
  CHECK_EQ(reader.send_bytes(bytes, av::PagesReadRecord::encoded_size - 1u), invalid_parameter);
  CHECK_EQ(reader.send_bytes(bytes, av::PagesReadRecord::encoded_size + 1u), invalid_parameter);
  CHECK_STATUS(av::PagesReadRecord(av::PagesReadRecord::Action::begin, destination.kmt_handle(), 0u)
                   .answered(9u, true)
                   .encode(bytes, av::PagesReadRecord::encoded_size),
               av::Status::ok);
  std::uint8_t answered[av::PagesReadRecord::encoded_size] = {};
  std::memcpy(answered, bytes, sizeof answered);
  CHECK_EQ(reader.send_bytes(bytes, av::PagesReadRecord::encoded_size), invalid_parameter);
  CHECK(std::memcmp(answered, bytes, sizeof answered) == 0);
  CHECK_EQ(handles.outstanding(), 0u);
  CHECK(fixture.adapter().invariants_hold());
  fixture.stop_presents();
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_reader_asleep_in_a_begin_is_woken_by_the_dpc_that_sees_the_engine_finish)
{ Fixture fixture;
  fixture.offer_present_engine();
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  destination.paint_all(0u);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t             dma[512] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(destination.count(MappedSurface::colour), 0u);

  struct Engine {
      Fixture       *fixture;
      MappedSurface *destination;
      std::uint32_t  sleeps;
  } engine{&fixture, &destination, 0u};
  fixture.platform().on_stall(
      [](void *context_of) {
          auto *const running = static_cast<Engine *>(context_of);
          running->sleeps++;
          running->fixture->serve();
          const timespec pause = {0, 100000};
          for (std::uint32_t waited = 0u;
               waited < 20000u && running->destination->count(MappedSurface::colour) == 0u; waited++)
              nanosleep(&pause, nullptr);
          const timespec counted = {0, 20000000};
          nanosleep(&counted, nullptr);
          running->fixture->platform().set_irql(Irql::dispatch);
          running->fixture->adapter().dpc_routine();
          running->fixture->platform().set_irql(Irql::passive);
      },
      &engine);
  PagesReader reader(fixture, destination.kmt_handle());
  CHECK_EQ(reader.begin(1000000u), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK(reader.complete());
  CHECK_EQ(reader.writes(), 2u);
  CHECK(engine.sleeps >= 1u);
  CHECK_EQ(fixture.platform().slept_ticks(), 0u);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
  const StateDump dump(fixture);
  CHECK(dump.holds("timeouts=0"));
  CHECK_EQ(handles.outstanding(), 0u);
  fixture.stop_presents();
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_present_moved_in_place_leaves_its_reader_nothing_to_wait_for)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  av::DxgkAllocationListRecord list[2] = {};
  list[0].hDeviceSpecificAllocation = source.open_handle();
  list[1].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t                     dma[512] = {};
  av::D3dddiPatchLocationListRecord patches[8] = {};
  av::DxgkargPresentRecord         present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 8u;
  present.Flags = 1u;
  present.pAllocationList = list;
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  PagesReader reader(fixture, destination.kmt_handle());
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
  const std::uint64_t slept = fixture.platform().slept_ticks();
  CHECK_EQ(reader.begin(1000000u), success);
  CHECK_EQ(reader.writes(), 2u);
  CHECK(reader.complete());
  CHECK_EQ(fixture.platform().slept_ticks(), slept);
  CHECK_EQ(reader.end(), success);
  CHECK_EQ(reader.writes(), 2u);
  const StateDump dump(fixture);
  CHECK(dump.holds("present_waiters/0 busy=false waiting=0 waits=0 timeouts=0"));
  CHECK_EQ(handles.outstanding(), 0u);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_queued_blt_with_no_room_for_its_ticket_is_counted_and_said)
{ Fixture fixture;
  fixture.offer_present_engine();
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  source.paint_all(MappedSurface::colour);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t             dma[512] = {};
  std::uint8_t             short_data[av::PresentTicketRecord::encoded_size - 1u] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unticketed private data absent bytes 0"));
  present.pDmaBuffer = dma;
  present.pDmaBufferPrivateData = short_data;
  present.DmaBufferPrivateDataSize = sizeof short_data;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unticketed private data given bytes 15"));
  CHECK(present.pDmaBufferPrivateData == short_data);
  CHECK_EQ(present.DmaBufferPrivateDataSize, sizeof short_data);

  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  present.pDmaBuffer = dma;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(fixture.platform().logged(), spent);
  const StateDump dump(fixture);
  CHECK(dump.holds("presents_unticketed=3"));
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_blt_the_host_cannot_take_is_moved_by_the_miniport)
{
  std::uint32_t payload = 0u;
  for (std::uint32_t nth = 1u; nth <= 64u && payload == 0u; nth++) {
      Fixture probe;
      probe.platform().fail_allocation_at(nth);
      const bool started = probe.start() == success;
      probe.platform().fail_allocation_at(0u);
      if (started && probe.platform().logged_line("no memory to hand presents to the host")) payload = nth;
  }
  CHECK(payload != 0u);
  for (const bool starved : {false, true}) {
      Fixture fixture;
      if (starved) fixture.platform().fail_allocation_at(payload);
      CHECK_EQ(fixture.start(), success);
      fixture.platform().fail_allocation_at(0u);
      if (starved) fixture.answer_asks();
      av::DxgkargCreateDeviceRecord device = {};
      CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
      av::DxgkargCreateContextRecord context = {};
      CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
      av::FakeHandleTable handles;
      MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
      MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::staging);
      source.paint_all(MappedSurface::colour);
      av::DxgkPresentAllocationInfoRecord info[3] = {};
      info[1].hDeviceSpecificAllocation = source.open_handle();
      info[2].hDeviceSpecificAllocation = destination.open_handle();
      std::uint8_t             dma[512] = {};
      av::DxgkargPresentRecord present = {};
      present.pDmaBuffer = dma;
      present.DmaSize = sizeof dma;
      present.Flags = 1u;
      present.pAllocationList = info;
      const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
      present.SrcRect = {0, 0, side, side};
      present.DstRect = {0, 0, side, side};
      CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
      CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
      CHECK_EQ(fixture.present_copies(), 0u);
      CHECK(fixture.platform().logged_line(starved ? "no memory to hand presents" : "would not move"));
      CHECK_EQ(fixture.platform().mapped_frames(), 0u);
      CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
      CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  } }

TEST(adapter_present, a_blt_into_a_surface_with_no_mapped_pages_says_which_side)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::gdi);
  av::DxgkBuildPagingBufferUnmapApertureSegmentRecord unmapping = {};
  unmapping.hAllocation = destination.handle();
  unmapping.SegmentId = av::ApertureSegment::segment_id;
  unmapping.NumberOfPages = 1u;
  std::uint8_t                       paging_dma[64] = {};
  av::DxgkargBuildPagingBufferRecord paging = {};
  paging.pDmaBuffer = paging_dma;
  paging.DmaSize = sizeof paging_dma;
  paging.Operation = 6;
  std::memcpy(paging.Reserved, &unmapping, sizeof unmapping);
  CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&paging)), success);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t             dma[512] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present surfaces reachable source 1 kind 2 destination 0 kind 4 from 8x8 into 8x8"));
  present.Flags = 2u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present surfaces reachable source 0 kind 0 destination 0 kind 4"));
  present.Flags = 1u;
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(fixture.platform().logged(), spent);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_colour_fill_writes_the_present_s_colour_over_its_destination)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface       destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  av::DxgkAllocationListRecord list[1] = {};
  list[0].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t                      dma[512] = {};
  av::D3dddiPatchLocationListRecord patches[8] = {};
  av::DxgkargPresentRecord          present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 8u;
  present.Flags = 2u;
  present.Color = MappedSurface::colour;
  present.pAllocationList = list;
  present.NumSrcAllocations = 0u;
  present.NumDstAllocations = 1u;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(destination.count(MappedSurface::colour), MappedSurface::side * MappedSurface::side);
  const StateDump filled(fixture);
  CHECK(filled.holds("present_rows=8"));
  CHECK(filled.holds("presents_unmoved=0"));
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);

  list[0].hDeviceSpecificAllocation = &fixture;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unmoved handles flags 0x00000002 sources 0 destinations 1"));
  present.NumDstAllocations = 0u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unmoved list flags 0x00000002 sources 0 destinations 0"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_blt_naming_a_source_it_cannot_reach_moves_nothing)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface       destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  destination.paint_all(0u);
  av::DxgkAllocationListRecord list[3] = {};
  list[0].hDeviceSpecificAllocation = &fixture;
  list[1].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t                      dma[512] = {};
  av::D3dddiPatchLocationListRecord patches[8] = {};
  av::DxgkargPresentRecord          present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 8u;
  present.Flags = 1u;
  present.pAllocationList = list;
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unmoved handles flags 0x00000001 sources 1 destinations 1"));
  present.NumSrcAllocations = 2u;
  list[2].hDeviceSpecificAllocation = destination.open_handle();
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(fixture.platform().logged_line("avgpu: present unmoved list flags 0x00000001 sources 2 destinations 1"));
  const StateDump dump(fixture);
  CHECK(dump.holds("present_rows=0"));
  CHECK(dump.holds("presents_unmoved=2"));
  CHECK_EQ(destination.count(0u), MappedSurface::side * MappedSurface::side);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_flip_with_no_allocation_list_or_dma_buffer_is_accepted_and_a_blt_without_one_refused)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint8_t             dma[128] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.Flags = 0xCu;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  present.pDmaBuffer = nullptr;
  present.DmaSize = 0u;
  present.SubRectCnt = 1u;
  const av::RectRecord nothing = {};
  present.pDstSubRects = &nothing;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK(present.pDmaBuffer == nullptr);
  CHECK(!fixture.platform().logged_line("avgpu: present refused"));
  present.DmaSize = sizeof dma;
  present.SubRectCnt = 0u;
  present.pDstSubRects = nullptr;
  present.pDmaBuffer = dma;
  present.Flags = 1u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), invalid_parameter);
  CHECK(fixture.platform().logged_line("avgpu: present refused flags 0x00000001 list absent"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_flip_an_unnamed_operation_and_a_refused_present_move_nothing)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint8_t                      dma[128] = {};
  av::D3dddiPatchLocationListRecord patches[4] = {};
  av::DxgkargPresentRecord          present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 4u;
  const std::uint32_t quiet = fixture.platform().logged();
  present.Flags = 4u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  present.Flags = 0u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), invalid_parameter);
  present.Flags = 1u;
  present.DmaSize = 0u;
  CHECK(nt(fixture.adapter().present(context.hContext, &present)) != success);
  const StateDump dump(fixture);
  CHECK(dump.holds("present_rows=0"));
  CHECK(dump.holds("presents_unmoved=0"));
  CHECK(fixture.platform().logged() > quiet);
  CHECK(!fixture.platform().logged_line("avgpu: present unmoved"));
  CHECK(fixture.platform().logged_line("avgpu: present names no operation flags 0x00000000 list absent"));
  CHECK(!fixture.platform().logged_line("avgpu: present refused"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_present_that_moves_nothing_is_counted_even_when_the_log_is_spent)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t                spent = fixture.platform().logged();
  std::uint8_t                       dma[128] = {};
  av::D3dddiPatchLocationListRecord  patches[4] = {};
  av::DxgkargPresentRecord           present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 4u;
  present.Flags = 1u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(fixture.platform().logged(), spent);
  const StateDump dump(fixture);
  CHECK(dump.holds("presents_unmoved=1"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter, the_display_ddis_are_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().is_supported_vidpn(nullptr));
  static_cast<void>(fixture.adapter().recommend_functional_vidpn(nullptr));
  static_cast<void>(fixture.adapter().enum_vidpn_cofunc_modality(nullptr));
  static_cast<void>(fixture.adapter().commit_vidpn(nullptr));
  static_cast<void>(fixture.adapter().update_active_vidpn_present_path(nullptr));
  static_cast<void>(fixture.adapter().recommend_monitor_modes(nullptr));
  static_cast<void>(fixture.adapter().recommend_vidpn_topology(nullptr));
  static_cast<void>(fixture.adapter().get_scan_line(nullptr));
  static_cast<void>(fixture.adapter().control_interrupt(0, 0u));
  static_cast<void>(fixture.adapter().set_vidpn_source_visibility(nullptr));
  static_cast<void>(fixture.adapter().present(nullptr, nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 11; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


TEST(adapter_get_node_metadata, one_3d_node_named_as_basicrender_names_it)
{ Fixture fixture;
  av::DxgkNodeMetadataRecord metadata;
  std::memset(&metadata, 0xEE, sizeof metadata);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0u, &metadata)), success);
  CHECK_EQ(metadata.EngineType, 1);
  const char expected[] = "3DNode00";
  for (unsigned index = 0u; index < sizeof expected; index++)
      CHECK_EQ(metadata.FriendlyName[index], static_cast<unsigned>(expected[index]));
  CHECK_EQ(metadata.FriendlyName[31], 0u);
  CHECK_EQ(metadata.Flags, 0u);
  CHECK_EQ(metadata.GpuMmuSupported, 1u);
  CHECK_EQ(metadata.IoMmuSupported, 0u);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(1u, &metadata)), success);
  CHECK_EQ(metadata.EngineType, 1);
  CHECK_EQ(metadata.GpuMmuSupported, 1u);
  CHECK_EQ(metadata.Flags, 0u);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(2u, &metadata)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0x10000u, &metadata)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0u, nullptr)), invalid_parameter); }

TEST(adapter_get_node_metadata, asks_the_os_before_reporting_how_the_node_is_scheduled)
{ Fixture fixture;
  fixture.dxgkrnl().allow_feature(av::feature_hwsch);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.dxgkrnl().feature_queries(), 2u);
  CHECK(fixture.platform().logged_line("avgpu: hardware scheduling status 0x00000001"));
  CHECK(fixture.platform().logged_line("avgpu: user-mode submission status 0x00000000"));
  av::DxgkNodeMetadataRecord metadata;
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0u, &metadata)), success);
  CHECK_EQ(metadata.Flags, 0u);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(1u, &metadata)), success);
  CHECK_EQ(metadata.Flags & 0x8u, 0u);
  CHECK_EQ(metadata.Flags & 0x1u, 0x1u);

  Fixture fences;
  fences.dxgkrnl().allow_feature(av::feature_hwsch);
  fences.dxgkrnl().allow_feature(av::feature_native_fence);
  CHECK_EQ(fences.start(), success);
  CHECK_EQ(fences.dxgkrnl().feature_queries(), 3u);
  CHECK(fences.platform().logged_line("avgpu: user-mode submission status 0x00000000"));
  CHECK_EQ(nt(fences.adapter().get_node_metadata(1u, &metadata)), success);
  CHECK_EQ(metadata.Flags & 0x8u, 0u);

  Fixture allowed;
  allowed.dxgkrnl().allow_feature(av::feature_hwsch);
  allowed.dxgkrnl().allow_feature(av::feature_native_fence);
  allowed.dxgkrnl().allow_feature(av::feature_user_mode_submission);
  CHECK_EQ(allowed.start(), success);
  CHECK_EQ(allowed.dxgkrnl().feature_queries(), 3u);
  CHECK(allowed.platform().logged_line("avgpu: user-mode submission status 0x00000001"));
  CHECK_EQ(nt(allowed.adapter().get_node_metadata(1u, &metadata)), success);
  CHECK_EQ(metadata.Flags & 0x9u, 0x9u);
  CHECK_EQ(metadata.Flags >> 16u, av::HwQueueSubmissions::capacity);

  Fixture refused;
  refused.dxgkrnl().allow_feature(av::feature_hwsch);
  refused.dxgkrnl().refuse_feature_queries(0xC0000001u);
  CHECK_EQ(refused.start(), success);
  CHECK_EQ(nt(refused.adapter().get_node_metadata(0u, &metadata)), success);
  CHECK_EQ(metadata.Flags, 0u);
  CHECK(refused.platform().logged_line("avgpu: hardware scheduling status 0x00000000")); }

TEST(adapter_control_interrupt2, enables_and_disables_the_vsync_reports)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 0})), success);
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 1})), success);
  fixture.blank();
  fixture.blank();
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 2})), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 3})), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, -1})), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_dpc_log, 0})), not_supported); }

TEST(adapter_calibrate_gpu_clock, the_gpu_clock_is_the_performance_counter)
{ Fixture fixture;
  fixture.platform().advance_time(4242u);
  av::DxgkGpuClockDataRecord clock;
  std::memset(&clock, 0xEE, sizeof clock);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(0u, 0u, &clock)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(clock.GpuFrequency, 10000000u);
  CHECK_EQ(clock.GpuClockCounter, fixture.platform().interrupt_time());
  CHECK_EQ(clock.CpuClockCounter, clock.GpuClockCounter);
  CHECK_EQ(clock.Flags, 0u);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(1u, 0u, &clock)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(0u, 1u, &clock)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(0u, 0u, nullptr)), invalid_parameter); }

TEST(adapter, the_wddm_1_3_ddis_declare_their_ceilings)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().set_power_pstate(0u, 0u)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().get_node_metadata(0u, nullptr));
  static_cast<void>(fixture.adapter().set_power_pstate(0u, 0u));
  static_cast<void>(fixture.adapter().control_interrupt2({interrupt_dpc_log, 0}));
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.adapter().calibrate_gpu_clock(0u, 0u, nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 4; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


TEST(adapter_reset_engine, aborts_what_the_node_was_executing_and_keeps_it_running)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  fixture.platform().set_irql(Irql::dispatch);
  for (std::uint32_t fence = 1u; fence <= 2u; fence++) {
      submit.SubmissionFenceId = fence;
      CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  }
  fixture.platform().set_irql(Irql::passive);
  av::DxgkargResetEngineRecord reset = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().reset_engine(&reset)), success);
  CHECK_EQ(reset.LastAbortedFenceId, 1u);
  submit.SubmissionFenceId = 3u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  const av::DxgkargResetEngineRecord refused[] = {{2u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargResetEngineRecord &request : refused) {
      av::DxgkargResetEngineRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().reset_engine(&copy)), invalid_parameter);
  }
  CHECK_EQ(nt(fixture.adapter().reset_engine(nullptr)), invalid_parameter); }

TEST(adapter_reset_engine, answers_what_it_threw_away_and_reports_nothing_more)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord first_device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&first_device)), success);
  av::DxgkargCreateContextRecord first = {};
  first.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(first_device.hDevice, &first)), success);
  av::DxgkargCreateDeviceRecord second_device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&second_device)), success);
  av::DxgkargCreateContextRecord second = {};
  second.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(second_device.hDevice, &second)), success);

  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = first.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  submit.SubmissionFenceId = 2u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);

  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  fixture.platform().set_irql(Irql::dispatch);
  av::DxgkargResetEngineRecord reset = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().reset_engine(&reset)), success);
  CHECK_EQ(reset.LastAbortedFenceId, 1u);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);

  submit.SubmissionFenceId = 3u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  submit.hContext = second.hContext;
  submit.SubmissionFenceId = 4u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).InterruptType, 1);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before + 1u).FenceId, 4u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(first.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(second.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(first_device.hDevice)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(second_device.hDevice)), success); }

TEST(adapter_query_engine_status, an_engine_that_stopped_moving_is_admitted)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargQueryEngineStatusRecord status = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().query_engine_status(&status)), success);
  CHECK_EQ(status.EngineStatus, 1u);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  status.EngineStatus = 0u;
  CHECK_EQ(nt(fixture.adapter().query_engine_status(&status)), success);
  CHECK_EQ(status.EngineStatus, 1u);
  fixture.platform().advance_time(100u);
  status.EngineStatus = 0u;
  CHECK_EQ(nt(fixture.adapter().query_engine_status(&status)), success);
  CHECK_EQ(status.EngineStatus, 0u);
  av::DxgkargQueryEngineStatusRecord queued = {1u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().query_engine_status(&queued)), success);
  CHECK_EQ(queued.EngineStatus, 1u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  const av::DxgkargQueryEngineStatusRecord refused[] = {{2u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargQueryEngineStatusRecord &request : refused) {
      av::DxgkargQueryEngineStatusRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().query_engine_status(&copy)), invalid_parameter);
      CHECK_EQ(copy.EngineStatus, 0u);
  }
  CHECK_EQ(nt(fixture.adapter().query_engine_status(nullptr)), invalid_parameter); }

TEST(adapter_query_dependent_engine_group, each_node_depends_on_itself)
{ Fixture fixture;
  av::DxgkargQueryDependentEngineGroupRecord group = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(&group)), success);
  CHECK_EQ(group.DependentNodeOrdinalMask, 1u);
  av::DxgkargQueryDependentEngineGroupRecord hardware = {1u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(&hardware)), success);
  CHECK_EQ(hardware.DependentNodeOrdinalMask, 2u);
  const av::DxgkargQueryDependentEngineGroupRecord refused[] = {{2u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargQueryDependentEngineGroupRecord &request : refused) {
      av::DxgkargQueryDependentEngineGroupRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(&copy)), invalid_parameter);
  }
  CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(nullptr)), invalid_parameter); }

TEST(adapter_cancel_command, succeeds_for_a_live_context_only)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::DxgkargCancelCommandRecord cancel = {};
  cancel.hContext = context.hContext;
  CHECK_EQ(nt(fixture.adapter().cancel_command(&cancel)), success);
  cancel.hContext = &fixture;
  CHECK_EQ(nt(fixture.adapter().cancel_command(&cancel)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().cancel_command(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_notify_surprise_removal, nothing_touches_the_device_afterwards)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().notify_surprise_removal(1)), success);
  fixture.function().common().delay_reset(100000u);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  const std::uint64_t before = fixture.platform().interrupt_time();
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.platform().interrupt_time(), before);
  CHECK_EQ(fixture.platform().live_allocations(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u); }

TEST(adapter_notify_surprise_removal, a_new_start_finds_the_device_present_again)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().notify_surprise_removal(0)), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(nt(fixture.adapter().notify_surprise_removal(2)), invalid_parameter); }

TEST(adapter_stop_device_and_release_post_display_ownership, stops_and_hands_over_no_frame_buffer)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkDisplayInformationRecord display;
  std::memset(&display, 0xEE, sizeof display);
  CHECK_EQ(nt(fixture.adapter().stop_device_and_release_post_display_ownership(1u, &display)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().stop_device_and_release_post_display_ownership(0u, nullptr)), invalid_parameter);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  CHECK_EQ(nt(fixture.adapter().stop_device_and_release_post_display_ownership(0u, &display)), success);
  CHECK_EQ(display.Width, 0u);
  CHECK(display.PhysicAddress == 0);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u); }

TEST(adapter_get_child_container_id, describes_the_synthetic_monitor)
{ Fixture fixture;
  av::DxgkChildContainerIdRecord container = {};
  CHECK_EQ(nt(fixture.adapter().get_child_container_id(0u, &container)), success);
  CHECK_EQ(container.EldInfoProductCode, 1u);
  CHECK_EQ(nt(fixture.adapter().get_child_container_id(1u, &container)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_child_container_id(0u, nullptr)), invalid_parameter); }

TEST(adapter, system_display_power_components_and_overlays_decline)
{ Fixture fixture;
  std::uint32_t width = 7u, height = 7u;
  std::int32_t format = 7;
  CHECK_EQ(nt(fixture.adapter().system_display_enable(0u, nullptr, &width, &height, &format)), not_supported);
  CHECK_EQ(width, 7u);
  fixture.adapter().system_display_write(nullptr, 1u, 1u, 4u, 0u, 0u);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().set_power_component_fstate(0u, 1u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().power_runtime_control_request(nullptr, nullptr, 0u, nullptr, 0u, nullptr)),
           not_supported);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(adapter, the_win8_ddis_declare_their_ceilings)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().notify_surprise_removal(9));
  static_cast<void>(fixture.adapter().stop_device_and_release_post_display_ownership(9u, nullptr));
  static_cast<void>(fixture.adapter().get_child_container_id(9u, nullptr));
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.adapter().set_power_component_fstate(0u, 0u));
  static_cast<void>(fixture.adapter().power_runtime_control_request(nullptr, nullptr, 0u, nullptr, 0u, nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 5; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

TEST(adapter, the_engine_ddis_the_timeout_path_uses_are_dispatch_level)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().query_engine_status(nullptr));
  static_cast<void>(fixture.adapter().reset_engine(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);

  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().query_dependent_engine_group(nullptr));
  static_cast<void>(fixture.adapter().cancel_command(nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 2; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


TEST(adapter_query_vidpn_hw_capability, the_driver_transforms_nothing_on_the_one_path)
{ Fixture fixture;
  int vidpn = 0;
  av::DxgkargQueryVidPnHwCapabilityRecord query = {&vidpn, 0u, 0u, 0xFFFFFFFFu, 0u};
  CHECK_EQ(nt(fixture.adapter().query_vidpn_hw_capability(&query)), success);
  CHECK_EQ(query.VidPnHWCaps, 0u); }

TEST(adapter_query_vidpn_hw_capability, another_path_or_no_vidpn_is_invalid)
{ Fixture fixture;
  int vidpn = 0;
  const av::DxgkargQueryVidPnHwCapabilityRecord refused[] = {
      {nullptr, 0u, 0u, 7u, 0u}, {&vidpn, 1u, 0u, 7u, 0u}, {&vidpn, 0u, 1u, 7u, 0u}};
  for (const av::DxgkargQueryVidPnHwCapabilityRecord &query : refused) {
      av::DxgkargQueryVidPnHwCapabilityRecord copy = query;
      CHECK_EQ(nt(fixture.adapter().query_vidpn_hw_capability(&copy)), invalid_parameter);
      CHECK_EQ(copy.VidPnHWCaps, 7u);
  }
  CHECK_EQ(nt(fixture.adapter().query_vidpn_hw_capability(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().query_vidpn_hw_capability(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


TEST(adapter_set_pointer_shape, reaches_the_device_once_started)
{ Fixture fixture;
  const std::uint8_t pixels[4] = {1, 2, 3, 0xFF};
  const av::DxgkargSetPointerShapeRecord shape = {2u, 1u, 1u, 4u, 0u, 0u, pixels, 0u, 0u};
  const av::DxgkargSetPointerPositionRecord position = {0u, 10, 20, 1u};
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), invalid_device_state);
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(&position)), invalid_device_state);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(&position)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(nullptr)), invalid_parameter);
  av::FakeVirtqueueDevice &device = fixture.control();
  CHECK_EQ(device.respond_all(0x1100u), 3u);
  const std::uint32_t quiet = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), quiet); }

TEST(adapter_set_pointer_shape, a_power_cycle_creates_the_cursor_resource_again)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::uint8_t pixels[4] = {1, 2, 3, 0xFF};
  const av::DxgkargSetPointerShapeRecord shape = {2u, 1u, 1u, 4u, 0u, 0u, pixels, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), invalid_device_state);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), success);
  const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(0u);
  av::FakeVirtqueueDevice device(fixture.platform(), queue.desc, queue.driver, queue.device, queue.size);
  CHECK_EQ(device.respond_all(0x1100u), 4u);
  CHECK_EQ(device.request_type(0u), 0x02F4u);
  CHECK_EQ(device.request_type(1u), 0x0101u);
  for (std::uint32_t i = 0u; i < device.requests(); i++) CHECK(device.request_type(i) != 0x0108u); }

TEST(adapter, the_legacy_surface_answers_not_supported)
{ Fixture fixture;
  int request = 0;
  CHECK_EQ(nt(fixture.adapter().acquire_swizzling_range(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().release_swizzling_range(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().set_palette(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().stop_capture(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().create_overlay(&request)), not_supported);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().acquire_swizzling_range(nullptr));
  static_cast<void>(fixture.adapter().release_swizzling_range(nullptr));
  static_cast<void>(fixture.adapter().set_palette(nullptr));
  static_cast<void>(fixture.adapter().stop_capture(nullptr));
  static_cast<void>(fixture.adapter().create_overlay(nullptr));
  static_cast<void>(fixture.adapter().set_pointer_shape(nullptr));
  static_cast<void>(fixture.adapter().set_pointer_position(nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 7; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


TEST(adapter, dispatch_io_request_is_not_supported)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().dispatch_io_request(0u, nullptr)), not_supported); }

TEST(adapter_notify_acpi_event, succeeds_and_asks_for_nothing)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  std::uint32_t flags = 3u;
  CHECK_EQ(adapter.notify_acpi_event(0, 0u, nullptr, &flags), 0);
  CHECK_EQ(flags, 0u); }

TEST(adapter_invariants, a_powered_or_mapped_adapter_that_is_not_started_is_detected)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  AdapterProbe probe(fixture.adapter());
  probe.set_started(false);
  CHECK(!fixture.adapter().invariants_hold());
  probe.set_started(true);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  probe.set_started(false);
  CHECK(!fixture.adapter().invariants_hold());
  probe.set_started(true); }


namespace {

bool accepted_by_dxgkrnl(std::uint32_t status)
{
    return status == success || status == not_implemented || status == invalid_parameter || status == no_memory;
}

std::uint32_t query_driver_caps_of(Adapter &adapter, DxgkDriverCapsRecord &caps)
{
    DxgkargQueryAdapterInfoRecord query = {};
    query.Type = query_driver_caps;
    query.pOutputData = &caps;
    query.OutputDataSize = sizeof caps;
    return nt(adapter.query_adapter_info(&query));
}

}

TEST(adapter_query_adapter_info, answers_driver_caps_with_wddm_3_2)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK_EQ(caps.WDDMVersion, 0x3200); }

TEST(adapter_query_adapter_info, logs_each_type_with_the_status_it_answered)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "avgpu: QueryAdapterInfo type 1 status 0x00000000") == 0);
  DxgkargQueryAdapterInfoRecord declined = {};
  declined.Type = 999;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&declined)), not_implemented);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "avgpu: QueryAdapterInfo type 999 status 0xC0000002") == 0); }

TEST(adapter_query_adapter_info, answers_the_gpu_mmu_and_physical_adapter_queries)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  std::uint32_t physical_adapter = 0u;
  av::DxgkGpuMmuCapsRecord mmu = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 13;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &mmu;
  query.OutputDataSize = sizeof mmu;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(mmu.PageTableLevelCount, 4u);
  av::DxgkQueryPageTableLevelDescInRecord level_in = {3u, 0u};
  av::DxgkPageTableLevelDescRecord level = {};
  query.Type = 14;
  query.pInputData = &level_in;
  query.InputDataSize = sizeof level_in;
  query.pOutputData = &level;
  query.OutputDataSize = sizeof level;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(level.PageTableIndexBitCount, 9u);
  av::DxgkPhysicalAdapterCapsRecord physical = {};
  query.Type = 15;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &physical;
  query.OutputDataSize = sizeof physical;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(physical.Flags, 0x2u);
  CHECK(physical.DxgkPhysicalAdapterHandle == &fixture.dxgkrnl()); }

TEST(adapter_query_adapter_info, driver_caps_do_not_depend_on_the_device_state)
{ Fixture fixture; DxgkDriverCapsRecord before = {}, started = {}, d3 = {}, stopped = {};
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), before), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), started), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), d3), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), stopped), success);
  CHECK(std::memcmp(&before, &started, sizeof before) == 0);
  CHECK(std::memcmp(&before, &d3, sizeof before) == 0);
  CHECK(std::memcmp(&before, &stopped, sizeof before) == 0);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_query_adapter_info, answers_wddm_device_caps_before_start)
{ Fixture fixture; std::uint32_t interface_version = 0x11007u; std::int32_t caps = 0;
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = query_wddm_device_caps;
  query.pInputData = &interface_version;
  query.InputDataSize = sizeof interface_version;
  query.pOutputData = &caps;
  query.OutputDataSize = sizeof caps;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(caps, 0x3200); }

TEST(adapter_query_adapter_info, answers_what_a_user_mode_submission_doorbell_is)
{ Fixture fixture; av::DxgkUserModeSubmissionCapsRecord caps = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 38;
  query.pOutputData = &caps;
  query.OutputDataSize = sizeof caps;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(caps.DoorbellSizeInBytes, 4u);
  CHECK_EQ(caps.SecondaryDoorbellSizeInBytes, 0u);
  CHECK_EQ(caps.Flags, 0u);
  query.pInputData = nullptr;
  query.InputDataSize = 0u;
  query.pOutputData = &caps;
  query.OutputDataSize = sizeof caps;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  std::uint32_t narrow = 0xEEu;
  query.pOutputData = &narrow;
  query.OutputDataSize = sizeof narrow;
  CHECK(nt(fixture.adapter().query_adapter_info(&query)) != success);
  CHECK_EQ(narrow, 0xEEu); }

TEST(adapter_query_adapter_info, answers_the_display_caps_extension)
{ Fixture fixture; std::uint32_t extension = 0xEEu;
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 16;
  query.pOutputData = &extension;
  query.OutputDataSize = sizeof extension;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(extension, 0u); }

TEST(adapter_query_adapter_info, answers_the_segment_query_once_started)
{ Fixture fixture; std::uint32_t physical_adapter = 0u; av::DxgkQuerySegmentOut4Record out = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 11;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &out;
  query.OutputDataSize = sizeof out;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), invalid_parameter);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(out.NbSegment, 1u); }

TEST(adapter_query_adapter_info, umd_private_names_the_luid_the_adapter_was_started_with)
{ Fixture fixture;
  constexpr std::size_t luid_offset = 20u;
  std::uint8_t start_info[64] = {};
  const std::uint32_t luid[2] = {0x00001234u, 0x00000005u};
  std::memcpy(start_info + luid_offset, luid, sizeof luid);
  CHECK_EQ(fixture.start(start_info), success);
  std::uint64_t words[2] = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = query_umd_private;
  query.pOutputData = words;
  query.OutputDataSize = sizeof words;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(words[0], 0x0044494C555641ull);
  CHECK_EQ(words[1], 0x0000000500001234ull);
  std::uint64_t short_output[1] = {0xEEu};
  query.pOutputData = short_output;
  query.OutputDataSize = sizeof short_output;
  CHECK(nt(fixture.adapter().query_adapter_info(&query)) != success);
  CHECK_EQ(short_output[0], 0xEEu);
  std::uint64_t three[3] = {0u, 0u, 0xEEu};
  query.pOutputData = three;
  query.OutputDataSize = sizeof three;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(three[1], 0x0000000500001234ull);
  CHECK_EQ(three[2], 0u); }

TEST(adapter_query_adapter_info, umd_private_says_an_integrated_host_gpu_is_one)
{ Fixture fixture;
  fixture.offer_integrated_gpu();
  CHECK_EQ(fixture.start(), success);
  std::uint64_t                 words[3] = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = query_umd_private;
  query.pOutputData = words;
  query.OutputDataSize = sizeof words;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(words[2], av::UmdAdapterInfo::feature_integrated_gpu); }

TEST(adapter_present, a_blt_naming_a_point_on_a_host_surface_is_queued_from_its_frame)
{ Fixture fixture;
  fixture.offer_present_snapshots();
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  std::uint64_t                 words[3] = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = query_umd_private;
  query.pOutputData = words;
  query.OutputDataSize = sizeof words;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(words[2], av::UmdAdapterInfo::feature_present_snapshots);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::surface, true);
  MappedSurface unshared(fixture, handles, device.hDevice, av::AllocationKind::shadow);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t dma[512] = {};
  std::uint8_t point[av::PresentPointRecord::encoded_size] = {};
  CHECK_STATUS(av::PresentPointRecord(0xABCDu).encode(point, sizeof point), av::Status::ok);
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPrivateDriverData = point;
  present.PrivateDriverDataSize = sizeof point;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  fixture.serve();
  CHECK_EQ(fixture.queued_presents(), 1u);
  std::uint32_t kind = 0u, named_point = 0u, source_entries = 1u;
  std::uint64_t surface = 0u;
  std::memcpy(&kind, fixture.last_queued() + 32u, sizeof kind);
  std::memcpy(&named_point, fixture.last_queued() + 36u, sizeof named_point);
  std::memcpy(&surface, fixture.last_queued() + 40u, sizeof surface);
  std::memcpy(&source_entries, fixture.last_queued() + 64u, sizeof source_entries);
  CHECK_EQ(kind, 1u);
  CHECK_EQ(named_point, 0xABCDu);
  CHECK(surface != 0u);
  CHECK_EQ(source_entries, 0u);
  av::DxgkargPresentRecord pages = present;
  pages.pDmaBuffer = dma;
  pages.pPrivateDriverData = nullptr;
  pages.PrivateDriverDataSize = 0u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &pages)), success);
  fixture.serve();
  CHECK_EQ(fixture.queued_presents(), 2u);
  std::memcpy(&kind, fixture.last_queued() + 32u, sizeof kind);
  std::memcpy(&source_entries, fixture.last_queued() + 64u, sizeof source_entries);
  CHECK_EQ(kind, 0u);
  CHECK_EQ(source_entries, 1u);
  info[1].hDeviceSpecificAllocation = unshared.open_handle();
  av::DxgkargPresentRecord unknown = present;
  unknown.pDmaBuffer = dma;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &unknown)), success);
  fixture.serve();
  CHECK_EQ(fixture.queued_presents(), 3u);
  std::memcpy(&kind, fixture.last_queued() + 32u, sizeof kind);
  std::memcpy(&named_point, fixture.last_queued() + 36u, sizeof named_point);
  CHECK_EQ(kind, 0u);
  CHECK_EQ(named_point, 0u);
  fixture.stop_presents();
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_present, a_point_named_by_escape_is_the_next_blt_s_frame)
{ Fixture fixture;
  fixture.offer_present_snapshots();
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  int                           process = 0;
  av::DxgkargCreateDeviceRecord device = {};
  device.hKmdProcess = &process;
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::FakeHandleTable handles;
  MappedSurface source(fixture, handles, device.hDevice, av::AllocationKind::surface, true);
  MappedSurface destination(fixture, handles, device.hDevice, av::AllocationKind::shared_primary);
  av::DxgkPresentAllocationInfoRecord info[3] = {};
  info[1].hDeviceSpecificAllocation = source.open_handle();
  info[2].hDeviceSpecificAllocation = destination.open_handle();
  std::uint8_t dma[512] = {};
  std::uint8_t point[av::PresentPointRecord::encoded_size] = {};
  CHECK_STATUS(av::PresentPointRecord(1u).encode(point, sizeof point), av::Status::ok);
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPrivateDriverData = point;
  present.PrivateDriverDataSize = sizeof point;
  present.Flags = 1u;
  present.pAllocationList = info;
  const std::int32_t side = static_cast<std::int32_t>(MappedSurface::side);
  present.SrcRect = {0, 0, side, side};
  present.DstRect = {0, 0, side, side};
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  fixture.serve();
  std::uint64_t surface = 0u;
  std::memcpy(&surface, fixture.last_queued() + 40u, sizeof surface);
  CHECK(surface != 0u);

  std::uint8_t named[av::NextPointRecord::encoded_size] = {};
  CHECK_STATUS(av::NextPointRecord(surface, 0x55u).encode(named, sizeof named), av::Status::ok);
  DxgkargEscapeRecord escape = {};
  escape.hKmdProcessHandle = &process;
  escape.pPrivateDriverData = named;
  escape.PrivateDriverDataSize = sizeof named;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
  av::DxgkargPresentRecord bare = present;
  bare.pDmaBuffer = dma;
  bare.pPrivateDriverData = nullptr;
  bare.PrivateDriverDataSize = 0u;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &bare)), success);
  fixture.serve();
  std::uint32_t kind = 0u, named_point = 0u;
  std::memcpy(&kind, fixture.last_queued() + 32u, sizeof kind);
  std::memcpy(&named_point, fixture.last_queued() + 36u, sizeof named_point);
  CHECK_EQ(kind, 1u);
  CHECK_EQ(named_point, 0x55u);
  const std::uint32_t queued = fixture.queued_presents();
  av::DxgkargPresentRecord again = bare;
  again.pDmaBuffer = dma;
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &again)), success);
  fixture.serve();
  CHECK_EQ(fixture.queued_presents(), queued);

  CHECK_STATUS(av::NextPointRecord(surface + 1000u, 0x55u).encode(named, sizeof named), av::Status::ok);
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  CHECK_STATUS(av::NextPointRecord(surface, 0x56u).encode(named, sizeof named), av::Status::ok);
  int stranger = 0;
  escape.hKmdProcessHandle = &stranger;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.hKmdProcessHandle = nullptr;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.hKmdProcessHandle = &process;
  escape.PrivateDriverDataSize = sizeof named - 1u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  fixture.stop_presents();
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_query_adapter_info, declines_every_other_type_without_touching_either_buffer)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::int32_t out_of_range[] = {-1, last_query_type + 1, 0x7FFFFFFF, static_cast<std::int32_t>(0x80000000u)};
  for (std::int32_t type = 0; type <= last_query_type + 4; type++) {
      const std::int32_t requested = type <= last_query_type ? type : out_of_range[type - last_query_type - 1];
      if (requested == query_umd_private || requested == query_driver_caps || requested == query_wddm_device_caps || requested == 11 ||
          (requested >= 13 && requested <= 16) || requested == 37 || requested == 38)
          continue;
      GuardedBuffer input(64u), output(64u);
      std::memset(input.data(), 0xEE, 64u);
      std::memset(output.data(), 0xEE, 64u);
      DxgkargQueryAdapterInfoRecord query = {};
      query.Type = requested;
      query.pInputData = input.data();
      query.InputDataSize = 64u;
      query.pOutputData = output.data();
      query.OutputDataSize = 64u;
      CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), not_implemented);
      bool untouched = true;
      for (std::uint32_t byte = 0u; byte < 64u; byte++)
          untouched = untouched && input.data()[byte] == 0xEEu && output.data()[byte] == 0xEEu;
      CHECK(untouched);
  } }

TEST(adapter_query_adapter_info, every_answer_is_a_status_dxgkrnl_accepts)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  for (std::int32_t type = -1; type <= last_query_type + 1; type++) {
      const std::uint32_t sizes[] = {0u, 3u, 4u, 0x24Fu, 0x250u};
      for (std::uint32_t size : sizes) {
          std::uint8_t input[0x250] = {}, output[0x250] = {};
          DxgkargQueryAdapterInfoRecord query = {};
          query.Type = type;
          query.pInputData = input;
          query.InputDataSize = size;
          query.pOutputData = output;
          query.OutputDataSize = size;
          CHECK(accepted_by_dxgkrnl(nt(fixture.adapter().query_adapter_info(&query))));
      }
  }
  CHECK(accepted_by_dxgkrnl(nt(fixture.adapter().query_adapter_info(nullptr)))); }

TEST(adapter_query_adapter_info, a_null_argument_block_or_short_driver_caps_is_invalid)
{ Fixture fixture; std::uint8_t output[0x250] = {};
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(nullptr)), invalid_parameter);
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = query_driver_caps;
  query.pOutputData = output;
  query.OutputDataSize = 0x24Fu;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), invalid_parameter);
  query.pOutputData = nullptr;
  query.OutputDataSize = 0x250u;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), invalid_parameter); }

TEST(adapter_query_adapter_info, is_passive_only)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(query_driver_caps_of(fixture.adapter(), caps));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }


TEST(adapter_escape, an_escape_of_no_record_the_driver_knows_is_not_supported)
{ Fixture fixture; std::uint8_t data[64] = {};
  DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  const std::uint32_t sizes[] = {1u, 4u, 64u};
  for (std::uint32_t size : sizes) {
      escape.PrivateDriverDataSize = size;
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), not_supported);
  } }

TEST(adapter_escape, the_trace_escape_answers_the_state_of_the_adapter_without_a_context)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  std::uint8_t data[av::TraceEscapeRecord::encoded_size + 16u * 1024u] = {};
  const av::TraceEscapeRecord request(av::TraceEscapeRecord::Kind::state, 0u, 16u * 1024u);
  CHECK_STATUS(request.encode(data, sizeof data), av::Status::ok);
  DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = sizeof data;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
  av::TraceEscapeRecord answer = request;
  CHECK_STATUS(answer.accept_answer(data, sizeof data), av::Status::ok);
  CHECK_EQ(answer.window(), answer.total());
  char dump[16u * 1024u + 1u] = {};
  std::memcpy(dump, data + av::TraceEscapeRecord::encoded_size, answer.window());
  const char *const expected =
      "adapter/0 started=true present=true powering_down=false presented_fence=0 present_rows=0"
      " presents_unmoved=0 presents_unticketed=0 presents_ticketed=0 presents_parked=0 presents_made=0 presents_refused=0 presents_unpointed=0 last_present_flags=0x0000000000000000 last_present_status=0x0000000000000000 last_present_destination_kind=0 renderkm_calls=0 renderkm_bitblt=0 renderkm_colorfill=0 renderkm_alphablend=0 renderkm_stretchblt=0 renderkm_transparentblt=0 renderkm_cleartypeblend=0 renderkm_escape=0 renderkm_unknown=0 renderkm_last_bitblt_source_kind=0 renderkm_last_bitblt_destination_kind=0 display_changed=0x0000000000000000 aperture_bytes=0x0000000200000000 host_visible=false"
      " host_visible_base=0x0000000000000000 host_visible_bytes=0x0000000000000000 vsync_enables=0 vsync_reported=0\n"
      "adapter/0/device_table/0 busy=false devices=0 opens=0\n"
      "adapter/0/allocation_table/0 busy=false live=0\n"
      "adapter/0/shared_objects/0 busy=false live=0 released=0\n"
      "adapter/0/context_table/0 busy=false live=0\n"
      "adapter/0/hw_queue_table/0 busy=false live=0\n"
      "adapter/0/execution_node/0 busy=false pending=0 last_submitted=0 last_completed=0"
      " preemption_requested=false preemption_fence=0 stopped=false progress_time=0\n"
      "adapter/0/present_waiters/0 busy=false waiting=0 waits=0 timeouts=0\n"
      "adapter/0/av_context_table/0 busy=false bound=0\n"
      "adapter/0/display_sources/0 count=1 vsync=false\n"
      "adapter/0/display_sources/0/source/0 format=0 width=0 height=0 scanout=0x0000000000000000"
      " primary=0x0000000000000000 flip=0 visible=false";
  CHECK(std::strcmp(dump, expected) == 0);
  fixture.platform().hold_elsewhere(av::LockRank::device_table);
  CHECK_STATUS(request.encode(data, sizeof data), av::Status::ok);
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
  std::memcpy(dump, data + av::TraceEscapeRecord::encoded_size, sizeof dump - 1u);
  CHECK(std::strstr(dump, "\nadapter/0/device_table/0 busy=true\nadapter/0/allocation_table/0 busy=false") != nullptr);
  fixture.platform().released_elsewhere();
  CHECK_EQ(fixture.platform().locks_held(), 0u);
  const av::TraceEscapeRecord oversized(av::TraceEscapeRecord::Kind::table, 0u, 16u * 1024u + 1u);
  CHECK_STATUS(oversized.encode(data, sizeof data), av::Status::ok);
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  const av::TraceEscapeRecord table(av::TraceEscapeRecord::Kind::table, 0u, 16u * 1024u);
  CHECK_STATUS(table.encode(data, sizeof data), av::Status::ok);
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
  answer = table;
  CHECK_STATUS(answer.accept_answer(data, sizeof data), av::Status::ok);
  CHECK_EQ(answer.total(), 0u);
  CHECK_STATUS(request.encode(data, sizeof data), av::Status::ok);
  fixture.platform().fail_allocation_at(1u);
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), 0xC0000017u); }

TEST(adapter_escape, missing_or_empty_private_data_is_invalid)
{ Fixture fixture; std::uint8_t data[4] = {};
  CHECK_EQ(nt(fixture.adapter().escape(nullptr)), invalid_parameter);
  DxgkargEscapeRecord escape = {};
  escape.PrivateDriverDataSize = 4u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = 0u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter); }

TEST(adapter_escape, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().escape(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


namespace {

std::uint32_t collect(Adapter &adapter, std::uint8_t *buffer, std::uint64_t size, DxgkargCollectDbgInfoExtRecord *ext)
{
    DxgkargCollectDbgInfoRecord record = {};
    record.pBuffer = buffer;
    record.BufferSize = size;
    record.pExtension = ext;
    return nt(adapter.collect_dbg_info(&record));
}

}

TEST(adapter_collect_dbg_info, writes_the_started_and_powered_state)
{ Fixture fixture; GuardedBuffer buffer(av::DebugReport::report_bytes);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(collect(fixture.adapter(), buffer.data(), av::DebugReport::report_bytes, nullptr), success);
  CHECK_EQ(buffer.data()[0], 0x41u);
  CHECK_EQ(buffer.data()[6], 1u);
  CHECK_EQ(buffer.data()[7], 1u);
  CHECK(buffer.data()[8] != 0u || buffer.data()[12] != 0u);
  CHECK(buffer.guards_intact()); }

TEST(adapter_collect_dbg_info, a_stopped_adapter_reports_stopped)
{ Fixture fixture; std::uint8_t buffer[av::DebugReport::report_bytes] = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(collect(fixture.adapter(), buffer, sizeof buffer, nullptr), success);
  CHECK_EQ(buffer[6], 0u);
  CHECK_EQ(buffer[7], 0u);
  CHECK_EQ(buffer[8], 0u); }

TEST(adapter_collect_dbg_info, the_report_carries_the_node_state)
{ Fixture fixture; std::uint8_t buffer[av::DebugReport::report_bytes] = {};
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 4u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(collect(fixture.adapter(), buffer, sizeof buffer, nullptr), success);
  CHECK_EQ(buffer[16], 4u);
  CHECK_EQ(buffer[20], 0u);
  CHECK_EQ(buffer[32], 1u);    }

TEST(adapter_collect_dbg_info, a_short_buffer_gets_whole_fields_only_and_nothing_past_it)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  for (std::uint64_t size = 0u; size < av::DebugReport::report_bytes; size++) {
      GuardedBuffer buffer(av::DebugReport::report_bytes);
      std::memset(buffer.data(), 0xEE, av::DebugReport::report_bytes);
      CHECK_EQ(collect(fixture.adapter(), buffer.data(), size, nullptr), success);
      CHECK_EQ(buffer.data()[av::DebugReport::report_bytes - 1u], 0xEEu);
      CHECK(buffer.guards_intact());
  } }

TEST(adapter_collect_dbg_info, zeroes_the_extension_and_rejects_a_missing_buffer)
{ Fixture fixture; DxgkargCollectDbgInfoExtRecord ext;
  std::memset(&ext, 0xEE, sizeof ext);
  CHECK_EQ(collect(fixture.adapter(), nullptr, 0u, &ext), success);
  CHECK_EQ(ext.BucketingKey, 0u);
  CHECK_EQ(ext.Reserved7, 0u);
  CHECK_EQ(collect(fixture.adapter(), nullptr, 16u, nullptr), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().collect_dbg_info(nullptr)), invalid_parameter); }

TEST(adapter_collect_dbg_info, allocates_nothing)
{ Fixture fixture; std::uint8_t buffer[64] = {};
  CHECK_EQ(fixture.start(), success);
  const std::uint32_t allocations = fixture.platform().allocations_made();
  CHECK_EQ(collect(fixture.adapter(), buffer, sizeof buffer, nullptr), success);
  CHECK_EQ(fixture.platform().allocations_made(), allocations);
  CHECK_EQ(fixture.platform().locks_held(), 0u); }

TEST(adapter_collect_dbg_info, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().collect_dbg_info(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


namespace {

class ShadowSurface {
public:
    explicit ShadowSurface(Adapter &adapter) : adapter_(adapter), surface_{64u, 32u, 21, 0u}, descriptor_{}, info_{}
    {
        av::DxgkargGetStandardAllocationDriverDataRecord describe = {};
        describe.StandardAllocationType = 2;
        describe.pCreateSurfaceData = &surface_;
        describe.pAllocationPrivateDriverData = descriptor_;
        described_ = nt(adapter_.get_standard_allocation_driver_data(&describe));
        info_.pPrivateDriverData = descriptor_;
        info_.PrivateDriverDataSize = describe.AllocationPrivateDriverDataSize;
    }

    std::uint32_t described() const { return described_; }

    std::uint32_t create()
    {
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info_;
        return nt(adapter_.create_allocation(&create));
    }

    std::uint32_t destroy()
    {
        void *handle = info_.hAllocation;
        av::DxgkargDestroyAllocationRecord destroy = {};
        destroy.NumAllocations = 1u;
        destroy.pAllocationList = &handle;
        return nt(adapter_.destroy_allocation(&destroy));
    }

    void *handle() const { return info_.hAllocation; }
    std::uint32_t pitch() const { return surface_.Pitch; }

private:
    Adapter                             &adapter_;
    av::D3dkmdtShadowSurfaceDataRecord   surface_;
    std::uint8_t                         descriptor_[av::AllocationDescriptor::encoded_size];
    av::DxgkAllocationInfoRecord         info_;
    std::uint32_t                        described_ = 0u;
};

}

TEST(adapter_create_device, answers_a_device_that_leads_back_to_the_adapter)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord request = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&request)), success);
  CHECK(&static_cast<av::Device *>(request.hDevice)->adapter() == &fixture.adapter());
  CHECK_EQ(nt(fixture.adapter().destroy_device(request.hDevice)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(request.hDevice)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().create_device(nullptr)), invalid_parameter); }

TEST(adapter_create_allocation, a_standard_surface_lives_through_open_describe_close_and_destroy)
{ Fixture fixture; av::FakeHandleTable handles;
  CHECK_EQ(fixture.start(), success);
  ShadowSurface shadow(fixture.adapter());
  CHECK_EQ(shadow.described(), success);
  CHECK_EQ(shadow.pitch(), 256u);
  CHECK_EQ(shadow.create(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkOpenAllocationInfoRecord open_info = {};
  open_info.hAllocation = handles.insert(shadow.handle());
  av::DxgkargOpenAllocationRecord open = {};
  open.NumAllocations = 1u;
  open.pOpenAllocation = &open_info;
  CHECK_EQ(nt(fixture.adapter().open_allocation(device.hDevice, &open)), success);
  CHECK_EQ(open.Pitch, 256u);
  av::DxgkargDescribeAllocationRecord describe = {};
  describe.hAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().describe_allocation(&describe)), success);
  CHECK_EQ(describe.Width, 64u);
  CHECK_EQ(shadow.destroy(), invalid_device_state);
  void *open_handle = open_info.hDeviceSpecificAllocation;
  av::DxgkargCloseAllocationRecord close = {};
  close.NumAllocations = 1u;
  close.pOpenHandleList = &open_handle;
  CHECK_EQ(nt(fixture.adapter().close_allocation(device.hDevice, &close)), success);
  CHECK_EQ(shadow.destroy(), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_create_allocation, every_allocation_ddi_refuses_a_missing_argument_block)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().create_allocation(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_allocation(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().describe_allocation(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_standard_allocation_driver_data(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().open_allocation(nullptr, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().close_allocation(nullptr, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(nullptr)), invalid_parameter); }

TEST(adapter_create_allocation, the_log_line_names_the_private_data_length)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateAllocationRecord request = {};
  request.NumAllocations = 1u;
  CHECK_EQ(nt(fixture.adapter().create_allocation(&request)), invalid_parameter);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "avgpu: CreateAllocation bytes 0 status 0xC000000D") == 0); }

TEST(adapter_create_allocation, before_start_there_is_no_segment_to_place_it_in)
{ Fixture fixture;
  ShadowSurface shadow(fixture.adapter());
  CHECK_EQ(shadow.create(), invalid_parameter); }

TEST(adapter, each_step_of_the_display_path_says_what_it_answered)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  ShadowSurface shadow(fixture.adapter());
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "avgpu: GetStandardAllocationDriverData status 0x00000000") == 0);
  CHECK_EQ(shadow.create(), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "avgpu: CreateAllocation bytes 56 status 0x00000000") == 0);
  static_assert(av::AllocationDescriptor::encoded_size == 56u, "the line above names the descriptor's size");
  av::DxgkargCreateDeviceRecord device = {};
  device.hDevice = &fixture;
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  const char *line = fixture.platform().last_log();
  CHECK(std::strncmp(line, "avgpu: CreateDevice runtime 0x", 30u) == 0);
  CHECK(std::strcmp(line + 46, " status 0x00000000") == 0);
  CHECK(std::strncmp(line + 30, "0000000000000000", 16u) != 0);
  av::DxgkargCreateDeviceRecord second = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&second)), success);
  CHECK(std::strncmp(fixture.platform().last_log() + 30, "0000000000000000", 16u) == 0);
  av::DxgkargDescribeAllocationRecord describe = {};
  describe.hAllocation = shadow.handle();
  char before_describe[128] = {};
  std::strncpy(before_describe, fixture.platform().last_log(), sizeof before_describe - 1u);
  CHECK_EQ(nt(fixture.adapter().describe_allocation(&describe)), success);
  CHECK(std::strcmp(before_describe, fixture.platform().last_log()) == 0);
  av::DxgkargDescribeAllocationRecord unknown = {};
  unknown.hAllocation = &unknown;
  CHECK(fixture.adapter().describe_allocation(&unknown) < 0);
  CHECK(std::strncmp(fixture.platform().last_log(), "avgpu: DescribeAllocation status 0xC", 36u) == 0);
  av::DxgkargSetDisplayPrivateDriverFormatRecord format = {};
  format.PrimaryAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&format)), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "avgpu: SetDisplayPrivateDriverFormat status 0x00000000") == 0);
  av::DxgkargSetVidPnSourceVisibilityRecord visibility = {};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visibility)), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "avgpu: SetVidPnSourceVisibility source 0 hidden status 0x00000000") == 0);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  CHECK_EQ(shadow.destroy(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "avgpu: StopDevice status 0x00000000") == 0); }

TEST(adapter_set_display_private_driver_format, takes_no_primary_or_one_of_ours)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  ShadowSurface shadow(fixture.adapter());
  CHECK_EQ(shadow.create(), success);
  av::DxgkargSetDisplayPrivateDriverFormatRecord request = {};
  request.PrivateDriverFormatAttribute = 5u;
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), success);
  request.PrimaryAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), success);
  request.PrimaryAllocation = &request;
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), invalid_parameter);
  request.PrimaryAllocation = nullptr;
  request.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), invalid_parameter);
  CHECK_EQ(shadow.destroy(), success); }

TEST(adapter, removal_with_devices_and_allocations_outstanding_releases_them)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  { FakeDxgkrnlAdapter dxgkrnl(platform, function);
    Adapter adapter(platform, &function);
    std::uint32_t sources = 0u, children = 0u;
    CHECK_EQ(nt(adapter.start_device(nullptr, &dxgkrnl.record(), &sources, &children)), success);
    ShadowSurface shadow(adapter);
    CHECK_EQ(shadow.create(), success);
    av::DxgkargCreateDeviceRecord device = {};
    CHECK_EQ(nt(adapter.create_device(&device)), success);
    CHECK_EQ(nt(adapter.stop_device()), success); }
  CHECK_EQ(platform.live_allocations(), 0u); }


TEST(adapter_create_context, answers_a_context_on_a_device_and_destroys_it)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  CHECK(&static_cast<av::Context *>(context.hContext)->device().adapter() == &fixture.adapter());
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_render, needs_a_live_context_then_renders)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint8_t command[16] = {1, 2, 3}, dma[16] = {};
  av::DxgkargRenderRecord render = {};
  render.pCommand = command;
  render.CommandLength = sizeof command;
  render.pDmaBuffer = dma;
  render.DmaSize = sizeof dma;
  CHECK_EQ(nt(fixture.adapter().render(&fixture, &render)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().render(context.hContext, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().render(context.hContext, &render)), success);
  CHECK_EQ(dma[2], 3u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_render_km, counts_a_gdi_context_s_commands_and_its_submission_completes_like_any_other)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  const std::uint32_t gdi = 0x2u, gdi_virtual = 0x6u;
  const std::uint32_t kinds[] = {gdi, gdi_virtual};
  for (const std::uint32_t flags : kinds) {
      av::DxgkargCreateContextRecord context = {};
      context.Flags = flags;
      CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
      CHECK_EQ(static_cast<av::Context *>(context.hContext)->flags(), flags);
      av::DxgkRenderkmCommandRecord commands[2] = {};
      commands[0].OpCode = static_cast<std::uint32_t>(av::DxgkRenderkmOperation::colorfill);
      commands[0].CommandSize = sizeof(av::DxgkRenderkmCommandRecord);
      commands[1].OpCode = static_cast<std::uint32_t>(av::DxgkRenderkmOperation::bitblt);
      commands[1].CommandSize = sizeof(av::DxgkRenderkmCommandRecord);
      std::uint8_t dma[16] = {};
      av::DxgkargRenderRecord render = {};
      render.pCommand = commands;
      render.CommandLength = sizeof commands;
      render.pDmaBuffer = dma;
      render.DmaSize = sizeof dma;
      CHECK_EQ(nt(fixture.adapter().render_km(&fixture, &render)), invalid_parameter);
      CHECK_EQ(nt(fixture.adapter().render_km(context.hContext, nullptr)), invalid_parameter);
      CHECK_EQ(nt(fixture.adapter().render_km(context.hContext, &render)), success);
      CHECK(render.pDmaBuffer == dma + 4);
      const std::uint32_t before = fixture.dxgkrnl().dpcs_queued();
      if (flags == gdi) {
          av::DxgkargSubmitCommandRecord submit = {};
          submit.SubmissionFenceId = 1u;
          fixture.platform().set_irql(Irql::dispatch);
          CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
          fixture.platform().set_irql(Irql::passive);
      } else {
          av::DxgkargSubmitCommandVirtualRecord submit = {};
          submit.hContext = context.hContext;
          submit.SubmissionFenceId = 2u;
          submit.DmaBufferVirtualAddress = 0x8000000000ull;
          CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
      }
      CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), before + 1u);
      CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  }
  char bytes[8192] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.adapter().report(writer);
  CHECK(std::strstr(writer.dump(),
                    "renderkm_calls=2 renderkm_bitblt=2 renderkm_colorfill=2 renderkm_alphablend=0") != nullptr);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_patch, patches_or_refuses_a_missing_block)
{ Fixture fixture;
  std::uint8_t dma[8] = {};
  av::DxgkargPatchRecord patch = {};
  patch.pDmaBuffer = dma;
  patch.DmaBufferSize = sizeof dma;
  CHECK_EQ(nt(fixture.adapter().patch(&patch)), success);
  CHECK_EQ(nt(fixture.adapter().patch(nullptr)), invalid_parameter); }


TEST(adapter_submit_command_virtual, queues_a_context_s_buffer_inside_its_root)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u);
  const av::DxgkargSetRootPageTableRecord root = {context.hContext, 2u, 0u, 0x4000u, 1u, 0u};
  fixture.adapter().set_root_page_table(&root);
  submit.SubmissionFenceId = 2u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  submit.DmaBufferVirtualAddress = 0x10000u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 2u);
  submit.SubmissionFenceId = 3u;
  submit.NodeOrdinal = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  submit.NodeOrdinal = 0u;
  submit.hContext = &fixture;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  av::DxgkargQueryCurrentFenceRecord fence = {};
  CHECK_EQ(nt(fixture.adapter().query_current_fence(&fence)), success);
  CHECK_EQ(fence.CurrentFence, 2u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_set_root_page_table, a_root_for_no_context_or_no_table_is_logged)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  const std::uint32_t logged = fixture.platform().logged();
  const av::DxgkargSetRootPageTableRecord good = {context.hContext, 2u, 0u, 0x4000u, 512u, 0u};
  fixture.adapter().set_root_page_table(&good);
  CHECK_EQ(fixture.platform().logged(), logged);
  const av::DxgkargSetRootPageTableRecord empty = {context.hContext, 2u, 0u, 0x4000u, 0u, 0u};
  fixture.adapter().set_root_page_table(&empty);
  const av::DxgkargSetRootPageTableRecord stranger = {&fixture, 2u, 0u, 0x4000u, 1u, 0u};
  fixture.adapter().set_root_page_table(&stranger);
  fixture.adapter().set_root_page_table(nullptr);
  CHECK_EQ(fixture.platform().logged(), logged + 3u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_get_root_page_table_size, is_eight_bytes_an_entry)
{ Fixture fixture;
  av::DxgkargGetRootPageTableSizeRecord request = {512u, 0u};
  CHECK_EQ(fixture.adapter().get_root_page_table_size(&request), 4096u);
  CHECK_EQ(fixture.adapter().get_root_page_table_size(nullptr), 0u); }

TEST(adapter_create_process, keeps_no_driver_process)
{ Fixture fixture;
  av::DxgkargCreateProcessRecord request = {};
  request.hKmdProcess = &fixture;
  CHECK_EQ(nt(fixture.adapter().create_process(&request)), success);
  CHECK(request.hKmdProcess == nullptr);
  CHECK_EQ(nt(fixture.adapter().create_process(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_process(nullptr)), success); }

TEST(adapter_wddm_2_0_process_and_root_ddis, are_passive_only)
{ Fixture fixture;
  av::DxgkargGetRootPageTableSizeRecord size = {1u, 0u};
  const av::DxgkargSetStablePowerStateRecord stable = {1};
  fixture.adapter().set_stable_power_state(&stable);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().submit_command_virtual(nullptr));
  fixture.adapter().set_root_page_table(nullptr);
  static_cast<void>(fixture.adapter().get_root_page_table_size(&size));
  static_cast<void>(fixture.adapter().create_process(nullptr));
  static_cast<void>(fixture.adapter().destroy_process(nullptr));
  fixture.adapter().set_stable_power_state(&stable);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

TEST(adapter_control_mode_behavior, answers_before_start_with_nothing_satisfied)
{ Fixture fixture;
  av::DxgkargControlModeBehaviorRecord request = {0x1u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().control_mode_behavior(&request)), success);
  CHECK_EQ(request.NotSatisfied, 0x1u);
  CHECK_EQ(nt(fixture.adapter().control_mode_behavior(nullptr)), invalid_parameter); }

TEST(adapter_update_monitor_link_info, describes_the_monitor_on_the_one_target)
{ Fixture fixture;
  av::DxgkargUpdateMonitorLinkInfoRecord link = {0u, 0x1u, 0xEEu, 0xEEu};
  CHECK_EQ(nt(fixture.adapter().update_monitor_link_info(&link)), success);
  CHECK_EQ(link.MonitorLinkInfoCapabilities, 0u);
  CHECK_EQ(link.MonitorLinkInfoUsageHints, 0x1u);
  link.VideoPresentTargetId = 1u;
  CHECK_EQ(nt(fixture.adapter().update_monitor_link_info(&link)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().update_monitor_link_info(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().update_monitor_link_info(nullptr));
  static_cast<void>(fixture.adapter().control_mode_behavior(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

TEST(adapter_hw_queue, submissions_complete_through_the_dpc_and_a_reset_drops_them)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x14u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint64_t fence = 0u;
  av::DxgkargCreateHwQueueRecord queue = {};
  queue.HwQueueProgressFenceCPUVirtualAddress = &fence;
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(context.hContext, &queue)), success);
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(context.hContext, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), invalid_parameter);
  av::DxgkargSubmitCommandToHwQueueRecord submit = {};
  submit.hHwQueue = queue.hHwQueue;
  submit.HwQueueProgressFenceId = 1u;
  submit.HwQueueProgressFenceCpuVa = &fence;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  submit.HwQueueProgressFenceId = 2u;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 2u);
  submit.DmaBufferPrivateDataSize = 4u;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), invalid_parameter);
  submit.DmaBufferPrivateDataSize = 0u;
  submit.HwQueueProgressFenceCpuVa = nullptr;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(nullptr)), invalid_parameter);
  const std::uint32_t reported = fixture.dxgkrnl().interrupts();
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fence, 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), reported + 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(reported).InterruptType, 11);
  submit.HwQueueProgressFenceId = 3u;
  submit.HwQueueProgressFenceCpuVa = &fence;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  const av::DxgkargResetHwEngineRecord reset = {0u, 0u};
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(&reset)), success);
  const av::DxgkargResetHwEngineRecord other_node = {1u, 0u};
  const av::DxgkargResetHwEngineRecord other_engine = {0u, 1u};
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(&other_node)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(&other_engine)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(nullptr)), invalid_parameter);
  submit.HwQueueProgressFenceId = 4u;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(queue.hHwQueue)), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fence, 2u);
  CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(queue.hHwQueue)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_hw_queue, a_queue_nobody_can_run_or_record_leaves_nothing_and_a_stale_handle_is_refused)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x14u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint64_t                  progress = 0u;
  av::DxgkargCreateHwQueueRecord queue = {};
  queue.HwQueueProgressFenceCPUVirtualAddress = &progress;
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(&fixture, &queue)), invalid_parameter);
  CHECK(queue.hHwQueue == nullptr);

  void *made[av::HwQueueContexts::capacity] = {};
  for (std::uint32_t index = 0u; index < av::HwQueueContexts::capacity; index++) {
      av::DxgkargCreateHwQueueRecord each = {};
      each.HwQueueProgressFenceCPUVirtualAddress = &progress;
      CHECK_EQ(nt(fixture.adapter().create_hw_queue(context.hContext, &each)), success);
      made[index] = each.hHwQueue;
  }
  av::DxgkargCreateHwQueueRecord overflow = {};
  overflow.HwQueueProgressFenceCPUVirtualAddress = &progress;
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(context.hContext, &overflow)), no_space);
  CHECK(overflow.hHwQueue == nullptr);
  CHECK(fixture.adapter().invariants_hold());

  int                                     stranger = 0;
  av::DxgkargSubmitCommandToHwQueueRecord submit = {};
  submit.HwQueueProgressFenceId = 1u;
  submit.HwQueueProgressFenceCpuVa = &progress;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), invalid_parameter);
  submit.hHwQueue = &stranger;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), invalid_parameter);
  submit.hHwQueue = made[0];
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  av::DxgkargResetEngineRecord reset = {1u, 0u, 0xEEu};
  CHECK_EQ(nt(fixture.adapter().reset_engine(&reset)), success);
  CHECK_EQ(reset.LastAbortedFenceId, 0u);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(progress, 0u);
  for (void *const each : made) CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(each)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_resume_hw_engine, resumes_the_one_engine)
{ Fixture fixture;
  const av::DxgkargResumeHwEngineRecord engine = {0u, 0u};
  const av::DxgkargResumeHwEngineRecord other_node = {1u, 0u};
  const av::DxgkargResumeHwEngineRecord other_engine = {0u, 1u};
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(&engine)), success);
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(&other_node)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(&other_engine)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().resume_hw_engine(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }


class HardwareQueue {
public:
    explicit HardwareQueue(Fixture &fixture) : fixture_(fixture)
    {
        av::DxgkargCreateDeviceRecord device = {};
        CHECK_EQ(nt(fixture_.adapter().create_device(&device)), success);
        context_.Flags = 0x14u;
        CHECK_EQ(nt(fixture_.adapter().create_context(device.hDevice, &context_)), success);
        queue_.HwQueueProgressFenceCPUVirtualAddress = &progress_;
        CHECK_EQ(nt(fixture_.adapter().create_hw_queue(context_.hContext, &queue_)), success);
    }
    HardwareQueue(const HardwareQueue &) = delete;
    HardwareQueue &operator=(const HardwareQueue &) = delete;

    void          *handle() const { return queue_.hHwQueue; }
    std::uint64_t &progress() { return progress_; }

private:
    Fixture                       &fixture_;
    av::DxgkargCreateContextRecord context_ = {};
    av::DxgkargCreateHwQueueRecord queue_ = {};
    std::uint64_t                  progress_ = 0u;
};

TEST(adapter_dpc_routine, a_refused_progress_fence_signal_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  HardwareQueue queue(fixture);
  std::uint64_t fence = 0u;
  av::DxgkargSubmitCommandToHwQueueRecord submit = {};
  submit.hHwQueue = queue.handle();
  submit.HwQueueProgressFenceId = 1u;
  submit.HwQueueProgressFenceCpuVa = &fence;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  const std::uint32_t logged = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), logged + 1u);
  const av::DxgkargResetHwEngineRecord reset = {0u, 0u};
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().reset_hw_engine(&reset));
  static_cast<void>(fixture.adapter().create_hw_queue(nullptr, nullptr));
  static_cast<void>(fixture.adapter().destroy_hw_queue(nullptr));
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.adapter().submit_command_to_hw_queue(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {} }

TEST(adapter_submit_command, queues_and_asks_for_the_dpc_that_completes_it)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_parameter);
  submit.SubmissionFenceId = 2u;
  submit.NodeOrdinal = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().submit_command(nullptr)), invalid_parameter);
  av::DxgkargPreemptCommandRecord preempt = {};
  preempt.PreemptionFenceId = 5u;
  CHECK_EQ(nt(fixture.adapter().preempt_command(&preempt)), success);
  preempt.NodeOrdinal = 3u;
  CHECK_EQ(nt(fixture.adapter().preempt_command(&preempt)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().preempt_command(nullptr)), invalid_parameter);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(fixture.dxgkrnl().dpc_notifications(), 1u);
  av::DxgkargQueryCurrentFenceRecord fence = {};
  CHECK_EQ(nt(fixture.adapter().query_current_fence(&fence)), success);
  CHECK_EQ(fence.CurrentFence, 1u);
  fence.NodeOrdinal = 1u;
  CHECK_EQ(nt(fixture.adapter().query_current_fence(&fence)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().query_current_fence(nullptr)), invalid_parameter); }

TEST(adapter_dpc_routine, a_storm_of_refusals_is_silent_once_the_window_is_spent)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  HardwareQueue hardware(fixture);
  std::uint64_t progress = 0u;
  av::DxgkargSubmitCommandToHwQueueRecord hw_submit = {};
  hw_submit.hHwQueue = hardware.handle();
  hw_submit.HwQueueProgressFenceId = 1u;
  hw_submit.HwQueueProgressFenceCpuVa = &progress;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&hw_submit)), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      CHECK_EQ(nt(fixture.adapter().recommend_vidpn_topology(&topology)), no_recommended_vidpn_topology);
  const std::uint32_t spent = fixture.platform().logged();
  CHECK_EQ(nt(fixture.adapter().recommend_vidpn_topology(&topology)), no_recommended_vidpn_topology);
  CHECK_EQ(fixture.platform().logged(), spent);
  DxgkDriverCapsRecord caps = {};
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK_EQ(fixture.platform().logged(), spent);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  CHECK_EQ(fixture.platform().logged(), spent);
  av::DxgkargCreateAllocationRecord allocation = {};
  allocation.NumAllocations = 1u;
  CHECK_EQ(nt(fixture.adapter().create_allocation(&allocation)), invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), spent);
  for (std::uint16_t index = 0u; index < 2u; index++) {
      const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(index);
      const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
      used[2] = 1u;
  }
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "virtio-gpu: a queue returned a request it was never given") == 0);
  CHECK_EQ(fixture.platform().logged(), spent + 1u); }

TEST(adapter_dpc_routine, a_refused_completion_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  const std::uint32_t logged = fixture.platform().logged();
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().logged() > logged); }

TEST(adapter_build_paging_buffer, answers_aperture_operations_only)
{ Fixture fixture;
  av::DxgkargBuildPagingBufferRecord request = {};
  request.Operation = 5;
  CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&request)), success);
  request.Operation = 1;
  CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&request)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().build_paging_buffer(nullptr)), invalid_parameter); }


TEST(adapter_reset_from_timeout, abandons_packets_resets_the_device_then_restart_brings_both_back)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.function().common().status(), 0u);
  submit.SubmissionFenceId = 2u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_device_state);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).FenceId, 2u); }

TEST(adapter_reset_from_timeout, before_start_touches_no_device)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  CHECK_EQ(fixture.function().common().status_writes(), 0u); }

TEST(adapter_restart_from_timeout, a_device_that_will_not_come_back_is_reported_and_submissions_stay_refused)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  fixture.function().common().set_queues(1u, 256u);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), not_found);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_device_state);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().live_allocations(), 2u);    }

TEST(adapter_destroy_allocation, an_empty_list_is_accepted)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const av::DxgkargDestroyAllocationRecord empty = {};
  CHECK_EQ(nt(fixture.adapter().destroy_allocation(&empty)), success); }

TEST(adapter_set_vidpn_source_address, says_where_a_source_points_while_the_window_lasts)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const Primary primary(fixture, 64u, 64u);
  av::DxgkargSetVidPnSourceAddressRecord address = {};
  address.PrimaryAddress = primary.address();
  address.PrimarySegment = av::ApertureSegment::segment_id;
  address.hAllocation = primary.handle();
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
  CHECK(std::strstr(fixture.platform().last_log(),
                    "SetVidPnSourceAddress source 0 segment 1 address 0x00000000C0000000") != nullptr);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
  CHECK_EQ(fixture.platform().logged(), spent);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), spent); }

TEST(adapter_start_device, offers_the_modes_the_device_s_display_holds)
{ Fixture fixture;
  fixture.display_info().describe(0u, 1024u, 768u);
  CHECK_EQ(fixture.start(), success);
  av::DxgkDeviceDescriptorRecord descriptor = {};
  std::uint8_t edid[av::Edid::block_bytes] = {};
  descriptor.DescriptorOffset = 0u;
  descriptor.DescriptorLength = sizeof edid;
  descriptor.DescriptorBuffer = edid;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  CHECK_EQ(edid[56] | (edid[58] >> 4u) << 8u, 1024u);
  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  const av::DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, no_pivot, 0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(&request)), success);
  CHECK_EQ(vidpn.source_modes(0u).modes(), 3u * av::DisplayMode::source_format_count);
  CHECK_EQ(commit(fixture, 1280u, 800u), invalid_source_mode); }

TEST(adapter_interrupt_routine, a_configuration_change_is_followed_by_the_next_monitor_ddi)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.display_info().describe(0u, 1600u, 900u);
  fixture.platform().on_stall(&Fixture::answer_display_info_from, &fixture);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 1u);
  fixture.platform().set_irql(Irql::passive);
  av::DxgkDeviceDescriptorRecord descriptor = {};
  std::uint8_t edid[av::Edid::block_bytes] = {};
  descriptor.DescriptorLength = sizeof edid;
  descriptor.DescriptorBuffer = edid;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(edid[56] | (edid[58] >> 4u) << 8u, 1600u);
  const std::uint32_t asked = fixture.control().requests();
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  CHECK_EQ(fixture.control().requests(), asked); }

TEST(adapter_start_device, keeps_its_modes_when_the_device_says_nothing_useful)
{ const char *const answers[] = {"none", "not display info", "no enabled scanout"};
  for (const char *const answer : answers) {
      Fixture fixture;
      fixture.answer_with(std::strcmp(answer, "none") == 0          ? Fixture::Answer::none
                          : std::strcmp(answer, "not display info") == 0 ? Fixture::Answer::wrong_type
                                                                         : Fixture::Answer::no_scanout);
      CHECK_EQ(fixture.start(), success);
      av::FakeVidPn vidpn;
      vidpn.add_path(0u, 0u);
      const av::DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, no_pivot, 0u, 0u, 0u};
      CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(&request)), success);
      CHECK_EQ(vidpn.source_modes(0u).modes(), av::DisplayModeList::standard_count * av::DisplayMode::source_format_count);
  } }

TEST(adapter_query_device_descriptor, asks_no_device_that_is_gone)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 1u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  av::DxgkDeviceDescriptorRecord descriptor = {};
  std::uint8_t edid[av::Edid::block_bytes] = {};
  descriptor.DescriptorLength = sizeof edid;
  descriptor.DescriptorBuffer = edid;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  CHECK_EQ(edid[56] | (edid[58] >> 4u) << 8u, 1280u); }

TEST(adapter_dpc_routine, a_failing_harvest_is_silent_once_the_window_is_spent)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(0u);
  const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
  used[2] = 9u;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), spent + 1u);
  CHECK(std::strstr(fixture.platform().last_log(), "harvest failed in the DPC") == nullptr); }


TEST(adapter_start_device, offers_a_source_and_a_child_for_every_display)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 2u);
  fixture.display_info().describe(1u, 1280u, 800u);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.sources(), 2u);
  CHECK_EQ(fixture.children(), 2u);
  DxgkChildDescriptorRecord relations[3];
  std::memset(relations, 0xEE, sizeof relations);
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations)), success);
  CHECK_EQ(relations[0].ChildUid, 0u);
  CHECK_EQ(relations[1].ChildUid, 1u);
  CHECK_EQ(relations[1].ChildDeviceType, 1);
  CHECK_EQ(relations[2].ChildDeviceType, static_cast<std::int32_t>(0xEEEEEEEEu));
  av::DxgkChildStatusRecord status = {};
  status.Type = 1;
  status.ChildUid = 1u;
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), success);
  CHECK_EQ(status.HotPlugConnected, 1u);
  status.ChildUid = 2u;
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), invalid_parameter);
  av::DxgkDeviceDescriptorRecord descriptor = {};
  std::uint8_t edid[av::Edid::block_bytes] = {};
  descriptor.DescriptorLength = sizeof edid;
  descriptor.DescriptorBuffer = edid;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(1u, &descriptor)), success);
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(2u, &descriptor)), invalid_parameter); }

TEST(adapter_commit_vidpn, the_second_display_scans_out_on_its_own)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 2u);
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  av::FakeVidPn vidpn;
  vidpn.add_path(1u, 1u);
  av::D3dkmdtVidPnSourceModeRecord pinned = {};
  av::DisplayMode(1024u, 768u).describe(pinned, av::DisplayMode::source_formats[0]);
  vidpn.source_modes(1u).seed(pinned, true);
  const av::DxgkargCommitVidPnRecord commit = {&vidpn, 1u, 0, nullptr, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().commit_vidpn(&commit)), success);
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {1u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visible)), success);
  const Primary primary(fixture, 1024u, 768u);
  av::DxgkargSetVidPnSourceAddressRecord address = {};
  address.VidPnSourceId = 1u;
  address.PrimaryAddress = primary.address();
  address.PrimarySegment = av::ApertureSegment::segment_id;
  address.hAllocation = primary.handle();
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&address)), success);
  fixture.platform().advance_time(display_period);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(control.respond_all(0x1100u), 5u);
  CHECK_EQ(control.request_type(5u), 0x0101u);
  CHECK_EQ(u32(control.request(5u), 24u), av::ScanoutResource::first_resource_id + 1u);
  CHECK_EQ(u32(control.request(5u), 32u), 1024u);
  CHECK_EQ(control.request_type(7u), 0x0103u);
  CHECK_EQ(u32(control.request(7u), 40u), 1u);
  CHECK_EQ(u32(control.request(7u), 44u), av::ScanoutResource::first_resource_id + 1u); }

TEST(adapter_start_device, says_when_the_displays_are_not_all_one_size)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 2u);
  fixture.display_info().describe(1u, 1024u, 768u);
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.platform().logged_line("display 1 is 1024x768, not the first display's size")); }

TEST(adapter_start_device, clamps_a_display_count_it_cannot_serve)
{ const std::uint32_t counts[] = {0u, av::DisplaySources::max_sources + 1u};
  const std::uint32_t offered[] = {1u, av::DisplaySources::max_sources};
  for (std::uint32_t index = 0u; index < 2u; index++) {
      Fixture fixture;
      fixture.function().device_config().set_u32(8u, counts[index]);
      CHECK_EQ(fixture.start(), success);
      CHECK_EQ(fixture.sources(), offered[index]);
      CHECK(fixture.platform().logged_line("the device reports"));
  } }

TEST(adapter_start_device, says_nothing_about_the_displays_once_the_window_is_spent)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, av::DisplaySources::max_sources + 1u);
  fixture.display_info().describe(2u, 800u, 600u);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.platform().logged(), spent + 1u);
  CHECK(!fixture.platform().logged_line("display 2 is"));
  CHECK(!fixture.platform().logged_line("the device reports"));
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {0u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visible)), success);
  CHECK_EQ(fixture.platform().logged(), spent + 1u); }

TEST(adapter_start_device, names_the_display_that_is_not_the_first_s_size)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 3u);
  fixture.display_info().describe(1u, 1280u, 800u);
  fixture.display_info().describe(2u, 1280u, 600u);
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.platform().logged_line("display 2 is 1280x600, not the first display's size"));
  CHECK(!fixture.platform().logged_line("display 1 is")); }

TEST(adapter_set_power_state, the_same_device_across_a_power_cycle_keeps_its_contexts)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);

  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  fixture.platform().on_stall(&Fixture::answer_display_info_from, &fixture);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);

  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_set_power_state, a_device_that_changed_refuses_its_contexts_and_keeps_the_display)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);

  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  fixture.answer_capset_of(other_gpu);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  fixture.platform().on_stall(&Fixture::answer_display_info_from, &fixture);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  CHECK(fixture.platform().logged_line("avgpu: the device under this adapter is not the one it started on"));

  av::DxgkargCreateContextRecord refused = {};
  refused.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &refused)), not_supported);
  submit.SubmissionFenceId = 2u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);

  fixture.platform().set_irql(Irql::dispatch);
  av::DxgkargResetEngineRecord reset = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().reset_engine(&reset)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(reset.LastAbortedFenceId, 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);

  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_interrupt_routine, a_device_that_exists_but_is_not_running_is_left_alone)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  AdapterProbe(fixture.adapter()).stop_device_object();
  const std::uint32_t queued = fixture.dxgkrnl().dpcs_queued();
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), queued); }

TEST(adapter_set_power_state, a_device_that_never_said_what_it_was_is_not_judged)
{ Fixture fixture;
  fixture.answer_with(Fixture::Answer::none);
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  av::DxgkargCreateContextRecord another = {};
  another.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &another)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(another.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_set_power_state, a_device_that_changed_with_no_work_queued_tells_nobody)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  fixture.answer_capset_of(other_gpu);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  fixture.platform().on_stall(&Fixture::answer_display_info_from, &fixture);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  av::DxgkargCreateContextRecord refused = {};
  refused.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &refused)), not_supported);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }


namespace {

class TransportAllocation {
public:
    TransportAllocation(Fixture &fixture, av::FakeHandleTable &handles, av::AllocationKind kind,
                        std::uint64_t bytes, std::uint64_t offset_in_pages)
        : fixture_(fixture), bytes_(bytes), block_(nullptr), frames_{}, encoded_{}, handle_(nullptr), kmt_handle_(0u)
    {
        const av::AllocationDescriptor descriptor(kind, true, av::SurfaceLayout(), bytes, av::ScanoutTarget());
        CHECK_STATUS(descriptor.encode(encoded_, sizeof encoded_), av::Status::ok);
        av::DxgkAllocationInfoRecord info = {};
        info.pPrivateDriverData = encoded_;
        info.PrivateDriverDataSize = av::AllocationDescriptor::encoded_size;
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info;
        CHECK_EQ(nt(fixture.adapter().create_allocation(&create)), success);
        handle_ = info.hAllocation;
        kmt_handle_ = handles.insert(handle_);
        block_ = static_cast<std::uint8_t *>(fixture.platform().allocate_contiguous(bytes));
        std::memset(block_, 0, bytes);
        const auto pages = static_cast<std::uint32_t>(bytes / 0x1000u);
        for (std::uint32_t index = 0u; index < pages; index++)
            frames_[index] = fixture.platform().physical_address(block_) + index * 0x1000u;
        av::DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
        mapping.hAllocation = handle_;
        mapping.SegmentId = av::ApertureSegment::segment_id;
        mapping.OffsetInPages = offset_in_pages;
        mapping.NumberOfPages = pages;
        mapping.pMdl = fixture.platform().attach_page_frames(frames_, pages);
        std::uint8_t                       dma[64] = {};
        av::DxgkargBuildPagingBufferRecord paging = {};
        paging.pDmaBuffer = dma;
        paging.DmaSize = sizeof dma;
        paging.Operation = 5;
        std::memcpy(paging.Reserved, &mapping, sizeof mapping);
        CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&paging)), success);
    }
    ~TransportAllocation() { fixture_.platform().release_contiguous(block_, bytes_); }

    std::uint32_t kmt_handle() const { return kmt_handle_; }
    void         *handle() const { return handle_; }
    std::uint8_t *bytes() const { return block_; }
    std::uint32_t destroy()
    {
        av::DxgkargDestroyAllocationRecord destroy = {};
        destroy.NumAllocations = 1u;
        destroy.pAllocationList = &handle_;
        return nt(fixture_.adapter().destroy_allocation(&destroy));
    }
    std::uint32_t unmap()
    {
        av::DxgkBuildPagingBufferUnmapApertureSegmentRecord unmapping = {};
        unmapping.hAllocation = handle_;
        unmapping.SegmentId = av::ApertureSegment::segment_id;
        unmapping.NumberOfPages = bytes_ / 0x1000u;
        std::uint8_t                       dma[64] = {};
        av::DxgkargBuildPagingBufferRecord paging = {};
        paging.pDmaBuffer = dma;
        paging.DmaSize = sizeof dma;
        paging.Operation = 6;
        std::memcpy(paging.Reserved, &unmapping, sizeof unmapping);
        return nt(fixture_.adapter().build_paging_buffer(&paging));
    }

private:
    Fixture      &fixture_;
    std::uint64_t bytes_;
    std::uint8_t *block_;
    std::uint64_t frames_[4];
    std::uint8_t  encoded_[av::AllocationDescriptor::encoded_size];
    void         *handle_;
    std::uint32_t kmt_handle_;
};

constexpr std::uint32_t reply_bytes = 4096u;

class AvProcess {
public:
    AvProcess(Fixture &fixture, av::FakeHandleTable &handles)
        : fixture_((fixture.answer_asks(), fixture)), device_{},
          ring_(fixture, handles, av::AllocationKind::context_ring, 0x1000u, 10u), record_bytes_{},
          fence_block_(0u)
    {
        CHECK_EQ(nt(fixture.adapter().create_device(&device_)), success);
        const av::AvContextRecord record(av::SubApi::vulkan, 12u, ring_.kmt_handle(), reply_bytes);
        CHECK_STATUS(record.encode(record_bytes_, sizeof record_bytes_), av::Status::ok);
    }

    std::uint32_t create_hw_queue_context(av::DxgkargCreateContextRecord &request)
    {
        request.Flags = 0x14u;
        return create(request);
    }

    void ask_for_gl(const av::GlContextOptions &options)
    {
        const av::AvContextRecord record(av::SubApi::opengl, 12u, ring_.kmt_handle(), reply_bytes, options);
        CHECK_STATUS(record.encode(record_bytes_, sizeof record_bytes_), av::Status::ok);
    }
    std::uint32_t create_context(av::DxgkargCreateContextRecord &request)
    {
        request.Flags = 0x4u;
        return create(request);
    }
    std::uint32_t submit(void *context, std::uint32_t fence_id, const av::SubmissionDescriptor &descriptor)
    {
        static_cast<void>(descriptor.encode(descriptor_bytes_, sizeof descriptor_bytes_));
        av::DxgkargSubmitCommandVirtualRecord submit = {};
        submit.hContext = context;
        submit.DmaBufferVirtualAddress = 0x8000000000ull;
        submit.DmaBufferSize = 32u;
        submit.pDmaBufferPrivateData = descriptor_bytes_;
        submit.DmaBufferPrivateDataSize = sizeof descriptor_bytes_;
        submit.SubmissionFenceId = fence_id;
        return nt(fixture_.adapter().submit_command_virtual(&submit));
    }
    void host_completes(std::uint64_t completed)
    {
        static_cast<void>(av::FencePage(completed, completed).encode(fence_page(), av::FencePage::encoded_size));
    }
    void host_faults(std::uint64_t completed, std::uint32_t code)
    {
        static_cast<void>(av::FencePage(completed, completed, code).encode(fence_page(), av::FencePage::encoded_size));
    }
    TransportAllocation &ring() { return ring_; }
    void                *device() const { return device_.hDevice; }
    std::uint8_t        *record_bytes() { return record_bytes_; }

private:
    std::uint32_t create(av::DxgkargCreateContextRecord &request)
    {
        request.pPrivateDriverData = record_bytes_;
        request.PrivateDriverDataSize = sizeof record_bytes_;
        const std::uint32_t made = fixture_.platform().page_blocks_made();
        const std::uint32_t status = nt(fixture_.adapter().create_context(device_.hDevice, &request));
        if (status != success) return status;
        fence_block_ = made + 1u;
        host_completes(0u);
        return status;
    }
    std::uint8_t *fence_page() { return fixture_.platform().page_block(fence_block_); }

    Fixture                      &fixture_;
    av::DxgkargCreateDeviceRecord device_;
    TransportAllocation           ring_;
    std::uint8_t                  record_bytes_[av::AvContextRecord::encoded_size];
    std::uint8_t                  descriptor_bytes_[av::SubmissionDescriptor::encoded_size] = {};
    std::uint32_t                 fence_block_;
};

}

TEST(adapter_av_context, a_gl_context_created_with_options_asks_the_host_for_them)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  fixture.offer_opengl();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  process.ask_for_gl(av::GlContextOptions(3u, 3u, av::GlContextOptions::Profile::core,
                                          av::GlContextOptions::forward_compatible, 0u));
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK_EQ(fixture.context_requests(), 2u);
  av::FakeVirtqueueDevice &control = fixture.control();
  const std::uint8_t *setup = nullptr;
  for (std::uint32_t index = 0u; index < control.requests(); index++)
      if (control.request_type(index) == 0x0207u) setup = control.request(index);
  CHECK(setup != nullptr);
  CHECK_EQ(setup[24], 48u);
  CHECK_EQ(setup[32 + 32], 3u);
  CHECK_EQ(setup[32 + 33], 3u);
  CHECK_EQ(setup[32 + 34], 1u);
  CHECK_EQ(setup[32 + 35], 2u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  process.ask_for_gl(av::GlContextOptions(4u, 7u, av::GlContextOptions::Profile::core, 0u, 0u));
  const std::uint32_t before = fixture.context_requests();
  av::DxgkargCreateContextRecord refused = {};
  CHECK(process.create_context(refused) != success);
  CHECK_EQ(fixture.context_requests(), before);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_av_context, a_process_s_batch_completes_when_the_host_says_it_did)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK(context.hContext != nullptr);
  CHECK_EQ(context.ContextInfoDmaBufferPrivateDataSize, 32u);
  CHECK(fixture.platform().logged_line("av context on doorbell 0 message 0 status 0x00000000"));
  CHECK_EQ(fixture.context_requests(), 2u);
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(fixture.function().doorbells().rings(0u), 1u);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  process.host_completes(0u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  process.host_completes(1u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).InterruptType, 1);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).FenceId, 1u);
  CHECK_EQ(process.submit(context.hContext, 2u, av::SubmissionDescriptor(2u, 32u, 9000u)), invalid_parameter);
  CHECK_EQ(fixture.function().doorbells().rings(0u), 1u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), 6u);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_av_context, freeing_the_page_that_names_a_context_destroys_the_host_context_first)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  const std::uint32_t requests = fixture.context_requests();
  CHECK_EQ(process.ring().destroy(), success);
  CHECK_EQ(fixture.context_requests(), requests + 4u);
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), requests + 4u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  CHECK(!fixture.platform().logged_line("av context destroyed"));
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_av_context, an_aperture_unmap_ends_no_context)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  const std::uint32_t requests = fixture.context_requests();
  CHECK_EQ(process.ring().unmap(), success);
  CHECK_EQ(fixture.context_requests(), requests);
  CHECK_EQ(fixture.platform().process_mappings(), 3u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 3u);
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(fixture.function().doorbells().rings(0u), 1u);
  CHECK_EQ(process.ring().destroy(), success);
  CHECK_EQ(fixture.context_requests(), requests + 4u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_av_context, a_record_that_cannot_run_fails_the_creation_and_leaves_nothing)
{ Fixture stock; av::FakeHandleTable handles;
  CHECK_EQ(stock.start(), success);
  { AvProcess process(stock, handles);
    av::DxgkargCreateContextRecord context = {};
    CHECK_EQ(process.create_context(context), invalid_parameter);
    CHECK(context.hContext == nullptr);
    CHECK_EQ(stock.context_requests(), 0u);
    CHECK_EQ(nt(stock.adapter().destroy_device(process.device())), success); }

  Fixture av_fixture;
  av_fixture.function().add_doorbell_region();
  CHECK_EQ(av_fixture.start(), success);
  AvProcess process(av_fixture, handles);
  const std::uint32_t nobody = 0x7777u;
  std::memcpy(process.record_bytes() + 12u, &nobody, sizeof nobody);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), invalid_parameter);
  CHECK(context.hContext == nullptr);
  CHECK_EQ(nt(av_fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_av_context, a_power_transition_abandons_the_host_contexts)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  const std::uint32_t requests = fixture.context_requests();
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), requests);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_av_context, a_device_that_is_gone_or_silent_runs_no_record)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord bad_node = {};
  bad_node.NodeOrdinal = 1u;
  CHECK_EQ(process.create_context(bad_node), invalid_parameter);
  CHECK(bad_node.hContext == nullptr);
  fixture.ignore_capset();
  av::DxgkargCreateContextRecord silent = {};
  CHECK_EQ(process.create_context(silent), invalid_parameter);
  CHECK(silent.hContext == nullptr);
  CHECK_EQ(nt(fixture.adapter().notify_surprise_removal(1)), success);
  av::DxgkargCreateContextRecord removed = {};
  CHECK_EQ(process.create_context(removed), invalid_parameter);
  CHECK_EQ(fixture.context_requests(), 0u);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_doorbell, refuses_every_doorbell_nobody_could_have)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_hw_queue_context(context), success);
  std::uint64_t                  progress = 0u;
  av::DxgkargCreateHwQueueRecord queue = {};
  queue.HwQueueProgressFenceCPUVirtualAddress = &progress;
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(context.hContext, &queue)), success);

  av::DxgkargCreateDoorbellRecord create = {};
  int                             stack = 0;
  create.hHwQueue = &stack;
  CHECK_EQ(nt(fixture.adapter().create_doorbell(&create)), invalid_parameter);
  create.hHwQueue = queue.hHwQueue;
  create.PrivateDriverDataSize = 4u;
  CHECK_EQ(nt(fixture.adapter().create_doorbell(&create)), invalid_parameter);
  create.PrivateDriverDataSize = 0u;
  create.Flags = 1u;
  CHECK_EQ(nt(fixture.adapter().create_doorbell(&create)), invalid_parameter);
  create.Flags = 0u;
  CHECK_EQ(nt(fixture.adapter().create_doorbell(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().create_doorbell(&create)), success);

  av::DxgkargConnectDoorbellRecord connect = {};
  connect.hDoorbell = create.hDoorbell;
  connect.Flags = 1u;
  CHECK_EQ(nt(fixture.adapter().connect_doorbell(&connect)), invalid_parameter);
  connect.Flags = 0u;
  CHECK_EQ(nt(fixture.adapter().connect_doorbell(nullptr)), invalid_parameter);
  connect.hDoorbell = &stack;
  CHECK_EQ(nt(fixture.adapter().connect_doorbell(&connect)), invalid_parameter);
  connect.hDoorbell = create.hDoorbell;
  CHECK_EQ(nt(fixture.adapter().connect_doorbell(&connect)), success);
  CHECK_EQ(connect.Status, static_cast<std::uint32_t>(av::DoorbellStatus::connected));
  CHECK(connect.SecondaryKernelCpuVirtualAddress == nullptr);

  av::DxgkargDisconnectDoorbellRecord disconnect = {&stack, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().disconnect_doorbell(&disconnect)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().disconnect_doorbell(nullptr)), invalid_parameter);
  const av::DxgkargDestroyDoorbellRecord destroy = {&stack};
  CHECK_EQ(nt(fixture.adapter().destroy_doorbell(&destroy)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_doorbell(nullptr)), invalid_parameter);
  const av::DxgkargNotifyWorkSubmissionRecord notify = {&stack, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().notify_work_submission(&notify)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().notify_work_submission(nullptr)), invalid_parameter);
  const av::DxgkargNotifyWorkSubmissionRecord asked = {queue.hHwQueue, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().notify_work_submission(&asked)), success);

  av::DxgkargCreateContextRecord plain = {};
  plain.Flags = 0x14u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &plain)), success);
  av::DxgkargCreateHwQueueRecord elsewhere = {};
  elsewhere.HwQueueProgressFenceCPUVirtualAddress = &progress;
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(plain.hContext, &elsewhere)), success);
  av::DxgkargCreateDoorbellRecord pageless = {};
  pageless.hHwQueue = elsewhere.hHwQueue;
  CHECK_EQ(nt(fixture.adapter().create_doorbell(&pageless)), invalid_parameter);
  CHECK(fixture.adapter().invariants_hold());
  const av::DxgkargDestroyDoorbellRecord gone = {create.hDoorbell};
  CHECK_EQ(nt(fixture.adapter().destroy_doorbell(&gone)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(queue.hHwQueue)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(elsewhere.hHwQueue)), success); }

TEST(adapter_native_fence, makes_fences_writes_what_the_os_asks_and_refuses_the_rest)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateNativeFenceRecord create = {};
  CHECK_EQ(nt(fixture.adapter().create_native_fence(&create)), success);
  CHECK(create.hGlobalNativeFence != nullptr);
  CHECK_EQ(nt(fixture.adapter().create_native_fence(nullptr)), invalid_parameter);

  std::uint64_t                            current = 0u;
  std::uint64_t                            monitored = 0u;
  void                                    *handles[] = {create.hGlobalNativeFence};
  std::uint64_t                            values[] = {21u};
  void                                    *at_current[] = {&current};
  void                                    *at_monitored[] = {&monitored};
  av::DxgkargUpdateNativeFenceValuesRecord update = {};
  update.NativeFenceArray = handles;
  update.UpdatedValueArray = values;
  update.NumFences = 1u;
  update.ValueKernelCpuVa = at_current;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().update_current_values_from_cpu(&update)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(current, 21u);
  update.ValueKernelCpuVa = at_monitored;
  values[0] = 22u;
  CHECK_EQ(nt(fixture.adapter().update_monitored_values(&update)), success);
  CHECK_EQ(monitored, 22u);
  CHECK_EQ(nt(fixture.adapter().update_monitored_values(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().update_current_values_from_cpu(nullptr)), invalid_parameter);

  int                                       outside = 0;
  void                                     *stranger[] = {&outside};
  update.NativeFenceArray = stranger;
  CHECK_EQ(nt(fixture.adapter().update_monitored_values(&update)), invalid_parameter);
  const av::DxgkargDestroyNativeFenceRecord unknown = {&outside, 0u, {}};
  CHECK_EQ(nt(fixture.adapter().destroy_native_fence(&unknown)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_native_fence(nullptr)), invalid_parameter);
  const av::DxgkargDestroyNativeFenceRecord destroy = {create.hGlobalNativeFence, 0u, {}};
  CHECK_EQ(nt(fixture.adapter().destroy_native_fence(&destroy)), success);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_query_adapter_info, answers_what_this_drivers_native_fences_need)
{ Fixture fixture;
  fixture.dxgkrnl().allow_feature(av::feature_native_fence);
  CHECK_EQ(fixture.start(), success);
  av::DxgkNativeFenceCapsRecord caps;
  std::memset(&caps, 0xEE, sizeof caps);
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 37;
  query.pOutputData = &caps;
  query.OutputDataSize = sizeof caps;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(caps.MonitoredValueStride, sizeof(std::uint64_t));
  CHECK_EQ(caps.MapToGpuSystemProcess, 0u);
  CHECK_EQ(caps.MinimumAddress, 0u);
  CHECK_EQ(caps.MaximumAddress, 0u);
  for (const std::uint8_t byte : caps.Reserved) CHECK_EQ(byte, 0u);
  query.OutputDataSize = sizeof caps - 1u;
  CHECK(nt(fixture.adapter().query_adapter_info(&query)) != success); }

TEST(adapter_query_interface, answers_the_feature_interface_and_what_this_driver_implements)
{ Fixture fixture;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  static const std::uint8_t feature_guid[16] = {0x93, 0x39, 0xbb, 0x94, 0xc3, 0xc6, 0xa7, 0x4d,
                                                0x89, 0x49, 0xa1, 0x13, 0x82, 0x32, 0xe7, 0x59};
  av::DxgkddiFeatureInterfaceRecord table = {};
  av::QueryInterfaceRecord          query = {};
  query.InterfaceType = feature_guid;
  query.Size = sizeof table;
  query.Version = av::feature_interface_version_1;
  query.Interface = &table;
  CHECK_EQ(nt(fixture.adapter().query_interface(&query)), success);
  CHECK_EQ(table.Version, av::feature_interface_version_1);
  CHECK(table.Context == &fixture.adapter());
  CHECK(table.QueryFeatureSupport != nullptr);
  CHECK(table.InterfaceReference != nullptr && table.InterfaceDereference != nullptr);
  table.InterfaceReference(table.Context);
  table.InterfaceDereference(table.Context);

  const std::uint32_t implemented[] = {av::feature_hwsch, av::feature_native_fence,
                                       av::feature_user_mode_submission};
  for (const std::uint32_t feature : implemented) {
      av::DxgkargQueryFeatureSupportRecord support = {};
      support.FeatureId = feature;
      CHECK_EQ(table.QueryFeatureSupport(table.Context, &support), success);
      CHECK_EQ(support.SupportedByDriver, 1u);
      CHECK_EQ(support.SupportedOnCurrentConfig, 1u);
      CHECK_EQ(support.MinSupportedVersion, av::feature_version_lowest);
      CHECK_EQ(support.MaxSupportedVersion, av::feature_version_highest);
  }
  const std::uint32_t others[] = {1u, 2u, 3u, 5u, 0xFFFFu};
  for (const std::uint32_t feature : others) {
      av::DxgkargQueryFeatureSupportRecord support = {};
      support.FeatureId = feature;
      CHECK_EQ(table.QueryFeatureSupport(table.Context, &support), success);
      CHECK_EQ(support.SupportedByDriver, 0u);
      CHECK_EQ(support.SupportedOnCurrentConfig, 0u);
  }
  CHECK_EQ(nt(fixture.adapter().query_feature_support(nullptr)), invalid_parameter); }

TEST(adapter_query_interface, refuses_an_interface_nobody_here_implements)
{ Fixture fixture;
  static const std::uint8_t feature_guid[16] = {0x93, 0x39, 0xbb, 0x94, 0xc3, 0xc6, 0xa7, 0x4d,
                                                0x89, 0x49, 0xa1, 0x13, 0x82, 0x32, 0xe7, 0x59};
  static const std::uint8_t other_guid[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                              0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};
  av::DxgkddiFeatureInterfaceRecord table = {};
  av::QueryInterfaceRecord          query = {};
  query.InterfaceType = other_guid;
  query.Size = sizeof table;
  query.Version = av::feature_interface_version_1;
  query.Interface = &table;
  CHECK_EQ(nt(fixture.adapter().query_interface(&query)), not_supported);
  query.InterfaceType = feature_guid;
  query.Version = av::feature_interface_version_1 + 1u;
  CHECK_EQ(nt(fixture.adapter().query_interface(&query)), not_supported);
  query.Version = av::feature_interface_version_1;
  query.Size = sizeof table - 1u;
  CHECK_EQ(nt(fixture.adapter().query_interface(&query)), not_supported);
  query.Size = sizeof table;
  query.Interface = nullptr;
  CHECK_EQ(nt(fixture.adapter().query_interface(&query)), invalid_parameter);
  query.Interface = &table;
  query.InterfaceType = nullptr;
  CHECK_EQ(nt(fixture.adapter().query_interface(&query)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().query_interface(nullptr)), invalid_parameter);

  query.InterfaceType = feature_guid;
  CHECK_EQ(nt(fixture.adapter().query_interface(&query)), success);
  av::DxgkargQueryFeatureSupportRecord support = {};
  support.FeatureId = av::feature_user_mode_submission;
  CHECK_EQ(table.QueryFeatureSupport(table.Context, &support), success);
  CHECK_EQ(support.SupportedByDriver, 1u);
  CHECK_EQ(support.SupportedOnCurrentConfig, 1u); }

TEST(adapter_start_device, a_second_start_without_a_stop_is_refused)
{ Fixture fixture;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.start() != success);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_start_device, doorbell_pages_that_cannot_be_mapped_fail_the_start)
{ Fixture fixture;
  fixture.function().add_doorbell_region();
  fixture.dxgkrnl().refuse_map(2u, 0xC000009Au);
  CHECK_EQ(fixture.start(), no_space);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_av_context, a_host_fault_completes_the_waiting_submission_and_refuses_the_next)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  av::DxgkargCreateContextRecord neighbour = {};
  neighbour.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &neighbour)), success);
  CHECK_EQ(process.submit(context.hContext, 7u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(process.submit(context.hContext, 8u, av::SubmissionDescriptor(2u, 32u, 64u)), success);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  const std::uint32_t rings = fixture.function().doorbells().rings(0u);
  process.host_faults(0u, 6u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).InterruptType, 1);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).FenceId, 7u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before + 1u).InterruptType, 1);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before + 1u).FenceId, 8u);
  CHECK_EQ(process.submit(context.hContext, 9u, av::SubmissionDescriptor(3u, 64u, 96u)), invalid_parameter);
  CHECK_EQ(fixture.function().doorbells().rings(0u), rings);

  av::DxgkargSubmitCommandVirtualRecord plain = {};
  plain.hContext = neighbour.hContext;
  plain.DmaBufferVirtualAddress = 0x8000000000ull;
  plain.SubmissionFenceId = 10u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&plain)), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 3u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before + 2u).FenceId, 10u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(neighbour.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_av_context, a_timeout_reset_loses_the_host_context_and_a_new_one_sets_up_after_restart)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  fixture.forget_control();
  CHECK_EQ(process.submit(context.hContext, 2u, av::SubmissionDescriptor(2u, 32u, 64u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  const std::uint32_t requests = fixture.context_requests();
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), requests);
  av::DxgkargCreateContextRecord again = {};
  CHECK_EQ(process.create_context(again), success);
  CHECK_EQ(fixture.context_requests(), requests + 2u);
  CHECK_EQ(process.submit(again.hContext, 3u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(again.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_escape, a_wake_event_is_taken_only_from_a_device_that_counts_raises)
{ for (const bool raises : {false, true}) {
      Fixture fixture; av::FakeHandleTable handles;
      fixture.function().add_doorbell_region();
      if (raises) fixture.offer_raises();
      CHECK_EQ(fixture.start(), success);
      AvProcess process(fixture, handles);
      av::DxgkargCreateContextRecord context = {};
      CHECK_EQ(process.create_context(context), success);
      const std::uint64_t event = fixture.platform().make_process_event();
      std::uint8_t data[av::WakeEventRecord::encoded_size] = {};
      CHECK_STATUS(av::WakeEventRecord(event).encode(data, sizeof data), av::Status::ok);
      DxgkargEscapeRecord escape = {};
      escape.pPrivateDriverData = data;
      escape.PrivateDriverDataSize = sizeof data;
      escape.hContext = context.hContext;
      if (!raises) {
          CHECK_EQ(nt(fixture.adapter().escape(&escape)), not_supported);
          CHECK_EQ(fixture.platform().event_references(event), 0u);
          CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
          continue;
      }
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
      CHECK_EQ(fixture.platform().event_references(event), 1u);
      CHECK_STATUS(av::WakeEventRecord(event + 4u).encode(data, sizeof data), av::Status::ok);
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
      CHECK_STATUS(av::WakeEventRecord(event).encode(data, sizeof data), av::Status::ok);
      int stranger = 0;
      escape.hContext = &stranger;
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
      escape.hContext = nullptr;
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
      escape.hContext = context.hContext;
      escape.PrivateDriverDataSize = sizeof data - 1u;
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
      CHECK_EQ(fixture.platform().event_references(event), 1u);
      CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
      CHECK_EQ(fixture.platform().event_references(event), 0u);
      CHECK_EQ(fixture.platform().event_signals(event), 1u);
  } }

TEST(adapter_escape, a_context_s_views_are_answered_in_the_private_data)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK_EQ(fixture.platform().process_mappings(), 3u);
  CHECK_EQ(fixture.platform().read_only_process_mappings(), 1u);
  std::uint8_t data[av::ContextViewsRecord::encoded_size] = {};
  CHECK_STATUS(av::ContextViewsRecord().encode(data, sizeof data), av::Status::ok);
  DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = sizeof data;
  escape.hContext = context.hContext;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
  av::ContextViewsRecord views;
  CHECK_STATUS(views.accept_answer(data, sizeof data), av::Status::ok);
  av::RingHeader header;
  CHECK_STATUS(header.decode(reinterpret_cast<const std::uint8_t *>(static_cast<std::uintptr_t>(views.ring())),
                             av::RingHeader::encoded_size),
               av::Status::ok);
  CHECK_EQ(header.capacity(), std::uint64_t{1} << 12u);
  auto *const reply = reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(views.reply()));
  reply[reply_bytes - 1u] = 0x42u;
  av::FencePage page;
  CHECK_STATUS(page.decode(reinterpret_cast<const std::uint8_t *>(static_cast<std::uintptr_t>(views.fence())),
                           av::FencePage::encoded_size),
               av::Status::ok);
  CHECK_EQ(page.completed_fence(), 0u);
  std::uint8_t first[av::ContextViewsRecord::encoded_size] = {};
  const av::ContextViewsRecord older(av::ContextViewsRecord::without_fence);
  CHECK_STATUS(older.encode(first, sizeof first), av::Status::ok);
  DxgkargEscapeRecord ask_first = escape;
  ask_first.pPrivateDriverData = first;
  CHECK_EQ(nt(fixture.adapter().escape(&ask_first)), success);
  av::ContextViewsRecord first_views(av::ContextViewsRecord::without_fence);
  CHECK_STATUS(first_views.accept_answer(first, sizeof first), av::Status::ok);
  CHECK_EQ(first_views.ring(), views.ring());
  CHECK_EQ(first_views.fence(), 0u);

  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  CHECK_STATUS(av::ContextViewsRecord().encode(data, sizeof data), av::Status::ok);
  int stranger = 0;
  escape.hContext = &stranger;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.hContext = nullptr;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.hContext = context.hContext;
  escape.PrivateDriverDataSize = sizeof data - 1u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.PrivateDriverDataSize = 3u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), not_supported);
  std::uint64_t ring = 0u;
  std::memcpy(&ring, data + 8u, sizeof ring);
  CHECK_EQ(ring, 0u);

  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }


namespace {

std::uint32_t reported_segments(Fixture &fixture)
{
    std::uint32_t                  physical_adapter = 0u;
    av::DxgkQuerySegmentOut4Record out = {};
    DxgkargQueryAdapterInfoRecord  query = {};
    query.Type = 11;
    query.pInputData = &physical_adapter;
    query.InputDataSize = sizeof physical_adapter;
    query.pOutputData = &out;
    query.OutputDataSize = sizeof out;
    CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
    return out.NbSegment;
}

std::uint32_t window_escape(Fixture &fixture, void *context, const av::HostWindowRecord &record,
                            std::uint8_t (&data)[av::HostWindowRecord::encoded_size])
{
    CHECK_STATUS(record.encode(data, sizeof data), av::Status::ok);
    DxgkargEscapeRecord escape = {};
    escape.pPrivateDriverData = data;
    escape.PrivateDriverDataSize = sizeof data;
    escape.hContext = context;
    return nt(fixture.adapter().escape(&escape));
}

using WindowOperation = av::HostWindowRecord::Operation;

constexpr std::uint32_t first_chunk_resource = av::HostBlobId::first;
const av::HostMemoryRequest upload(av::HostMemoryRequest::Kind::upload, 0u);

}

TEST(adapter_start_device, offers_a_host_window_and_reports_only_the_aperture)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  CHECK(AdapterProbe(fixture.adapter()).window_offered());
  CHECK(fixture.platform().logged_line("avgpu: host window status 0x00000000"));
  CHECK_EQ(reported_segments(fixture), 1u);
  CHECK_EQ(fixture.blob_maps(), 0u); }

TEST(adapter_start_device, a_device_with_no_region_has_no_window)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(!AdapterProbe(fixture.adapter()).window_offered());
  CHECK(!fixture.platform().logged_line("avgpu: host window"));
  CHECK_EQ(reported_segments(fixture), 1u); }

TEST(adapter_escape, a_window_chunk_opened_by_escape_answers_its_resource_and_address)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  std::uint8_t data[av::HostWindowRecord::encoded_size] = {};
  const av::HostWindowRecord open(WindowOperation::open, 0x2800u, 0u, upload);
  CHECK_EQ(window_escape(fixture, context.hContext, open, data), success);
  av::HostWindowRecord answer = open;
  CHECK_STATUS(answer.accept_answer(data, sizeof data), av::Status::ok);
  CHECK_EQ(answer.resource_id(), first_chunk_resource);
  CHECK_EQ(answer.bytes(), 0x3000u);
  CHECK(fixture.platform().maps_into_process(answer.address(), host_region_address, 0x3000u));
  CHECK_EQ(fixture.blob_maps(), 1u);
  const std::uint8_t *created = nullptr;
  for (std::uint32_t index = 0u; index < fixture.control().requests(); index++)
      if (fixture.control().request_type(index) == create_blob) created = fixture.control().request(index);
  CHECK(created != nullptr);
  std::uint32_t resource = 0u, blob_mem = 0u;
  std::uint64_t blob_id = 0u;
  std::memcpy(&resource, created + 24u, sizeof resource);
  std::memcpy(&blob_mem, created + 28u, sizeof blob_mem);
  std::memcpy(&blob_id, created + 40u, sizeof blob_id);
  CHECK_EQ(resource, first_chunk_resource);
  CHECK_EQ(blob_mem, 2u);
  CHECK(blob_id == upload.encoded());

  DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = sizeof data;
  escape.hContext = context.hContext;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  CHECK_EQ(window_escape(fixture, nullptr, open, data), invalid_parameter);
  int stranger = 0;
  CHECK_EQ(window_escape(fixture, &stranger, open, data), invalid_parameter);
  CHECK_STATUS(open.encode(data, sizeof data), av::Status::ok);
  escape.PrivateDriverDataSize = sizeof data - 1u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  CHECK_EQ(fixture.blob_maps(), 1u);

  const av::HostWindowRecord close(WindowOperation::close, 0u, answer.resource_id(), av::HostMemoryRequest());
  CHECK_EQ(window_escape(fixture, context.hContext, close, data), success);
  av::HostWindowRecord closed = close;
  CHECK_STATUS(closed.accept_answer(data, sizeof data), av::Status::ok);
  CHECK_EQ(fixture.blob_unmaps(), 1u);
  CHECK_EQ(fixture.platform().process_mappings(), 3u);
  CHECK_EQ(window_escape(fixture, context.hContext, close, data), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_escape, a_chunk_with_no_window_is_not_supported)
{ Fixture bare; av::FakeHandleTable handles;
  bare.function().add_doorbell_region();
  CHECK_EQ(bare.start(), success);
  AvProcess process(bare, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  const std::uint32_t blocks = bare.platform().owned_page_blocks();
  const std::uint32_t requests = bare.context_requests();
  std::uint8_t data[av::HostWindowRecord::encoded_size] = {};
  const av::HostWindowRecord open(WindowOperation::open, 0x1800u, 0u, upload);
  CHECK_EQ(window_escape(bare, context.hContext, open, data), not_supported);
  CHECK_EQ(bare.context_requests(), requests);
  CHECK_EQ(bare.blob_maps(), 0u);
  CHECK_EQ(bare.platform().process_mappings(), 3u);
  CHECK_EQ(bare.platform().owned_page_blocks(), blocks);
  CHECK_EQ(nt(bare.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(bare.adapter().destroy_device(process.device())), success); }

TEST(adapter_escape, a_chunk_the_device_will_back_nowhere_is_not_supported)
{ Fixture refusing; av::FakeHandleTable refusing_handles;
  refusing.function().add_doorbell_region();
  refusing.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(refusing.start(), success);
  AvProcess process(refusing, refusing_handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  refusing.refuse_the_blob();
  std::uint8_t data[av::HostWindowRecord::encoded_size] = {};
  CHECK_EQ(window_escape(refusing, context.hContext, av::HostWindowRecord(WindowOperation::open, 0x1000u, 0u, upload), data),
           not_supported);
  CHECK_EQ(refusing.platform().process_mappings(), 3u);
  CHECK_EQ(refusing.platform().owned_page_blocks(), 3u);
  CHECK_EQ(nt(refusing.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(refusing.adapter().destroy_device(process.device())), success); }

TEST(adapter_escape, a_window_chunk_with_no_device_is_invalid)
{ Fixture fixture;
  std::uint8_t data[av::HostWindowRecord::encoded_size] = {};
  int context = 0;
  CHECK_EQ(window_escape(fixture, &context, av::HostWindowRecord(WindowOperation::open, 0x1000u, 0u, upload), data),
           invalid_parameter); }

TEST(adapter_escape, destroying_a_context_closes_its_window_chunks)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  std::uint8_t data[av::HostWindowRecord::encoded_size] = {};
  CHECK_EQ(window_escape(fixture, context.hContext, av::HostWindowRecord(WindowOperation::open, 0x1000u, 0u, upload), data),
           success);
  CHECK_EQ(fixture.platform().process_mappings(), 4u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.blob_unmaps(), 1u);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_set_power_state, the_same_device_at_d0_keeps_the_window_and_frees_what_d3_abandoned)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  std::uint8_t data[av::HostWindowRecord::encoded_size] = {};
  CHECK_EQ(window_escape(fixture, context.hContext, av::HostWindowRecord(WindowOperation::open, 0x1000u, 0u, upload), data),
           success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK(AdapterProbe(fixture.adapter()).window_offered());
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.blob_unmaps(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  av::DxgkargCreateContextRecord again = {};
  CHECK_EQ(process.create_context(again), success);
  const av::HostWindowRecord open(WindowOperation::open, 0x1000u, 0u, upload);
  CHECK_EQ(window_escape(fixture, again.hContext, open, data), success);
  av::HostWindowRecord answer = open;
  CHECK_STATUS(answer.accept_answer(data, sizeof data), av::Status::ok);
  CHECK(fixture.platform().maps_into_process(answer.address(), host_region_address, 0x1000u));
  CHECK_EQ(nt(fixture.adapter().destroy_context(again.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success);
  fixture.platform().on_stall(nullptr, nullptr); }

TEST(adapter_set_power_state, a_device_that_changed_or_went_quiet_withdraws_the_window)
{ const bool changed[] = {true, false};
  for (bool other : changed) {
      Fixture fixture;
      fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
      CHECK_EQ(fixture.start(), success);
      CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
      fixture.forget_control();
      if (other) fixture.answer_capset_of(other_gpu);
      if (!other) fixture.ignore_capset();
      fixture.answer_asks();
      CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
      fixture.platform().on_stall(nullptr, nullptr);
      CHECK(!AdapterProbe(fixture.adapter()).window_offered());
      CHECK_EQ(reported_segments(fixture), 1u);
  } }

TEST(adapter_stop_device, withdraws_the_window_until_the_next_start)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  fixture.forget_control();
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK(AdapterProbe(fixture.adapter()).window_offered());
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK(!AdapterProbe(fixture.adapter()).window_offered());
  CHECK_EQ(fixture.start(), success);
  CHECK(AdapterProbe(fixture.adapter()).window_offered()); }


namespace {

std::uint32_t submit_packet(Fixture &fixture, void *context, std::uint32_t fence_id, std::uint8_t *bytes,
                            std::uint32_t length)
{
    av::DxgkargSubmitCommandVirtualRecord submit = {};
    submit.hContext = context;
    submit.DmaBufferVirtualAddress = 0x8000000000ull;
    submit.pDmaBufferPrivateData = bytes;
    submit.DmaBufferPrivateDataSize = length;
    submit.SubmissionFenceId = fence_id;
    return nt(fixture.adapter().submit_command_virtual(&submit));
}

void run_dpc(Fixture &fixture)
{
    fixture.platform().set_irql(Irql::dispatch);
    fixture.adapter().dpc_routine();
    fixture.platform().set_irql(Irql::passive);
}

}

TEST(adapter_escape, a_tail_publish_names_the_ring_of_the_context_it_is_sent_on)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  av::DxgkargCreateContextRecord plain = {};
  plain.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &plain)), success);
  std::uint8_t data[av::TailPublishRecord::encoded_size] = {};
  av::DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = sizeof data;
  CHECK_STATUS(av::TailPublishRecord(process.ring().kmt_handle() + 1u).encode(data, sizeof data), av::Status::ok);
  escape.hContext = context.hContext;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  CHECK_STATUS(av::TailPublishRecord(process.ring().kmt_handle()).encode(data, sizeof data), av::Status::ok);
  escape.hContext = plain.hContext;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.hContext = nullptr;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.hContext = context.hContext;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_context(plain.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success); }

TEST(adapter_submit_command_virtual, a_follow_record_holds_the_packet_until_the_host_completes_the_batch)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  av::DxgkargCreateContextRecord queue = {};
  queue.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &queue)), success);
  CHECK_EQ(queue.ContextInfoDmaBufferPrivateDataSize, av::ReleaseRecord::encoded_size);
  CHECK_EQ(process.submit(context.hContext, 7u, av::SubmissionDescriptor(1u, 0u, 32u)), success);

  std::uint8_t follow[av::FollowRecord::encoded_size] = {};
  CHECK_STATUS(av::FollowRecord(process.ring().kmt_handle(), 1u).encode(follow, sizeof follow), av::Status::ok);
  const std::uint32_t rings = fixture.function().doorbells().rings(0u);
  const std::uint32_t dpcs = fixture.dxgkrnl().dpcs_queued();
  CHECK_EQ(submit_packet(fixture, queue.hContext, 8u, follow, sizeof follow), success);
  CHECK_EQ(fixture.function().doorbells().rings(0u), rings + 1u);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), dpcs);

  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  run_dpc(fixture);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  process.host_completes(1u);
  run_dpc(fixture);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).FenceId, 7u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before + 1u).FenceId, 8u);

  CHECK_EQ(submit_packet(fixture, queue.hContext, 9u, follow, sizeof follow), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), dpcs + 1u);
  run_dpc(fixture);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 3u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before + 2u).FenceId, 9u);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_context(queue.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_submit_command_virtual, a_follow_record_of_no_ring_is_refused_and_other_data_is_ordinary)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  av::DxgkargCreateContextRecord queue = {};
  queue.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &queue)), success);

  std::uint8_t follow[av::FollowRecord::encoded_size] = {};
  CHECK_STATUS(av::FollowRecord(process.ring().kmt_handle() + 0x100u, 1u).encode(follow, sizeof follow),
               av::Status::ok);
  const std::uint32_t rings = fixture.function().doorbells().rings(0u);
  CHECK_EQ(submit_packet(fixture, queue.hContext, 5u, follow, sizeof follow), invalid_parameter);
  CHECK_EQ(fixture.function().doorbells().rings(0u), rings);

  std::uint8_t other[av::FollowRecord::encoded_size] = {};
  const std::uint32_t dpcs = fixture.dxgkrnl().dpcs_queued();
  CHECK_EQ(submit_packet(fixture, queue.hContext, 5u, other, sizeof other), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), dpcs + 1u);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  run_dpc(fixture);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).FenceId, 5u);
  CHECK_EQ(fixture.function().doorbells().rings(0u), rings);
  CHECK_EQ(nt(fixture.adapter().destroy_context(queue.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_dpc_routine, lets_a_follower_go_once_it_has_waited_the_deadline)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  av::DxgkargCreateContextRecord queue = {};
  queue.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &queue)), success);
  CHECK_EQ(process.submit(context.hContext, 7u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  std::uint8_t follow[av::FollowRecord::encoded_size] = {};
  CHECK_STATUS(av::FollowRecord(process.ring().kmt_handle(), 1u).encode(follow, sizeof follow), av::Status::ok);
  CHECK_EQ(submit_packet(fixture, queue.hContext, 8u, follow, sizeof follow), success);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  fixture.platform().advance_time(av::ExecutionNode::park_deadline - 1u);
  run_dpc(fixture);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  CHECK(!fixture.platform().logged_line("a held packet passed its deadline"));
  fixture.platform().advance_time(1u);
  run_dpc(fixture);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before + 1u).FenceId, 8u);
  CHECK(fixture.platform().logged_line("avgpu: a held packet passed its deadline and was let go"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(queue.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }


namespace {

av::DisplayModeSet host_modes(std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> modes,
                              std::uint32_t preferred)
{
    av::DisplayModeSet set;
    for (const auto &mode : modes) CHECK_STATUS(set.add(mode.first, mode.second), av::Status::ok);
    CHECK_STATUS(set.prefer(preferred), av::Status::ok);
    return set;
}

std::uint32_t edid_preferred_width(Fixture &fixture)
{
    av::DxgkDeviceDescriptorRecord descriptor = {};
    std::uint8_t                   edid[av::Edid::block_bytes] = {};
    descriptor.DescriptorLength = sizeof edid;
    descriptor.DescriptorBuffer = edid;
    CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
    return static_cast<std::uint32_t>(edid[56] | (edid[58] >> 4u) << 8u);
}

}

TEST(adapter_start_device, offers_the_hosts_own_modes_whatever_their_size)
{ Fixture fixture;
  fixture.offer_modes(host_modes({{1920u, 1080u}, {100u, 100u}, {2560u, 1440u}, {5120u, 2880u}, {80u, 80u}}, 2u));
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(edid_preferred_width(fixture), 2560u);
  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  const av::DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, no_pivot, 0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(&request)), success);
  CHECK_EQ(vidpn.source_modes(0u).modes(), 3u * av::DisplayMode::source_format_count);
  CHECK_EQ(commit(fixture, 100u, 100u), success);
  CHECK_EQ(commit(fixture, 80u, 80u), invalid_source_mode);
  CHECK_EQ(commit(fixture, 1280u, 800u), invalid_source_mode); }

TEST(adapter_dpc_routine, a_display_event_is_cleared_and_each_monitor_replugged)
{ Fixture fixture;
  fixture.offer_modes(host_modes({{1920u, 1080u}}, 0u));
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(edid_preferred_width(fixture), 1920u);
  fixture.offer_modes(host_modes({{100u, 100u}, {1024u, 768u}}, 0u));
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 1u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.function().device_config().read_u32(4u), 1u);
  CHECK_EQ(fixture.dxgkrnl().child_statuses(), 2u);
  CHECK_EQ(fixture.dxgkrnl().child_status_uid(0u), 0u);
  CHECK(!fixture.dxgkrnl().child_status_connected(0u));
  CHECK(fixture.dxgkrnl().child_status_connected(1u));
  fixture.platform().on_stall(&Fixture::answer_display_info_from, &fixture);
  CHECK_EQ(edid_preferred_width(fixture), 100u);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(commit(fixture, 100u, 100u), success);
  CHECK_EQ(commit(fixture, 1920u, 1080u), invalid_source_mode);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().child_statuses(), 2u); }

namespace {

void spend_log_budget(Fixture &fixture)
{
    const av::DxgkargRecommendVidPnTopologyRecord topology = {};
    for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
        static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
}

}

TEST(adapter_dpc_routine, a_context_whose_release_is_held_past_the_deadline_is_lost_and_its_neighbour_is_not)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  AvProcess other(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  av::DxgkargCreateContextRecord neighbour = {};
  CHECK_EQ(other.create_context(neighbour), success);
  av::DxgkargCreateContextRecord queue = {};
  queue.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &queue)), success);
  av::DxgkargCreateContextRecord follower = {};
  follower.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(other.device(), &follower)), success);

  std::uint8_t release[av::ReleaseRecord::encoded_size] = {};
  CHECK_STATUS(av::ReleaseRecord(process.ring().kmt_handle(), av::SubmissionDescriptor(1u, 32u, 64u))
                   .encode(release, sizeof release),
               av::Status::ok);
  CHECK_EQ(submit_packet(fixture, queue.hContext, 5u, release, sizeof release), success);
  std::uint8_t follow[av::FollowRecord::encoded_size] = {};
  CHECK_STATUS(av::FollowRecord(other.ring().kmt_handle(), 1u).encode(follow, sizeof follow), av::Status::ok);
  CHECK_EQ(submit_packet(fixture, follower.hContext, 6u, follow, sizeof follow), success);

  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  fixture.platform().advance_time(av::ExecutionNode::park_deadline);
  run_dpc(fixture);
  CHECK(fixture.platform().logged_line("avgpu: a held packet passed its deadline and was let go"));
  CHECK(fixture.platform().logged_line("avgpu: a context's batches cannot be released in order: the context is lost"));
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 2u);
  CHECK_EQ(submit_packet(fixture, queue.hContext, 7u, release, sizeof release), invalid_parameter);
  CHECK_EQ(submit_packet(fixture, follower.hContext, 8u, follow, sizeof follow), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_context(follower.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(queue.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(neighbour.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(other.device())), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_dpc_routine, a_deadline_passed_with_the_log_budget_spent_is_let_go_unlogged)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  AvProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  av::DxgkargCreateContextRecord queue = {};
  queue.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(process.device(), &queue)), success);
  CHECK_EQ(process.submit(context.hContext, 7u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  std::uint8_t follow[av::FollowRecord::encoded_size] = {};
  CHECK_STATUS(av::FollowRecord(process.ring().kmt_handle(), 1u).encode(follow, sizeof follow), av::Status::ok);
  CHECK_EQ(submit_packet(fixture, queue.hContext, 8u, follow, sizeof follow), success);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  fixture.platform().advance_time(av::ExecutionNode::park_deadline);
  spend_log_budget(fixture);
  run_dpc(fixture);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 2u);
  CHECK(!fixture.platform().logged_line("a held packet passed its deadline"));
  CHECK_EQ(nt(fixture.adapter().destroy_context(queue.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

TEST(adapter_start_device, a_present_engine_the_device_will_not_start_leaves_presents_to_the_miniport_until_it_will)
{ for (const Fixture::Misanswer how : {Fixture::Misanswer::silent, Fixture::Misanswer::error,
                                       Fixture::Misanswer::truncated}) {
      Fixture fixture;
      fixture.offer_present_engine();
      fixture.misanswer(av::PageRequest::present_engine, how);
      CHECK_EQ(fixture.start(), success);
      CHECK(fixture.platform().logged_line("avgpu: the device's present engine was not started, status 0x"));
      CHECK(!fixture.platform().logged_line("avgpu: presents are moved by the device's present engine"));
      CHECK(fixture.adapter().invariants_hold());
      fixture.misanswer(0u, Fixture::Misanswer::none);
      fixture.answer_asks();
      CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
      fixture.forget_control();
      CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
      CHECK(fixture.platform().logged_line("avgpu: presents are moved by the device's present engine"));
      CHECK(fixture.adapter().invariants_hold());
  } }

TEST(adapter_start_device, a_host_that_answers_the_window_counters_badly_is_logged_and_the_window_stays_offered)
{ for (const Fixture::Misanswer how : {Fixture::Misanswer::silent, Fixture::Misanswer::error,
                                       Fixture::Misanswer::truncated}) {
      Fixture fixture;
      fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
      fixture.misanswer(av::PageRequest::window_stats, how);
      CHECK_EQ(fixture.start(), success);
      CHECK(AdapterProbe(fixture.adapter()).window_offered());
      CHECK(fixture.platform().logged_line("avgpu: host window counters status 0x"));
      CHECK(!fixture.platform().logged_line("avgpu: host window counters status 0x00000000"));
  } }

TEST(adapter_start_device, a_window_with_no_stats_page_or_no_map_is_logged_and_withheld_or_offered_bare)
{ std::uint32_t stats_page = 0u;
  for (std::uint32_t nth = 1u; nth <= 16u && stats_page == 0u; nth++) {
      Fixture probe;
      probe.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
      probe.platform().fail_allocation_at(nth);
      const bool started = probe.start() == success;
      probe.platform().fail_allocation_at(0u);
      if (started && probe.platform().logged_line("host window stats page is unavailable")) stats_page = nth;
  }
  CHECK(stats_page != 0u);
  Fixture bare;
  bare.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  bare.platform().fail_allocation_at(stats_page);
  CHECK_EQ(bare.start(), success);
  bare.platform().fail_allocation_at(0u);
  CHECK(AdapterProbe(bare.adapter()).window_offered());
  CHECK(bare.platform().logged_line("avgpu: host window counters status 0xC0000184"));

  std::uint32_t map = 0u;
  for (std::uint32_t nth = 1u; nth <= 64u && map == 0u; nth++) {
      Fixture probe;
      probe.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
      probe.platform().fail_allocation_at(nth);
      const bool started = probe.start() == success;
      probe.platform().fail_allocation_at(0u);
      if (started && probe.platform().logged_line("avgpu: host window status 0xC0000017")) map = nth;
  }
  CHECK(map != 0u);
  Fixture mapless;
  mapless.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  mapless.platform().fail_allocation_at(map);
  CHECK_EQ(mapless.start(), success);
  mapless.platform().fail_allocation_at(0u);
  CHECK(!AdapterProbe(mapless.adapter()).window_offered());
  CHECK(!mapless.platform().logged_line("avgpu: host window counters")); }

TEST(adapter_set_vidpn_source_address_with_multi_plane_overlay, a_base_plane_with_no_allocation_is_not_scanned_out_and_the_system_segment_is_)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  int primary = 0;
  av::DxgkMultiplaneOverlayPlaneRecord plane = {};
  plane.LayerIndex = 0u;
  plane.Enabled = 1u;
  plane.AllocationSegment = 2u;
  plane.AllocationAddress = 0x6000000000ll;
  plane.hAllocation = nullptr;
  av::DxgkargSetVidPnSourceAddressWithMultiPlaneOverlayRecord request = {};
  request.PlaneCount = 1u;
  request.pPlanes = &plane;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK(AdapterProbe(fixture.adapter()).primary(0u) == nullptr);
  fixture.platform().set_irql(Irql::isr);
  plane.hAllocation = &primary;
  plane.AllocationSegment = 0u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK(AdapterProbe(fixture.adapter()).primary(0u) == &primary);
  CHECK_EQ(AdapterProbe(fixture.adapter()).scanout(0u), 0x6000000000ull); }

TEST(adapter_set_vidpn_source_address_with_multi_plane_overlay3, skips_what_it_cannot_show_and_takes_no_more_than_the_planes_there_are)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  int primary = 0, overlay = 0;
  av::DxgkPrimaryContextDataRecord base_view = {};
  base_view.hAllocation = &primary;
  base_view.SegmentId = 0u;
  base_view.SegmentAddress = 0x6000000000ll;
  av::DxgkPrimaryContextDataRecord overlay_view = {};
  overlay_view.hAllocation = &overlay;
  overlay_view.SegmentId = 1u;
  overlay_view.SegmentAddress = 0x9000000000ll;
  av::DxgkPrimaryContextDataRecord unallocated = {};
  unallocated.hAllocation = nullptr;
  unallocated.SegmentId = 1u;
  av::DxgkPrimaryContextDataRecord *base_views[] = {&base_view};
  av::DxgkPrimaryContextDataRecord *overlay_views[] = {&overlay_view};
  av::DxgkPrimaryContextDataRecord *unallocated_views[] = {&unallocated};
  av::DxgkMultiplaneOverlayPlane3Record planes[7] = {};
  planes[0].LayerIndex = 0u;
  planes[0].InputFlags = av::plane_input_enabled;
  planes[0].ContextCount = 1u;
  planes[0].ppContextData = base_views;
  planes[1].LayerIndex = 1u;
  planes[1].InputFlags = av::plane_input_enabled;
  planes[1].ContextCount = 0u;
  planes[1].ppContextData = overlay_views;
  planes[2].LayerIndex = 2u;
  planes[2].InputFlags = av::plane_input_enabled;
  planes[2].ContextCount = 1u;
  planes[2].ppContextData = nullptr;
  planes[3].LayerIndex = 3u;
  planes[3].InputFlags = av::plane_input_enabled;
  planes[3].ContextCount = 1u;
  planes[3].ppContextData = unallocated_views;
  planes[4].LayerIndex = av::OverlayPlane::max_planes;
  planes[5].LayerIndex = 1u;
  planes[5].InputFlags = av::plane_input_enabled;
  planes[5].ContextCount = 1u;
  planes[5].ppContextData = overlay_views;
  planes[6].LayerIndex = 2u;
  planes[6].InputFlags = av::plane_input_enabled;
  planes[6].ContextCount = 1u;
  planes[6].ppContextData = overlay_views;
  av::DxgkMultiplaneOverlayPlane3Record *plane_list[] = {nullptr,    &planes[4], &planes[0], &planes[1],
                                                         &planes[2], &planes[3], &planes[5], &planes[6]};
  av::DxgkargSetVidPnSourceAddressWithMultiPlaneOverlay3Record request = {};
  request.PlaneCount = 8u;
  request.ppPlanes = plane_list;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay3(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(AdapterProbe(fixture.adapter()).scanout(0u), 0x6000000000ull);
  CHECK(AdapterProbe(fixture.adapter()).primary(0u) == &primary);
  CHECK(AdapterProbe(fixture.adapter()).plane(0u, 1u).allocation() == nullptr);
  CHECK(AdapterProbe(fixture.adapter()).plane(0u, 2u).allocation() == nullptr);
  CHECK(AdapterProbe(fixture.adapter()).plane(0u, 3u).allocation() == nullptr);
  request.PlaneCount = 0u;
  request.ppPlanes = nullptr;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay3(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK(AdapterProbe(fixture.adapter()).primary(0u) == &primary); }

#include "core/adapter/adapter.hpp"
#include "core/adapter/debug_report.hpp"
#include "core/display/display_info_response.hpp"
#include "core/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_child_container_id_record.hpp"
#include "core/ddi/dxgk_child_descriptor_record.hpp"
#include "core/ddi/dxgk_child_status_record.hpp"
#include "core/ddi/dxgk_device_descriptor_record.hpp"
#include "core/ddi/dxgk_display_information_record.hpp"
#include "core/ddi/dxgk_driver_caps_record.hpp"
#include "core/ddi/dxgk_gpu_clock_data_record.hpp"
#include "core/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "core/ddi/dxgk_node_metadata_record.hpp"
#include "core/ddi/dxgk_open_allocation_info_record.hpp"
#include "core/ddi/dxgk_page_table_level_desc_record.hpp"
#include "core/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "core/ddi/dxgk_query_page_table_level_desc_in_record.hpp"
#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/ddi/dxgkarg_cancel_command_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "core/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_create_process_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "core/ddi/dxgkarg_escape_record.hpp"
#include "core/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "core/ddi/dxgkarg_get_scan_line_record.hpp"
#include "core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/ddi/dxgkarg_patch_record.hpp"
#include "core/ddi/dxgkarg_preempt_command_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "core/ddi/dxgkarg_query_dependent_engine_group_record.hpp"
#include "core/ddi/dxgkarg_query_engine_status_record.hpp"
#include "core/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
#include "core/ddi/dxgkarg_recommend_functional_vidpn_record.hpp"
#include "core/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "core/ddi/dxgkarg_recommend_vidpn_topology_record.hpp"
#include "core/platform/log_budget.hpp"
#include "core/ddi/dxgkarg_render_record.hpp"
#include "core/ddi/dxgkarg_reset_engine_record.hpp"
#include "core/ddi/dxgkarg_reset_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_resume_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/ddi/dxgkarg_set_stable_power_state_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_address_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_visibility_record.hpp"
#include "core/ddi/dxgkarg_submit_command_record.hpp"
#include "core/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "core/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "core/ddi/linked_device_record.hpp"
#include "core/display/display_mode.hpp"
#include "core/scheduling/context.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/fake_vidpn.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <cstdint>
#include <optional>
#include <cstring>

namespace av {

struct AdapterProbe {
    explicit AdapterProbe(Adapter &target) : adapter(target) {}
    void set_started(bool started) { adapter.started_ = started; }
    /* The window inside a power transition that no caller can stand in: the
     * device object exists and is not running, which is when an interrupt
     * dxgkrnl left connected must be left alone (feature 040, US1). */
    void stop_device_object() { static_cast<void>(adapter.device_.object().stop()); }
    Adapter &adapter;
};

}  // namespace av

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
constexpr std::int32_t  query_driver_caps = 1;          /* DXGKQAITYPE_DRIVERCAPS */
constexpr std::int32_t  query_wddm_device_caps = 29;    /* DXGKQAITYPE_WDDMDEVICECAPS */
constexpr std::int32_t  last_query_type = 48;           /* DXGKQAITYPE_PAGINGPROCESSGPUVASIZE */

constexpr std::uint32_t adapter_uid = 0xFFFFFFFFu;   /* DISPLAY_ADAPTER_HW_ID */
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;
constexpr std::uint8_t  driver_ok_status = 0x0Fu;
constexpr std::uint8_t  failed_bit = 0x80u;

std::uint32_t nt(std::int32_t status) { return static_cast<std::uint32_t>(status); }

/* One little-endian field of a request the device saw. */
std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

/* The display info a host with one 1280x800 scanout answers. */
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

    /* A scanout the device reports as off. */
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

/* One adapter as dxgkrnl sees it: AddDevice done, StartDevice not yet. */
constexpr std::uint32_t create_blob = 0x010Cu;       /* VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB */
constexpr std::uint32_t map_blob = 0x0208u;          /* VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB */
constexpr std::uint32_t get_capset_info = 0x0108u;   /* VIRTIO_GPU_CMD_GET_CAPSET_INFO */
constexpr std::uint32_t get_capset = 0x0109u;        /* VIRTIO_GPU_CMD_GET_CAPSET */
constexpr std::uint32_t resource_unref = 0x0102u;    /* VIRTIO_GPU_CMD_RESOURCE_UNREF */
constexpr std::uint32_t ctx_create = 0x0200u;        /* VIRTIO_GPU_CMD_CTX_CREATE */
constexpr std::uint32_t ctx_destroy = 0x0201u;       /* VIRTIO_GPU_CMD_CTX_DESTROY */
constexpr std::uint32_t submit_3d = 0x0207u;         /* VIRTIO_GPU_CMD_SUBMIT_3D */
constexpr std::uint64_t host_region_address = 0x4000000000ull;
constexpr std::uint64_t host_region_bytes = 16u << 20u;
constexpr std::uint8_t  first_gpu[av::GpuIdentity::uuid_bytes] = {3};
constexpr std::uint8_t  other_gpu[av::GpuIdentity::uuid_bytes] = {4};

/* A device that says it has a host-visible region; whether it really presents
 * one is the PCI walk's answer, and both have to agree (feature 041). */
av::CapsetInfo host_capset(const std::uint8_t *uuid)
{
    return av::CapsetInfo(av::ProtocolVersion(av::ProtocolVersion::current_major,
                                              av::ProtocolVersion::current_minor),
                          1u, 64u, 12u, 24u, av::CapsetInfo::feature_host_visible_memory,
                          av::GpuIdentity(1u, 2u, av::GpuIdentity::discrete_gpu, uuid, 0u, 0u, "the host's"));
}

class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), adapter_(platform_, &function_), sources_(9u), children_(9u) {}
    ~Fixture() { static_cast<void>(adapter_.stop_device()); }

    /* StartDevice asks the device what its displays are and waits for the
     * answer, so the host answers from the stall between its polls. */
    std::uint32_t start()
    {
        /* A start builds new rings, so the device side of the old ones is gone. */
        control_.reset();
        platform_.on_stall(&Fixture::answer_display_info, this);
        const std::uint32_t started = nt(adapter_.start_device(nullptr, &dxgkrnl_.record(), &sources_, &children_));
        platform_.on_stall(nullptr, nullptr);
        return started;
    }

    DisplayInfoAnswer &display_info() { return display_info_; }

    /* What the host answers StartDevice's ask: nothing at all unless a test
     * says otherwise. */
    enum class Answer { display_info, none, wrong_type, no_scanout };
    void answer_with(Answer answer) { answer_ = answer; }

    /* The host answers whatever the adapter asks from here on, as it does
     * during StartDevice. */
    void answer_asks() { platform_.on_stall(&Fixture::answer_display_info, this); }

    /* The host answering a later ask, for a test that changes the display. */
    static void answer_display_info_from(void *context) { answer_display_info(context); }

    /* A power cycle destroys the device and builds it again, so the queue this
     * views is programmed afresh: a test that cycles power takes a new view. */
    void forget_control() { control_.reset(); }

    /* The device side of the control queue, shared with the answer StartDevice
     * waits for, so a test sees the requests made after it and no others. */
    av::FakeVirtqueueDevice &control()
    {
        if (!control_.has_value()) {
            const av::FakeCommonConfig::Queue &queue = function_.common().queue(0u);
            control_.emplace(platform_, queue.desc, queue.driver, queue.device, queue.size);
        }
        return *control_;
    }

    /* What the device says it is from the next ask on: another host GPU is
     * another device (feature 040, FR-005). */
    void                   answer_capset_of(const std::uint8_t *uuid) { capset_ = host_capset(uuid); }
    /* Whether the driver made its region blob and mapped it. */
    /* A device with no room left for the region's blob. */
    void                   refuse_the_blob() { refuse_blob_ = true; }
    bool                   created_blob() const { return created_blob_; }
    bool                   mapped_region() const { return mapped_region_; }
    /* MAP_BLOBs of the region the device answered, across power cycles. */
    std::uint32_t          region_maps() const { return region_maps_; }
    /* A device that stops answering what it is. */
    void                   ignore_capset() { ignore_capset_ = true; }
    /* CTX_CREATE, CTX_DESTROY, SUBMIT_3D and RESOURCE_UNREF the device answered. */
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
        /* The adapter asks the device what it is as well as what it shows, and
         * the answers are told apart by what was asked (feature 040, FR-005). */
        const std::uint32_t asked = fixture->control().pending_type();
        /* The whole host-visible region as one blob, which StartDevice creates
         * and maps when the device offers a region (feature 041). */
        if (asked == create_blob || asked == map_blob) {
            av::GpuResponse answer;
            if (fixture->refuse_blob_) {
                answer.set(av::GpuResponse::err_out_of_memory);
                static_cast<void>(fixture->control().respond_all(answer.bytes(),
                                                                 static_cast<std::uint32_t>(answer.length())));
                return;
            }
            if (asked == map_blob) {
                fixture->mapped_region_ = true;
                fixture->region_maps_++;
                answer.set_map_info(av::GpuResponse::map_cache_cached);
            } else {
                fixture->created_blob_ = true;
                answer.set(av::GpuResponse::ok_nodata);
            }
            static_cast<void>(fixture->control().respond_all(answer.bytes(),
                                                             static_cast<std::uint32_t>(answer.length())));
            return;
        }
        if ((asked == get_capset_info || asked == get_capset) && fixture->ignore_capset_) return;
        if (asked == get_capset_info || asked == get_capset) {
            av::GpuResponse answer;
            if (asked == get_capset_info)
                answer.set_capset_info(av::CapsetInfo::capset_id, 1u, av::CapsetInfo::encoded_size);
            else
                answer.set_capset(fixture->capset_);
            static_cast<void>(fixture->control().respond_all(answer.bytes(),
                                                             static_cast<std::uint32_t>(answer.length())));
            return;
        }
        /* An rvk context's setup and teardown (feature 043). */
        if (asked == ctx_create || asked == ctx_destroy || asked == submit_3d || asked == resource_unref) {
            fixture->context_requests_++;
            static_cast<void>(fixture->control().respond_all(0x1100u));   /* OK_NODATA */
            return;
        }
        if (fixture->answer_ == Answer::none) return;
        if (fixture->answer_ == Answer::wrong_type) {
            static_cast<void>(fixture->control().respond_all(0x1100u));   /* OK_NODATA, not display info */
            return;
        }
        if (fixture->answer_ == Answer::no_scanout) fixture->display_info_.disable(0u);
        static_cast<void>(fixture->control().respond_all(fixture->display_info_.bytes(),
                                                         fixture->display_info_.length()));
    }

    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    Adapter               adapter_;
    DisplayInfoAnswer     display_info_;
    av::CapsetInfo        capset_ = host_capset(first_gpu);
    bool                  refuse_blob_ = false;
    bool                  created_blob_ = false;
    bool                  mapped_region_ = false;
    std::uint32_t         region_maps_ = 0u;
    std::uint32_t         context_requests_ = 0u;
    bool                  ignore_capset_ = false;
    Answer                answer_ = Answer::display_info;
    std::optional<av::FakeVirtqueueDevice> control_;
    std::uint32_t         sources_;
    std::uint32_t         children_;
};

/* A primary the adapter created and VidMm mapped into the aperture: the handle
 * and address dxgkrnl hands back to SetVidPnSourceAddress, and the pages the
 * host will be told to read. `spare_pages` are mapped past the frame, so a test
 * can move the address a page at a time and still be fully backed. */
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
        paging.Operation = 5;   /* DXGK_OPERATION_MAP_APERTURE_SEGMENT */
        std::memcpy(paging.Reserved, &mapping, sizeof mapping);
        CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&paging)), success);
    }

    void        *handle() const { return handle_; }
    /* The address of the frame `offset_in_pages` into the mapping. */
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

}  // namespace

TEST(adapter, remembers_its_physical_device_object)
{ MockPlatform platform; int pdo = 0;
  const Adapter adapter(platform, &pdo);
  CHECK(adapter.physical_device_object() == &pdo);
  CHECK(adapter.invariants_hold()); }

/* ---- StartDevice ---------------------------------------------------------- */

/* US1 acceptance 2 asks the guest log to show the driver's steps in order, so
 * each line is asserted where it is written rather than only counted. Feature
 * 036's live run stopped after DriverEntry and said nothing more. */
TEST(adapter_start_device, logs_the_step_it_reached)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: StartDevice") == 0);
  av::DxgkChildDescriptorRecord relations[2] = {};
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations)), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: QueryChildRelations") == 0); }

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
      {0, no_memory},             /* DxgkCbGetDeviceInformation refused */
      {1, invalid_parameter},     /* config space read refused */
      {2, invalid_parameter},     /* short config read: malformed */
      {3, no_space},              /* BAR mapping refused */
      {4, not_found},             /* no resource for the virtio BAR */
      {5, invalid_parameter},     /* malformed capability list */
      {6, not_supported},         /* device without VERSION_1 */
      {7, invalid_parameter},     /* less system memory than a page */
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
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {} }

/* ---- StopDevice and teardown ---------------------------------------------- */

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

/* ---- Power ----------------------------------------------------------------- */

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

/* dxgkrnl leaves the interrupt connected across a power transition, so one can
 * arrive while power_up is between constructing the device and starting it.
 * A device that is being built is not one to read from (feature 040, US1). */
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
  CHECK_EQ(fixture.platform().live_allocations(), 1u);   /* the cursor's backing, held while started */ }

/* ---- ResetDevice ----------------------------------------------------------- */

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

/* ---- Interrupt and DPC ----------------------------------------------------- */

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
  CHECK_EQ(fixture.platform().logged(), quiet);   /* a clean DPC says nothing */
  CHECK_EQ(fixture.dxgkrnl().dpc_notifications(), 1u); }

TEST(adapter_dpc_routine, a_device_lying_about_a_completion_is_failed_and_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(0u);
  const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
  used[2] = 9u;   /* used.idx: completions nothing was submitted for */
  const std::uint32_t logged = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().logged() > logged);
  CHECK((fixture.function().common().status() & failed_bit) != 0u); }

/* ---- Children -------------------------------------------------------------- */

TEST(adapter_query_child_relations, reports_one_hot_pluggable_video_output)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  DxgkChildDescriptorRecord relations[2];
  std::memset(relations, 0xEE, sizeof relations);
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations)), success);
  CHECK_EQ(relations[0].ChildDeviceType, 1);              /* TypeVideoOutput */
  CHECK_EQ(relations[0].InterfaceTechnology, -1);         /* D3DKMDT_VOT_OTHER */
  CHECK_EQ(relations[0].MonitorOrientationAwareness, 1);  /* D3DKMDT_MOA_NONE */
  CHECK_EQ(relations[0].SupportsSdtvModes, 0u);
  CHECK_EQ(relations[0].HpdAwareness, 4);                 /* HpdAwarenessInterruptible */
  CHECK_EQ(relations[0].AcpiUid, 0u);
  CHECK_EQ(relations[0].ChildUid, 0u);
  CHECK_EQ(relations[1].ChildDeviceType, static_cast<std::int32_t>(0xEEEEEEEEu)); }

TEST(adapter_query_child_relations, no_room_is_buffer_too_small_and_no_buffer_invalid)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  DxgkChildDescriptorRecord relations[2];
  /* dxgkrnl's array holds one more descriptor than the children reported. */
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations - 1u)), buffer_too_small);
  CHECK_EQ(nt(fixture.adapter().query_child_relations(nullptr, sizeof relations)), invalid_parameter); }

TEST(adapter_query_child_status, the_one_child_is_connected)
{ Fixture fixture;
  DxgkChildStatusRecord status = {};
  status.Type = 1;   /* StatusConnection */
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 1u)), success);
  CHECK_EQ(status.HotPlugConnected, 1u); }

TEST(adapter_query_child_status, rotation_other_types_and_other_children_are_refused)
{ Fixture fixture;
  DxgkChildStatusRecord status = {};
  status.Type = 2;   /* StatusRotation */
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), invalid_parameter);
  status.Type = 3;   /* StatusMiracastConnection */
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

/* ---- VidPn and modes -------------------------------------------------------- */

namespace {

constexpr std::uint32_t invalid_source_mode = 0xC01E0310u;
constexpr std::uint32_t transformation_not_supported = 0xC01E0346u;
constexpr std::uint32_t gamma_ramp_not_supported = 0xC01E0348u;
constexpr std::uint32_t no_recommended_functional_vidpn = 0xC01E0323u;
constexpr std::uint32_t no_recommended_vidpn_topology = 0xC01E031Au;
constexpr std::int32_t  interrupt_crtc_vsync = 3;
constexpr std::int32_t  interrupt_dpc_log = 4;   /* DXGK_INTERRUPT_DPC_LOG_TYPE: anything else */
constexpr std::int32_t  no_pivot = 5;

av::D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    av::D3dkmdtVidPnSourceModeRecord mode = {};
    av::DisplayMode(width, height).describe(mode);
    return mode;
}

/* Commits `width` x `height` to source 0 through a fake VidPn. */
std::uint32_t commit(Fixture &fixture, std::uint32_t width, std::uint32_t height)
{
    av::FakeVidPn vidpn;
    vidpn.add_path(0u, 0u);
    vidpn.source_modes(0u).seed(source_mode(width, height), true);
    const av::DxgkargCommitVidPnRecord request = {&vidpn, 0u, 0, nullptr, 0u, 0u};
    return nt(fixture.adapter().commit_vidpn(&request));
}

}  // namespace

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
  /* The five standard modes that fit the 1280x800 display the device reports. */
  CHECK_EQ(vidpn.source_modes(0u).modes(), 5u);
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

/* A commit makes the resource the mode needs; the DPC attaches the primary,
 * scans it out and carries its first frame; a present carries another
 * (feature 038, US1). */
/* Between D3 and D0 the device object is gone. Every path a guest can still
 * reach must answer without touching it: dxgkrnl keeps flipping, moving the
 * cursor and asking for scan lines right up to the transition. */
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
  /* The cursor was told to forget the device, so it refuses rather than
   * pointing a request at memory that is gone. */
  const av::DxgkargSetPointerPositionRecord position = {0u, 10, 10, 0u};
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(&position)), invalid_device_state);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);

  fixture.forget_control();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK(fixture.adapter().invariants_hold()); }

/* D3 is not StopDevice: dxgkrnl keeps the VidPn and every allocation across it,
 * so what was scanning out before power went away is what it expects to find
 * after (feature 040, US1). */
TEST(adapter_set_power_state, what_was_scanning_out_before_d3_is_scanning_out_after_d0)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  CHECK_EQ(control.respond_all(0x1100u), 1u);   /* the committed mode's resource */
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  /* The resume asks the host for the same scanout the guest still believes in. */
  av::FakeVirtqueueDevice &resumed = fixture.control();
  /* The host lost every resource with the device, so the source that was
   * scanning out has its resource created again; the pages follow with the
   * guest's next address, which is what dxgkrnl sends after a resume. */
  CHECK_EQ(resumed.respond_all(0x1100u), 2u);   /* the scanout, and the capset the resume asks for */
  CHECK_EQ(resumed.request_type(0u), 0x0101u);
  CHECK(fixture.adapter().invariants_hold()); }

/* A resume that never had a scanout asks the host for none. */
TEST(adapter_set_power_state, a_source_that_was_dark_stays_dark_across_power)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  CHECK_EQ(control.respond_all(0x1100u), 0u);   /* StartDevice's display info was answered */
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  /* Nothing was scanning out: the only request the resume made is the one that
   * asks the device what it is. */
  CHECK_EQ(fixture.control().respond_all(0x1100u), 1u);
  CHECK_EQ(fixture.control().request_type(0u), 0x0108u);
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
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  const std::uint32_t after_commit = control.respond_all(0x1100u);
  CHECK_EQ(after_commit, 5u);                         /* create, attach, set_scanout, transfer, flush */
  /* Requests 0 to 2 were StartDevice's: the display info, and the two the
   * capset query takes (feature 040, FR-005). */
  CHECK_EQ(control.request_type(3u), 0x0101u);
  CHECK_EQ(control.request_type(4u), 0x0106u);
  CHECK_EQ(control.request_type(5u), 0x0103u);
  CHECK_EQ(control.request_type(6u), 0x0105u);
  CHECK_EQ(control.request_type(7u), 0x0104u);
  /* Nothing presented since: the host is not asked to read again. */
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(control.respond_all(0x1100u), 0u); }

/* A host that answers nothing fills the control queue, and the DPC says once
 * that a refresh was refused rather than silently dropping the frame. */
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
      fixture.platform().set_irql(Irql::dispatch);
      fixture.adapter().dpc_routine();
      fixture.platform().set_irql(Irql::passive);
      refused = fixture.platform().logged() != logged;
  }
  CHECK(refused);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: a scanout refresh was refused in the DPC") == 0);
  /* Spent budget: the refusal goes on being refused, and says nothing more. */
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

/* A commit whose resource the host refuses says so, and the commit still
 * answers what dxgkrnl asked about the VidPn. */
TEST(adapter_commit_vidpn, a_refused_scanout_resource_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  bool refused = false;
  for (std::uint32_t attempt = 0u; attempt < 200u && !refused; attempt++) {
      const std::uint32_t width = (attempt % 2u) == 0u ? 1280u : 1024u;
      const std::uint32_t height = (attempt % 2u) == 0u ? 800u : 768u;
      const std::uint32_t before = fixture.platform().logged();
      CHECK_EQ(commit(fixture, width, height), success);
      /* A commit logs its own status; one whose resource was refused logs the
       * refusal as well, which is the second line. */
      refused = fixture.platform().logged() > before + 1u;
  }
  CHECK(refused); }

/* Committing a source with no path takes it off the host. */
TEST(adapter_commit_vidpn, a_path_that_is_gone_scans_out_nothing)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  CHECK_EQ(commit(fixture, 1280u, 800u), success);
  CHECK_EQ(control.respond_all(0x1100u), 1u);
  av::FakeVidPn empty;
  const av::DxgkargCommitVidPnRecord request = {&empty, 0u, 0, nullptr, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().commit_vidpn(&request)), success);
  CHECK_EQ(control.respond_all(0x1100u), 0u);         /* it was never scanning out */
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
  const std::int32_t scalings[] = {0, 2, 5, 254, 255};   /* Uninitialized, Centered, Custom, Unpinned, NotSpecified */
  for (const std::int32_t scaling : scalings) {
      path.ContentTransformationScaling = scaling;
      CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), transformation_not_supported);
  }
  path.ContentTransformationScaling = 1;
  /* Uninitialized, Rotate90, Rotate270, Identity_Offset90, Unpinned, NotSpecified */
  const std::int32_t rotations[] = {0, 2, 4, 5, 254, 255};
  for (const std::int32_t rotation : rotations) {
      path.ContentTransformationRotation = rotation;
      CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), transformation_not_supported);
  } }

/* FR-006: a gamma ramp is refused with its own status, not accepted and then
 * never programmed, because GammaRampCaps claims nothing. */
TEST(adapter_update_active_vidpn_present_path, refuses_every_gamma_ramp_it_cannot_program)
{ Fixture fixture;
  av::D3dkmdtVidPnPresentPathRecord path = {};
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 1;
  path.GammaRampType = 1;   /* D3DDDI_GAMMARAMP_DEFAULT */
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
  const std::uint64_t frame = 10000000u / 60u;
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  fixture.platform().run_timers(frame * 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).InterruptType, interrupt_crtc_vsync);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 0u)), success);
  fixture.platform().run_timers(frame * 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_dpc_log, 1u)), not_supported); }

TEST(adapter_control_interrupt, before_start_there_is_no_vsync)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), invalid_device_state);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 0u)), success); }

TEST(adapter_stop_device, stops_the_vsync)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  fixture.platform().run_timers(10000000u);
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
  fixture.platform().run_timers(10000000u / 60u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0x70u);
  request.hAllocation = nullptr;
  request.PrimarySegment = 9u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().run_timers(10000000u / 60u);
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
  request.Flags = 0x2u;   /* FlipImmediate */
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.platform().run_timers(10000000u / 60u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0x50u); }

TEST(adapter_set_vidpn_source_address, a_primary_in_system_memory_names_segment_zero)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.hAllocation = &primary;   /* VIDMM_SEGMENT::DriverId answers 0 for the implicit segment, address 0 */
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  fixture.platform().run_timers(10000000u / 60u);
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
  present.Flags = 1u;   /* Blt */
  CHECK_EQ(nt(fixture.adapter().present(&fixture, &present)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(dma[0], 0x52u);
  CHECK_EQ(patches[1].AllocationIndex, 2u);
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

/* ---- WDDM 1.3 (feature 016) -------------------------------------------------- */

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
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(1u, &metadata)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0x10000u, &metadata)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0u, nullptr)), invalid_parameter); }

TEST(adapter_control_interrupt2, enables_and_disables_the_synthetic_vsync)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::uint64_t frame = 10000000u / 60u;
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 0})), success);
  fixture.platform().run_timers(frame);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 1})), success);
  fixture.platform().run_timers(frame * 3u);
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

/* ---- WDDM 1.2 (feature 015) -------------------------------------------------- */

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
  const av::DxgkargResetEngineRecord refused[] = {{1u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargResetEngineRecord &request : refused) {
      av::DxgkargResetEngineRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().reset_engine(&copy)), invalid_parameter);
  }
  CHECK_EQ(nt(fixture.adapter().reset_engine(nullptr)), invalid_parameter); }

/* VidSchiCheckNodeTimeout (dxgmms2.sys @1400c9b50) takes a responsive answer as
 * "no timeout this round", so an engine with work that has stopped moving must
 * say so: an answer that is always yes defers a hung host forever. */
/* US3 of feature 040: a reset throws away the work on the node and answers
 * what was executing; VidSch already knows what it reset, so nothing more is
 * reported - DMA_FAULTED would bugcheck the guest (0x119/0xD). The other
 * process's context is untouched - a reset of one engine is not an outage. */
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
  CHECK_EQ(reset.LastAbortedFenceId, 1u);   /* the head, not the tail */
  fixture.adapter().dpc_routine();          /* and nothing it threw away completes afterwards */
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);

  /* Both contexts still submit, and their work completes. */
  submit.SubmissionFenceId = 3u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  submit.hContext = second.hContext;
  submit.SubmissionFenceId = 4u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).InterruptType, 1);   /* DMA_COMPLETED */
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
  CHECK_EQ(status.EngineStatus, 1u);   /* nothing queued: an idle engine is not hung */
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
  CHECK_EQ(status.EngineStatus, 1u);   /* the first answer about a busy engine defers */
  fixture.platform().advance_time(100u);
  status.EngineStatus = 0u;
  CHECK_EQ(nt(fixture.adapter().query_engine_status(&status)), success);
  CHECK_EQ(status.EngineStatus, 0u);   /* it has not moved since: dxgkrnl may reset it */
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  const av::DxgkargQueryEngineStatusRecord refused[] = {{1u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargQueryEngineStatusRecord &request : refused) {
      av::DxgkargQueryEngineStatusRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().query_engine_status(&copy)), invalid_parameter);
      CHECK_EQ(copy.EngineStatus, 0u);
  }
  CHECK_EQ(nt(fixture.adapter().query_engine_status(nullptr)), invalid_parameter); }

TEST(adapter_query_dependent_engine_group, the_one_node_depends_on_itself)
{ Fixture fixture;
  av::DxgkargQueryDependentEngineGroupRecord group = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(&group)), success);
  CHECK_EQ(group.DependentNodeOrdinalMask, 1u);
  const av::DxgkargQueryDependentEngineGroupRecord refused[] = {{1u, 0u, 0u}, {0u, 1u, 0u}};
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
  fixture.function().common().delay_reset(100000u);   /* a gone device never acknowledges */
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
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(nullptr)), not_supported);
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

/* The two the timeout path uses are reachable from VidSch's DPC - its check and
 * its reset both run there - so dispatch is their ceiling, not passive. The
 * other two are asked on a thread. */
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

/* ---- WDDM 1.1 (feature 014) -------------------------------------------------- */

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

/* ---- Cursor and the legacy surface (feature 013) ---------------------------- */

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
  CHECK_EQ(fixture.platform().logged(), quiet); }   /* a clean DPC says nothing */

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
  /* The power cycle built new rings, so the device side is new too. */
  const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(0u);
  av::FakeVirtqueueDevice device(fixture.platform(), queue.desc, queue.driver, queue.device, queue.size);
  /* The resume asks the device what it is as well (feature 040, FR-005), and
   * nothing answers that here, so it is still published. */
  CHECK_EQ(device.respond_all(0x1100u), 4u);
  CHECK_EQ(device.request_type(0u), 0x0108u);   /* what the resume asked the device it is */
  CHECK_EQ(device.request_type(1u), 0x0101u); }

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

/* ---- The rest of the slot list --------------------------------------------- */

TEST(adapter_link_device, is_a_chain_of_one_led_by_this_adapter)
{ Fixture fixture;
  LinkedDeviceRecord linked;
  std::memset(&linked, 0xEE, sizeof linked);
  CHECK_EQ(nt(fixture.adapter().link_device(&fixture.function(), &linked)), success);
  CHECK_EQ(linked.ChainUid, 0u);
  CHECK_EQ(linked.NumberOfLinksInChain, 1u);
  CHECK_EQ(linked.LeadLink, 1u);
  CHECK_EQ(nt(fixture.adapter().link_device(&fixture.function(), nullptr)), invalid_parameter); }

TEST(adapter, dispatch_io_request_and_query_interface_are_not_supported)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().dispatch_io_request(0u, nullptr)), not_supported);
  CHECK_EQ(nt(fixture.adapter().query_interface(nullptr)), not_supported); }

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

/* ---- QueryAdapterInfo ------------------------------------------------------ */

namespace {

/* What DXGADAPTER::DdiQueryAdapterInfo accepts without logging an invalid status. */
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

}  // namespace

TEST(adapter_query_adapter_info, answers_driver_caps_with_wddm_3_2)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK_EQ(caps.WDDMVersion, 0x3200); }

/* dxgkrnl asks for types the driver declines, so the line carries the status
 * as well as the type: in the guest that is how the order of the capability
 * queries, and which of them were refused, is read back (US1 acceptance 2). */
TEST(adapter_query_adapter_info, logs_each_type_with_the_status_it_answered)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: QueryAdapterInfo type 1 status 0x00000000") == 0);
  DxgkargQueryAdapterInfoRecord declined = {};
  declined.Type = 999;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&declined)), not_implemented);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: QueryAdapterInfo type 999 status 0xC0000002") == 0); }

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

TEST(adapter_query_adapter_info, answers_the_display_caps_extension)
{ Fixture fixture; std::uint32_t extension = 0xEEu;
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 16;   /* DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION */
  query.pOutputData = &extension;
  query.OutputDataSize = sizeof extension;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(extension, 0u); }

TEST(adapter_query_adapter_info, answers_the_segment_query_once_started)
{ Fixture fixture; std::uint32_t physical_adapter = 0u; av::DxgkQuerySegmentOut4Record out = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 11;   /* DXGKQAITYPE_QUERYSEGMENT4 */
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &out;
  query.OutputDataSize = sizeof out;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), invalid_parameter);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(out.NbSegment, 1u); }

TEST(adapter_query_adapter_info, declines_every_other_type_without_touching_either_buffer)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::int32_t out_of_range[] = {-1, last_query_type + 1, 0x7FFFFFFF, static_cast<std::int32_t>(0x80000000u)};
  for (std::int32_t type = 0; type <= last_query_type + 4; type++) {
      const std::int32_t requested = type <= last_query_type ? type : out_of_range[type - last_query_type - 1];
      if (requested == query_driver_caps || requested == query_wddm_device_caps || requested == 11 ||
          (requested >= 13 && requested <= 16))
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

/* ---- Escape ---------------------------------------------------------------- */

TEST(adapter_escape, no_escape_is_defined_so_every_escape_is_not_supported)
{ Fixture fixture; std::uint8_t data[64] = {};
  DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  const std::uint32_t sizes[] = {1u, 4u, 64u};
  for (std::uint32_t size : sizes) {
      escape.PrivateDriverDataSize = size;
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), not_supported);
  } }

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

/* ---- CollectDbgInfo -------------------------------------------------------- */

namespace {

std::uint32_t collect(Adapter &adapter, std::uint8_t *buffer, std::uint64_t size, DxgkargCollectDbgInfoExtRecord *ext)
{
    DxgkargCollectDbgInfoRecord record = {};
    record.pBuffer = buffer;
    record.BufferSize = size;
    record.pExtension = ext;
    return nt(adapter.collect_dbg_info(&record));
}

}  // namespace

TEST(adapter_collect_dbg_info, writes_the_started_and_powered_state)
{ Fixture fixture; GuardedBuffer buffer(av::DebugReport::report_bytes);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(collect(fixture.adapter(), buffer.data(), av::DebugReport::report_bytes, nullptr), success);
  CHECK_EQ(buffer.data()[0], 0x52u);   /* 'R' */
  CHECK_EQ(buffer.data()[6], 1u);      /* started */
  CHECK_EQ(buffer.data()[7], 1u);      /* powered */
  CHECK(buffer.data()[8] != 0u || buffer.data()[12] != 0u);   /* VERSION_1 negotiated */
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
  CHECK_EQ(buffer[16], 4u);   /* last submitted */
  CHECK_EQ(buffer[20], 0u);   /* last completed */
  CHECK_EQ(buffer[32], 1u);   /* pending */ }

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

/* ---- Devices and allocations (feature 008) ---------------------------------- */

namespace {

/* The runtime's shadow surface, created the way dxgkrnl creates one: the
 * driver describes it, then allocates it. */
class ShadowSurface {
public:
    explicit ShadowSurface(Adapter &adapter) : adapter_(adapter), surface_{64u, 32u, 21, 0u}, descriptor_{}, info_{}
    {
        av::DxgkargGetStandardAllocationDriverDataRecord describe = {};
        describe.StandardAllocationType = 2;   /* D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE */
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

}  // namespace

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
  CHECK_EQ(shadow.destroy(), invalid_device_state);   /* still open */
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

TEST(adapter_create_allocation, before_start_there_is_no_segment_to_place_it_in)
{ Fixture fixture;
  ShadowSurface shadow(fixture.adapter());
  CHECK_EQ(shadow.create(), invalid_parameter); }

/* A bring-up that never reaches a committable VidPn is read back from the log,
 * so each step of the display path says what it answered (FR-003). The lines
 * are asserted in the order dxgkrnl walks them. */
TEST(adapter, each_step_of_the_display_path_says_what_it_answered)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  ShadowSurface shadow(fixture.adapter());
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: GetStandardAllocationDriverData status 0x00000000") == 0);
  CHECK_EQ(shadow.create(), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: CreateAllocation status 0x00000000") == 0);
  av::DxgkargCreateDeviceRecord device = {};
  device.hDevice = &fixture;   /* the runtime's handle, which the driver replaces with its own */
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  const char *line = fixture.platform().last_log();
  CHECK(std::strncmp(line, "rvkgpu: CreateDevice runtime 0x", 31u) == 0);
  CHECK(std::strcmp(line + 47, " status 0x00000000") == 0);
  CHECK(std::strncmp(line + 31, "0000000000000000", 16u) != 0);   /* the handle it was given */
  av::DxgkargCreateDeviceRecord second = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&second)), success);
  CHECK(std::strncmp(fixture.platform().last_log() + 31, "0000000000000000", 16u) == 0);
  av::DxgkargDescribeAllocationRecord describe = {};
  describe.hAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().describe_allocation(&describe)), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: DescribeAllocation status 0x00000000") == 0);
  av::DxgkargSetDisplayPrivateDriverFormatRecord format = {};
  format.PrimaryAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&format)), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: SetDisplayPrivateDriverFormat status 0x00000000") == 0);
  av::DxgkargSetVidPnSourceVisibilityRecord visibility = {};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visibility)), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: SetVidPnSourceVisibility source 0 hidden status 0x00000000") == 0);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  CHECK_EQ(shadow.destroy(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: StopDevice status 0x00000000") == 0); }

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

/* ---- Contexts (feature 009) --------------------------------------------------- */

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

TEST(adapter_patch, patches_or_refuses_a_missing_block)
{ Fixture fixture;
  std::uint8_t dma[8] = {};
  av::DxgkargPatchRecord patch = {};
  patch.pDmaBuffer = dma;
  patch.DmaBufferSize = sizeof dma;
  CHECK_EQ(nt(fixture.adapter().patch(&patch)), success);
  CHECK_EQ(nt(fixture.adapter().patch(nullptr)), invalid_parameter); }

/* ---- Submission (feature 009) ------------------------------------------------- */

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
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);   /* fence not newer */
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
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), invalid_parameter);   /* a queue still runs on it */
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
  CHECK_EQ(fence, 2u);   /* neither the reset's nor the destroyed queue's work is written */
  CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(queue.hHwQueue)), invalid_parameter);
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

TEST(adapter_dpc_routine, a_refused_progress_fence_signal_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  std::uint64_t fence = 0u;
  av::DxgkargSubmitCommandToHwQueueRecord submit = {};
  submit.hHwQueue = &fixture;
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

/* A fault storm at DISPATCH would otherwise turn one broken device into an
 * unbounded stream of log lines, so every refusal line in the DPC and every DDI
 * status line is spent from the budget (feature 037, FR-003). Once the window is
 * spent, the same four refusals and the same DDI say nothing at all. */
TEST(adapter_dpc_routine, a_storm_of_refusals_is_silent_once_the_window_is_spent)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  std::uint64_t progress = 0u;
  av::DxgkargSubmitCommandToHwQueueRecord hw_submit = {};
  hw_submit.hHwQueue = &fixture;
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
  CHECK_EQ(fixture.platform().logged(), spent);   /* the QueryAdapterInfo line too */
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  CHECK_EQ(fixture.platform().logged(), spent);   /* and the handle line */
  for (std::uint16_t index = 0u; index < 2u; index++) {
      const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(index);
      const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
      used[2] = 1u;   /* used.idx: a completion nothing was submitted for */
  }
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  /* The one line the DPC still writes is the device failing itself, which it
   * does once and then stops running - not a storm to budget. */
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

/* ---- TDR (feature 010) ---------------------------------------------------------- */

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
  CHECK_EQ(fixture.platform().live_allocations(), 1u);   /* the cursor's backing, held while started */ }

/* An empty list destroys nothing and leaves every source's primary alone. */
TEST(adapter_destroy_allocation, an_empty_list_is_accepted)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const av::DxgkargDestroyAllocationRecord empty = {};
  CHECK_EQ(nt(fixture.adapter().destroy_allocation(&empty)), success); }

/* Where a source points is worth a line, but not at the cost of the window,
 * and never at device IRQL where the budget's lock may not be taken. */
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

/* The device's own display info decides what the driver offers: the modes that
 * fit its scanout, and an EDID whose preferred timing is its size. */
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
  CHECK_EQ(vidpn.source_modes(0u).modes(), 3u);   /* 640x480, 800x600, 1024x768 */
  CHECK_EQ(commit(fixture, 1280u, 800u), invalid_source_mode); }

/* A host that resizes the display raises a configuration change; the next DDI
 * that describes the monitor asks the device again, because asking waits and
 * the interrupt may not. */
TEST(adapter_interrupt_routine, a_configuration_change_is_followed_by_the_next_monitor_ddi)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.display_info().describe(0u, 1600u, 900u);
  fixture.platform().on_stall(&Fixture::answer_display_info_from, &fixture);
  fixture.platform().set_irql(Irql::isr);
  /* With message interrupts the device's configuration vector is message 0. */
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 1u);
  fixture.platform().set_irql(Irql::passive);
  av::DxgkDeviceDescriptorRecord descriptor = {};
  std::uint8_t edid[av::Edid::block_bytes] = {};
  descriptor.DescriptorLength = sizeof edid;
  descriptor.DescriptorBuffer = edid;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(edid[56] | (edid[58] >> 4u) << 8u, 1600u);
  /* Asked once: the second descriptor is answered from what it learned. */
  const std::uint32_t asked = fixture.control().requests();
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  CHECK_EQ(fixture.control().requests(), asked); }

/* Nothing the device says about its displays can leave the driver unable to
 * offer one: the standard modes stand, and the refusal is logged. */
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
      CHECK_EQ(vidpn.source_modes(0u).modes(), av::DisplayModeList::standard_count);
  } }

/* A device that went away cannot be asked what its displays are, and the
 * change it signalled before it went is not followed into nothing. */
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

/* A harvest that fails every DPC would otherwise log every DPC. */
TEST(adapter_dpc_routine, a_failing_harvest_is_silent_once_the_window_is_spent)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(0u);
  const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
  used[2] = 9u;   /* used.idx: completions nothing was submitted for */
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  /* The queue says once, without the budget, that the device lied; the DPC's
   * own line about it is what the spent window keeps quiet. */
  CHECK_EQ(fixture.platform().logged(), spent + 1u);
  CHECK(std::strstr(fixture.platform().last_log(), "harvest failed in the DPC") == nullptr); }

/* ---- Several outputs (feature 038, US4) ------------------------------------- */

/* The device says how many displays it has, and every one of them gets a VidPn
 * source, a child, an EDID and a scanout of its own. */
TEST(adapter_start_device, offers_a_source_and_a_child_for_every_display)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 2u);   /* num_scanouts */
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
  status.Type = 1;   /* StatusConnection */
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

/* Each source has its own resource and its own scanout on the host. */
TEST(adapter_commit_vidpn, the_second_display_scans_out_on_its_own)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 2u);
  CHECK_EQ(fixture.start(), success);
  av::FakeVirtqueueDevice &control = fixture.control();
  av::FakeVidPn vidpn;
  vidpn.add_path(1u, 1u);
  av::D3dkmdtVidPnSourceModeRecord pinned = {};
  av::DisplayMode(1024u, 768u).describe(pinned);
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
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(control.respond_all(0x1100u), 5u);
  /* Requests 0 to 2 were StartDevice's display info and capset query; the
   * resource and the scanout are the second source's. */
  CHECK_EQ(control.request_type(3u), 0x0101u);
  CHECK_EQ(u32(control.request(3u), 24u), av::ScanoutResource::first_resource_id + 1u);
  CHECK_EQ(u32(control.request(3u), 32u), 1024u);
  CHECK_EQ(control.request_type(5u), 0x0103u);
  CHECK_EQ(u32(control.request(5u), 40u), 1u);   /* scanout 1 */
  CHECK_EQ(u32(control.request(5u), 44u), av::ScanoutResource::first_resource_id + 1u); }

/* One mode list cannot describe displays of different sizes, so the driver says
 * when the device gives it some. */
TEST(adapter_start_device, says_when_the_displays_are_not_all_one_size)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 2u);
  fixture.display_info().describe(1u, 1024u, 768u);
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.platform().logged_line("display 1 is 1024x768, not the first display's size")); }

/* A device with no displays, or more than this driver offers, is clamped and
 * the count it gave is logged: the outputs the guest sees are then not the
 * host's. */
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

/* The window can be spent before StartDevice says any of this. */
TEST(adapter_start_device, says_nothing_about_the_displays_once_the_window_is_spent)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, av::DisplaySources::max_sources + 1u);
  fixture.display_info().describe(2u, 800u, 600u);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.adapter().recommend_vidpn_topology(&topology));
  const std::uint32_t spent = fixture.platform().logged();
  CHECK_EQ(fixture.start(), success);
  /* StartDevice's own line is not from the budget; everything this task added
   * is. */
  CHECK_EQ(fixture.platform().logged(), spent + 1u);
  CHECK(!fixture.platform().logged_line("display 2 is"));
  CHECK(!fixture.platform().logged_line("the device reports"));
  const av::DxgkargSetVidPnSourceVisibilityRecord visible = {0u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visible)), success);
  CHECK_EQ(fixture.platform().logged(), spent + 1u); }

/* Only the display that differs is named, and the ones that match are passed
 * over. */
TEST(adapter_start_device, names_the_display_that_is_not_the_first_s_size)
{ Fixture fixture;
  fixture.function().device_config().set_u32(8u, 3u);
  fixture.display_info().describe(1u, 1280u, 800u);
  fixture.display_info().describe(2u, 1280u, 600u);
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.platform().logged_line("display 2 is 1280x600, not the first display's size"));
  CHECK(!fixture.platform().logged_line("display 1 is")); }

/* FR-005: a hibernated guest is restored onto whatever QEMU is running now. A
 * device answering for the same host GPU is the one the adapter started on, and
 * everything carries on. */
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

/* Another host GPU behind the same PCI function is another device: what the
 * guest still holds cannot be finished on it. Nothing is reported at D0 -
 * DMA_FAULTED would bugcheck the guest (0x119/0xD) - so the work queued for
 * the device that is gone stays queued until dxgkrnl's timeout detection
 * resets the engine, and ResetEngine answers it. A new context and a new
 * submission are refused, while the display, which is the scanout and none of
 * this, keeps working. */
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
  /* The transition itself still succeeds: a bugcheck is what FR-005 is about
   * not doing. */
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);
  CHECK(fixture.platform().logged_line("rvkgpu: the device under this adapter is not the one it started on"));

  /* Nothing new runs on it: a context is refused, and so is a submission on
   * the context that survived, rather than being taken for a completion nobody
   * will send. */
  av::DxgkargCreateContextRecord refused = {};
  refused.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &refused)), not_supported);
  submit.SubmissionFenceId = 2u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);

  /* WDDM's own recovery: the reset answers the submission that was queued. */
  fixture.platform().set_irql(Irql::dispatch);
  av::DxgkargResetEngineRecord reset = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().reset_engine(&reset)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(reset.LastAbortedFenceId, 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);

  /* The desktop is untouched: the scanout the adapter restored is still the
   * one it had. */
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

/* Power builds the device and then starts it, and dxgkrnl leaves the interrupt
 * connected across the whole transition: one arriving in between finds an
 * object that exists and has nothing to read. Neither the ISR nor the DPC may
 * touch it (feature 040, US1). */
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

/* A resume that never read what the device was has nothing to compare it with,
 * and runs on as it did: a device that will not answer is not a device that
 * changed (FR-005). */
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
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);   /* nothing was lost */
  av::DxgkargCreateContextRecord another = {};
  another.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &another)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(another.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

/* A device that changed with nothing queued on the one that is gone reports
 * nothing either, and the refusal still stands for everything after it. */
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

/* Feature 041, US1: VidMm maps a process's view of a CPU-visible segment
 * itself and tells the driver nothing, so every offset it can place an
 * allocation at must already have host memory behind it. The driver makes that
 * true the only way it can: one blob the size of the region, mapped at offset
 * zero when the device offers a region. */
TEST(adapter_start_device, maps_the_whole_host_visible_region_as_one_blob)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.created_blob());
  CHECK(fixture.mapped_region());

  std::uint32_t physical_adapter = 0u;
  av::DxgkQuerySegmentOut4Record out = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 11;   /* DXGKQAITYPE_QUERYSEGMENT4 */
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &out;
  query.OutputDataSize = sizeof out;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(out.NbSegment, 2u); }

/* A device with no region is the adapter as it was: one segment, and no blob
 * asked for. */
TEST(adapter_start_device, a_device_with_no_region_asks_for_no_blob)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(!fixture.created_blob());
  CHECK(!fixture.mapped_region()); }

/* A device that cannot give the driver its region is a device the driver runs
 * without one: the adapter still starts, and the segment it reported is the
 * aperture alone, because memory nobody can reach is worse than memory nobody
 * was promised (feature 041, FR-001). */
TEST(adapter_start_device, a_region_the_device_refuses_leaves_the_adapter_without_one)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  fixture.refuse_the_blob();
  const std::uint32_t logged = fixture.platform().logged();
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.platform().logged() > logged);   /* said once, with the status that stopped it */

  std::uint32_t physical_adapter = 0u;
  av::DxgkQuerySegmentOut4Record out = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 11;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &out;
  query.OutputDataSize = sizeof out;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  /* And the segment is not offered: a segment with no storage behind it is one
   * VidMm places allocations in and then writes into nothing. */
  CHECK_EQ(out.NbSegment, 1u); }

namespace {

/* How many segments the adapter reports to VidMm now. */
std::uint32_t reported_segments(Fixture &fixture)
{
    std::uint32_t                  physical_adapter = 0u;
    av::DxgkQuerySegmentOut4Record out = {};
    DxgkargQueryAdapterInfoRecord  query = {};
    query.Type = 11;   /* DXGKQAITYPE_QUERYSEGMENT4 */
    query.pInputData = &physical_adapter;
    query.InputDataSize = sizeof physical_adapter;
    query.pOutputData = &out;
    query.OutputDataSize = sizeof out;
    CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
    return out.NbSegment;
}

}  // namespace

/* A D3 resets the device, and the reset takes the region's blob with it; VidMm
 * still has the segment it was told about at start, so D0 on the same device
 * must make the blob again, or every offset of that segment is a BAR with
 * nothing placed in it. Found by the full-stack rig (tests/loopback). */
TEST(adapter_set_power_state, d0_on_the_same_device_makes_the_host_visible_region_again)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.region_maps(), 1u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(reported_segments(fixture), 2u);   /* what VidMm holds does not change across D3 */
  fixture.forget_control();
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.region_maps(), 2u);
  CHECK_EQ(reported_segments(fixture), 2u); }

/* dxgkrnl may ask for D0 on an adapter already in D0: the device was never
 * reset, so the blob it holds is the region working and is kept. */
TEST(adapter_set_power_state, a_second_d0_keeps_the_host_visible_region)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.region_maps(), 1u);
  CHECK_EQ(reported_segments(fixture), 2u); }

/* A timeout reset is a device reset too, and the restart makes the blob again. */
TEST(adapter_restart_from_timeout, makes_the_host_visible_region_again)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  fixture.forget_control();
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.region_maps(), 2u);
  CHECK_EQ(reported_segments(fixture), 2u); }

/* Another device behind the function is not the one the segment was reported
 * for, so the region is closed rather than made on it. */
TEST(adapter_set_power_state, a_device_that_changed_gets_no_host_visible_region)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  fixture.answer_capset_of(other_gpu);
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.region_maps(), 1u);
  CHECK_EQ(reported_segments(fixture), 1u); }

/* A device that no longer says what it is cannot be given the region either. */
TEST(adapter_set_power_state, a_device_that_stopped_saying_what_it_is_gets_no_host_visible_region)
{ Fixture fixture;
  fixture.function().add_shared_memory_region(1u, 2u, 0u, host_region_bytes, host_region_address);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  fixture.ignore_capset();
  fixture.answer_asks();
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_EQ(fixture.region_maps(), 1u);
  CHECK_EQ(reported_segments(fixture), 1u); }

/* ---- rvk contexts (feature 043) ------------------------------------------------- */

namespace {

/* A process's transport allocation: created through the DDI, given its D3DKMT
 * handle, and mapped into the aperture over guest memory by BuildPagingBuffer,
 * as VidMm does when the process makes it resident. */
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
        paging.Operation = 5;   /* DXGK_OPERATION_MAP_APERTURE_SEGMENT */
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

private:
    Fixture      &fixture_;
    std::uint64_t bytes_;
    std::uint8_t *block_;
    std::uint64_t frames_[4];
    std::uint8_t  encoded_[av::AllocationDescriptor::encoded_size];
    void         *handle_;
    std::uint32_t kmt_handle_;
};

/* One process's rvk transport as the adapter sees it: its device, its ring,
 * fence page and reply slot, and the record naming them. */
class RvkProcess {
public:
    RvkProcess(Fixture &fixture, av::FakeHandleTable &handles)
        : fixture_((fixture.answer_asks(), fixture)), device_{},
          ring_(fixture, handles, av::AllocationKind::context_ring, 0x2000u, 10u),
          fence_(fixture, handles, av::AllocationKind::fence_page, 0x1000u, 20u),
          reply_(fixture, handles, av::AllocationKind::reply_slot, 0x1000u, 30u), record_bytes_{}
    {
        CHECK_EQ(nt(fixture.adapter().create_device(&device_)), success);
        const av::RvkContextRecord record(av::SubApi::vulkan, 12u, ring_.kmt_handle(), fence_.kmt_handle(),
                                          reply_.kmt_handle());
        CHECK_STATUS(record.encode(record_bytes_, sizeof record_bytes_), av::Status::ok);
    }

    std::uint32_t create_context(av::DxgkargCreateContextRecord &request)
    {
        request.Flags = 0x4u;   /* VirtualAddressing */
        request.pPrivateDriverData = record_bytes_;
        request.PrivateDriverDataSize = sizeof record_bytes_;
        return nt(fixture_.adapter().create_context(device_.hDevice, &request));
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
    /* What the host writes once it finishes batches through `completed`. */
    void host_completes(std::uint64_t completed)
    {
        static_cast<void>(av::FencePage(completed, completed).encode(fence_.bytes(), 32u));
    }
    void host_faults(std::uint64_t completed, std::uint32_t code)
    {
        static_cast<void>(av::FencePage(completed, completed, code).encode(fence_.bytes(), 32u));
    }
    TransportAllocation &ring() { return ring_; }
    void                *device() const { return device_.hDevice; }
    std::uint8_t        *record_bytes() { return record_bytes_; }

private:
    Fixture                      &fixture_;
    av::DxgkargCreateDeviceRecord device_;
    TransportAllocation           ring_;
    TransportAllocation           fence_;
    TransportAllocation           reply_;
    std::uint8_t                  record_bytes_[av::RvkContextRecord::encoded_size];
    std::uint8_t                  descriptor_bytes_[av::SubmissionDescriptor::encoded_size] = {};
};

}  // namespace

/* SC-001's driver half: a process's context is set up on the host over its own
 * memory, a batch it describes rings the doorbell, and the submission fence
 * dxgkrnl waits on completes only when the host's fence page says so. */
TEST(adapter_rvk_context, a_process_s_batch_completes_when_the_host_says_it_did)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  RvkProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK(context.hContext != nullptr);
  CHECK_EQ(context.ContextInfoDmaBufferPrivateDataSize, 32u);
  CHECK(fixture.platform().logged_line("rvk context on doorbell 0 message 0 status 0x00000000"));
  CHECK_EQ(fixture.context_requests(), 2u);   /* CTX_CREATE and the setup */
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(fixture.function().doorbells().rings(0u), 1u);
  const std::uint32_t before = fixture.dxgkrnl().interrupts();
  process.host_completes(0u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before);   /* nothing finished on the host yet */
  process.host_completes(1u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), before + 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).InterruptType, 1);   /* DMA_COMPLETED */
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).FenceId, 1u);
  /* Private data that is not a descriptor of this ring publishes nothing. */
  CHECK_EQ(process.submit(context.hContext, 2u, av::SubmissionDescriptor(2u, 32u, 9000u)), invalid_parameter);
  CHECK_EQ(fixture.function().doorbells().rings(0u), 1u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), 6u);   /* and CTX_DESTROY and an unref per blob */
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

/* The pages a context runs on cannot be freed under it: freeing its ring first
 * destroys the host context, so the device writes nothing into pages that are
 * no longer the process's. */
TEST(adapter_rvk_context, freeing_memory_a_context_runs_on_destroys_the_host_context_first)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  RvkProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  const std::uint32_t requests = fixture.context_requests();
  CHECK_EQ(process.ring().destroy(), success);
  CHECK_EQ(fixture.context_requests(), requests + 4u);   /* CTX_DESTROY and the unrefs before the free */
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), requests + 4u);   /* nothing more to destroy */
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

/* A record is honoured only where it can run: a device with no doorbell pages,
 * or a record naming memory the process does not have, fails the creation and
 * leaves no context behind. */
TEST(adapter_rvk_context, a_record_that_cannot_run_fails_the_creation_and_leaves_nothing)
{ Fixture stock; av::FakeHandleTable handles;
  CHECK_EQ(stock.start(), success);
  { RvkProcess process(stock, handles);
    av::DxgkargCreateContextRecord context = {};
    CHECK_EQ(process.create_context(context), invalid_parameter);
    CHECK(context.hContext == nullptr);
    CHECK_EQ(stock.context_requests(), 0u);
    CHECK_EQ(nt(stock.adapter().destroy_device(process.device())), success); }

  Fixture rvk;
  rvk.function().add_doorbell_region();
  CHECK_EQ(rvk.start(), success);
  RvkProcess process(rvk, handles);
  const std::uint32_t nobody = 0x7777u;
  std::memcpy(process.record_bytes() + 12u, &nobody, sizeof nobody);   /* the ring's handle */
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), invalid_parameter);
  CHECK(context.hContext == nullptr);
  CHECK_EQ(nt(rvk.adapter().destroy_device(process.device())), success); }

/* A device that goes through D3 destroys every host context: the process's
 * context submits nothing more, and destroying it asks the device nothing. */
TEST(adapter_rvk_context, a_power_transition_abandons_the_host_contexts)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  RvkProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.forget_control();
  const std::uint32_t requests = fixture.context_requests();
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), requests);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

/* Everything that stops a record from running fails the creation, as a context
 * this adapter cannot create at all always did. */
TEST(adapter_rvk_context, a_device_that_is_gone_or_silent_runs_no_record)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  RvkProcess process(fixture, handles);
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

/* A device whose doorbell pages cannot be mapped is not started half-way. */
TEST(adapter_start_device, doorbell_pages_that_cannot_be_mapped_fail_the_start)
{ Fixture fixture;
  fixture.function().add_doorbell_region();
  fixture.dxgkrnl().refuse_map(2u, 0xC000009Au);
  CHECK_EQ(fixture.start(), no_space);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK(fixture.adapter().invariants_hold()); }

/* FR-006: a context the host faulted has what it queued completed, so the
 * process waiting on it wakes (to no reply), and every later submission on it
 * is refused; a plain context beside it keeps completing. dxgkrnl is never
 * told DMA_FAULTED, which bugchecks a GPU MMU adapter (0x119/0xD). */
TEST(adapter_rvk_context, a_host_fault_completes_the_waiting_submission_and_refuses_the_next)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  RvkProcess process(fixture, handles);
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
  CHECK_EQ(fixture.dxgkrnl().interrupt(before).InterruptType, 1);   /* DMA_COMPLETED */
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

/* A TDR resets the device: the host contexts are gone, a process's submissions
 * are refused rather than queued for a fence nobody will write, and after the
 * restart the process creates its context again from scratch. */
TEST(adapter_rvk_context, a_timeout_reset_loses_the_host_context_and_a_new_one_sets_up_after_restart)
{ Fixture fixture; av::FakeHandleTable handles;
  fixture.function().add_doorbell_region();
  CHECK_EQ(fixture.start(), success);
  RvkProcess process(fixture, handles);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(process.create_context(context), success);
  CHECK_EQ(process.submit(context.hContext, 1u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  fixture.forget_control();
  CHECK_EQ(process.submit(context.hContext, 2u, av::SubmissionDescriptor(2u, 32u, 64u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  const std::uint32_t requests = fixture.context_requests();
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(fixture.context_requests(), requests);   /* the reset already destroyed it on the host */
  av::DxgkargCreateContextRecord again = {};
  CHECK_EQ(process.create_context(again), success);
  CHECK_EQ(fixture.context_requests(), requests + 2u);
  CHECK_EQ(process.submit(again.hContext, 3u, av::SubmissionDescriptor(1u, 0u, 32u)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(again.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(process.device())), success); }

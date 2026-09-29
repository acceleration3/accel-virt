#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"

#include "core/ddi/dxgk_child_status_record.hpp"

#include "core/ddi/dxgkargcb_query_feature_support_record.hpp"

#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/vidpn/fake_vidpn.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::int32_t success = 0;
constexpr std::int32_t invalid_parameter = static_cast<std::int32_t>(0xC000000Du);

}  // namespace

FakeDxgkrnlAdapter::FakeDxgkrnlAdapter(MockPlatform &platform, FakeVirtioGpuFunction &function, std::uint8_t index)
    : platform_(platform), function_(function), record_(), device_information_status_(0u),
      config_read_status_(0u), short_config_read_(false), map_refusal_countdown_(0u),
      map_refusal_status_(0u), unmap_status_(0u), mappings_{}, mapping_count_(0u), maps_(0u), dpcs_(0u), index_(index), system_memory_(std::int64_t{8} << 30u), interrupts_{},
      interrupt_count_(0u), dpc_notifications_(0u), sink_(nullptr), sink_context_(nullptr), synchronize_status_(0u), synchronized_(false)
{
    record_.Size = sizeof record_;
    record_.Version = 0x11007u;
    record_.DeviceHandle = this;
    record_.DxgkCbGetDeviceInformation = +[](void *handle, DxgkDeviceInfoRecord *info) {
        return static_cast<FakeDxgkrnlAdapter *>(handle)->get_device_information(info);
    };
    record_.DxgkCbReadDeviceSpace = +[](void *handle, std::uint32_t space, void *buffer,
                                        std::uint32_t offset, std::uint32_t length, std::uint32_t *read) {
        return static_cast<FakeDxgkrnlAdapter *>(handle)->read_device_space(space, buffer, offset, length, read);
    };
    record_.DxgkCbMapMemory = +[](void *handle, std::int64_t address, std::uint32_t length,
                                  std::uint8_t io_space, std::uint8_t user_mode, std::int32_t cache,
                                  void **mapped) {
        return static_cast<FakeDxgkrnlAdapter *>(handle)->map_memory(address, length, io_space, user_mode,
                                                                     cache, mapped);
    };
    record_.DxgkCbUnmapMemory = +[](void *handle, void *mapped) {
        return static_cast<FakeDxgkrnlAdapter *>(handle)->unmap_memory(mapped);
    };
    record_.DxgkCbAcquireHandleData = &AvFakeAcquireHandleData;
    record_.DxgkCbReleaseHandleData = &AvFakeReleaseHandleData;
    record_.DxgkCbQueryVidPnInterface = &AvFakeQueryVidPnInterface;
    record_.DxgkCbSynchronizeExecution = +[](void *handle, SynchronizeRoutine routine, void *context,
                                             std::uint32_t message, std::uint8_t *returned) {
        return static_cast<FakeDxgkrnlAdapter *>(handle)->synchronize_execution(routine, context, message, returned);
    };
    record_.DxgkCbNotifyInterrupt = +[](void *handle, const DxgkargcbNotifyInterruptDataRecord *data) {
        static_cast<FakeDxgkrnlAdapter *>(handle)->notify_interrupt(*data);
    };
    record_.DxgkCbNotifyDpc = +[](void *handle) { static_cast<FakeDxgkrnlAdapter *>(handle)->notify_dpc(); };
    record_.DxgkCbIndicateChildStatus = +[](void *handle, const DxgkChildStatusRecord *status) -> std::int32_t {
        auto *const fake = static_cast<FakeDxgkrnlAdapter *>(handle);
        if (fake->platform_.current_irql() > Irql::dispatch)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "DxgkCbIndicateChildStatus above DISPATCH_LEVEL");
        if (fake->child_status_count_ < max_child_statuses) {
            fake->child_status_uids_[fake->child_status_count_] = status->ChildUid;
            fake->child_status_connected_[fake->child_status_count_] = status->HotPlugConnected != 0u;
        }
        fake->child_status_count_++;
        return 0;
    };
    record_.DxgkCbQueueDpc = +[](void *handle) {
        return static_cast<FakeDxgkrnlAdapter *>(handle)->queue_dpc();
    };
    /* What the OS answers about a feature the driver implements: nothing is
     * enabled until a test says so, as a machine with hardware scheduling off
     * answers (feature 049). */
    record_.DxgkCbQueryFeatureSupport = +[](DxgkargcbQueryFeatureSupportRecord *request) {
        return static_cast<FakeDxgkrnlAdapter *>(request->DeviceHandle)->query_feature_support(*request);
    };
}

FakeDxgkrnlAdapter::~FakeDxgkrnlAdapter()
{
    if (mapping_count_ != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "%u BAR mapping(s) never unmapped",
                                                mapping_count_);
    for (std::uint32_t i = 0u; i < mapping_count_; i++)
        platform_.detach_register_windows(mappings_[i].address, mappings_[i].length);
}

DxgkrnlInterfaceRecord &FakeDxgkrnlAdapter::record() { return record_; }

void FakeDxgkrnlAdapter::allow_feature(std::uint32_t feature_id)
{
    allowed_features_ |= std::uint64_t{1} << (feature_id % 64u);
}

void FakeDxgkrnlAdapter::refuse_feature_queries(std::uint32_t status) { feature_query_status_ = status; }

std::int32_t FakeDxgkrnlAdapter::query_feature_support(DxgkargcbQueryFeatureSupportRecord &request)
{
    feature_queries_++;
    request.Enabled = 0u;
    if (feature_query_status_ != 0u) return static_cast<std::int32_t>(feature_query_status_);
    /* A driver that says its implementation is off gets no from any OS. */
    if (request.DriverSupportState == 0u) return 0;
    request.Enabled = (allowed_features_ & (std::uint64_t{1} << (request.FeatureId % 64u))) != 0u ? 1u : 0u;
    return 0;
}

std::uint32_t FakeDxgkrnlAdapter::feature_queries() const { return feature_queries_; }

void FakeDxgkrnlAdapter::refuse_device_information(std::uint32_t status) { device_information_status_ = status; }
void FakeDxgkrnlAdapter::refuse_config_read(std::uint32_t status) { config_read_status_ = status; }
void FakeDxgkrnlAdapter::short_config_read() { short_config_read_ = true; }

void FakeDxgkrnlAdapter::refuse_unmap(std::uint32_t status) { unmap_status_ = status; }

void FakeDxgkrnlAdapter::refuse_map(std::uint32_t nth, std::uint32_t status)
{
    map_refusal_countdown_ = nth;
    map_refusal_status_ = status;
}

std::uint32_t FakeDxgkrnlAdapter::live_mappings() const { return mapping_count_; }
std::uint32_t FakeDxgkrnlAdapter::maps() const { return maps_; }
std::uint32_t FakeDxgkrnlAdapter::dpcs_queued() const { return dpcs_; }

void FakeDxgkrnlAdapter::set_system_memory(std::int64_t bytes) { system_memory_ = bytes; }

std::int32_t FakeDxgkrnlAdapter::get_device_information(DxgkDeviceInfoRecord *info)
{
    std::memset(info, 0, sizeof *info);
    info->TranslatedResourceList = function_.resources().bytes();
    info->PhysicalDeviceObject = &function_;
    info->SystemMemorySize = system_memory_;
    return static_cast<std::int32_t>(device_information_status_);
}

std::int32_t FakeDxgkrnlAdapter::read_device_space(std::uint32_t space, void *buffer, std::uint32_t offset,
                                                   std::uint32_t length, std::uint32_t *read)
{
    *read = 0u;
    if (config_read_status_ != 0u) return static_cast<std::int32_t>(config_read_status_);
    if (space != 0u || offset > max_config_bytes || length > max_config_bytes - offset) return invalid_parameter;
    const std::uint32_t copied = short_config_read_ ? length / 2u : length;
    std::memcpy(buffer, function_.config_space() + offset, copied);
    *read = copied;
    return success;
}

std::int32_t FakeDxgkrnlAdapter::map_memory(std::int64_t address, std::uint32_t length, std::uint8_t io_space,
                                            std::uint8_t user_mode, std::int32_t cache, void **mapped)
{
    *mapped = nullptr;
    if (map_refusal_countdown_ != 0u && --map_refusal_countdown_ == 0u)
        return static_cast<std::int32_t>(map_refusal_status_);
    const int bar = function_.bar_at(static_cast<std::uint64_t>(address));
    if (bar < 0 || io_space != 0u || user_mode != 0u || cache != 0 || mapping_count_ == max_mappings) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "bad DxgkCbMapMemory of 0x%llx",
                                                static_cast<unsigned long long>(address));
        return invalid_parameter;
    }
    const auto base = reinterpret_cast<std::uint8_t *>(virtual_base + (std::uintptr_t{index_} << 40u) + (std::uintptr_t{maps_} << 32u));
    for (std::uint64_t offset = 0u; offset < length; offset += region_step) {
        RegisterWindow *window = function_.region(static_cast<std::uint8_t>(bar), offset);
        if (window != nullptr) platform_.attach_register_window(base + offset, *window);
    }
    mappings_[mapping_count_++] = Mapping{base, static_cast<std::uint8_t>(bar), length};
    maps_++;
    *mapped = base;
    return success;
}

std::int32_t FakeDxgkrnlAdapter::unmap_memory(void *mapped)
{
    for (std::uint32_t i = 0u; i < mapping_count_; i++) {
        if (mappings_[i].address != mapped) continue;
        platform_.detach_register_windows(mapped, mappings_[i].length);
        mappings_[i] = mappings_[--mapping_count_];
        return static_cast<std::int32_t>(unmap_status_);
    }
    TestRegistry::instance().record_failure(__FILE__, __LINE__, "DxgkCbUnmapMemory of unmapped %p", mapped);
    return invalid_parameter;
}

std::uint8_t FakeDxgkrnlAdapter::queue_dpc()
{
    dpcs_++;
    return 1u;
}

}  // namespace av

namespace av {

std::uint32_t FakeDxgkrnlAdapter::interrupts() const { return interrupt_count_; }

const DxgkargcbNotifyInterruptDataRecord &FakeDxgkrnlAdapter::interrupt(std::uint32_t index) const
{
    return interrupts_[index < max_interrupts ? index : max_interrupts - 1u];
}

std::uint32_t FakeDxgkrnlAdapter::dpc_notifications() const { return dpc_notifications_; }
std::uint32_t FakeDxgkrnlAdapter::child_statuses() const { return child_status_count_; }
std::uint32_t FakeDxgkrnlAdapter::child_status_uid(std::uint32_t index) const
{
    return index < max_child_statuses ? child_status_uids_[index] : 0u;
}
bool FakeDxgkrnlAdapter::child_status_connected(std::uint32_t index) const
{
    return index < max_child_statuses && child_status_connected_[index];
}

void FakeDxgkrnlAdapter::refuse_synchronize(std::uint32_t status) { synchronize_status_ = status; }

/* DxgkCbSynchronizeExecution: the routine runs at device IRQL with the ISR's
 * lock held, so a notification is legal inside it and nowhere else outside the
 * ISR. */
std::int32_t FakeDxgkrnlAdapter::synchronize_execution(SynchronizeRoutine routine, void *context,
                                                       std::uint32_t, std::uint8_t *returned)
{
    if (synchronize_status_ != 0u) return static_cast<std::int32_t>(synchronize_status_);
    const Irql previous = platform_.current_irql();
    platform_.set_irql(Irql::isr);
    synchronized_ = true;
    *returned = routine(context);
    synchronized_ = false;
    platform_.set_irql(previous);
    return 0;
}

void FakeDxgkrnlAdapter::notify_interrupt(const DxgkargcbNotifyInterruptDataRecord &data)
{
    if (!synchronized_ && platform_.current_irql() != Irql::isr)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "DxgkCbNotifyInterrupt outside the ISR");
    /* dxgkrnl never processes DMA_FAULTED from this adapter: on a GPU MMU
     * adapter VidSchDdiNotifyInterruptWorker (dxgmms2.sys 26100 @140031b80)
     * reaches WdLogSingleEntry5(0, 0x119, 0xD, node, engine, fence) for every
     * one, and a real guest bugchecked 0x119 VIDEO_SCHEDULER_INTERNAL_ERROR
     * on the first. */
    if (data.InterruptType == dma_faulted) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "DXGK_INTERRUPT_DMA_FAULTED for fence %u bugchecks 0x119/0xD",
                                                data.FenceId);
        return;
    }
    if (interrupt_count_ < max_interrupts) interrupts_[interrupt_count_] = data;
    interrupt_count_++;
    if (sink_ != nullptr) sink_(sink_context_, data);
}

void FakeDxgkrnlAdapter::on_interrupt(InterruptSink sink, void *context)
{
    sink_ = sink;
    sink_context_ = context;
}

void FakeDxgkrnlAdapter::notify_dpc()
{
    if (platform_.current_irql() != Irql::dispatch)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "DxgkCbNotifyDpc outside the DPC");
    dpc_notifications_++;
}

}  // namespace av

#include "fake_dxgkrnl_adapter.hpp"

#include "test_registry.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::int32_t success = 0;
constexpr std::int32_t invalid_parameter = static_cast<std::int32_t>(0xC000000Du);

}  // namespace

FakeDxgkrnlAdapter::FakeDxgkrnlAdapter(MockPlatform &platform, FakeVirtioGpuFunction &function, std::uint8_t index)
    : platform_(platform), function_(function), record_(), device_information_status_(0u),
      config_read_status_(0u), short_config_read_(false), map_refusal_countdown_(0u),
      map_refusal_status_(0u), unmap_status_(0u), mappings_{}, mapping_count_(0u), maps_(0u), dpcs_(0u), index_(index)
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
    record_.DxgkCbQueueDpc = +[](void *handle) {
        return static_cast<FakeDxgkrnlAdapter *>(handle)->queue_dpc();
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

std::int32_t FakeDxgkrnlAdapter::get_device_information(DxgkDeviceInfoRecord *info)
{
    std::memset(info, 0, sizeof *info);
    info->TranslatedResourceList = function_.resources().bytes();
    info->PhysicalDeviceObject = &function_;
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

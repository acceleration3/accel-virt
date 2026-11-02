#include "miniport/adapter/adapter_description.hpp"

#include "miniport/adapter/utf16_text.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/context/gpu_identity.hpp"

namespace av {

namespace {

constexpr char adapter_string[] = "HardwareInformation.AdapterString";
constexpr char chip_type[]      = "HardwareInformation.ChipType";
constexpr char dac_type[]       = "HardwareInformation.DacType";
constexpr char memory_size[]    = "HardwareInformation.MemorySize";
constexpr char wide_memory[]    = "HardwareInformation.qwMemorySize";
constexpr char no_dac[]         = "Internal";

}

AdapterDescription::AdapterDescription(Platform &platform) : platform_(platform), published_(0u)
{
    AV_SCOPE("AdapterDescription::AdapterDescription");
}

Status AdapterDescription::publish(void *physical_device_object, const GpuIdentity &gpu)
{
    AV_SCOPE("AdapterDescription::publish");
    platform_.require_irql_at_most(Irql::passive, "AdapterDescription::publish");
    const Utf16Text name(gpu.name());
    const Utf16Text dac(no_dac);
    const uint64_t  memory = gpu.local_memory();
    Status          first = Status::ok;
    const auto      keep = [&first](Status status) {
        if (first == Status::ok) first = status;
    };
    if (!name.empty()) {
        keep(platform_.set_device_description(physical_device_object, name.units(), name.bytes()));
        keep(platform_.write_driver_text(physical_device_object, adapter_string, name.units(), name.bytes()));
        keep(platform_.write_driver_text(physical_device_object, chip_type, name.units(), name.bytes()));
    }
    keep(platform_.write_driver_text(physical_device_object, dac_type, dac.units(), dac.bytes()));
    keep(platform_.write_driver_u32(physical_device_object, memory_size,
                                    static_cast<uint32_t>(memory > largest_u32 ? largest_u32 : memory)));
    keep(platform_.write_driver_u64(physical_device_object, wide_memory, memory));
    if (first == Status::ok) published_++;
    return first;
}

uint32_t AdapterDescription::published() const
{
    AV_SCOPE("AdapterDescription::published");
    platform_.require_irql_at_most(Irql::dispatch, "AdapterDescription::published");
    return published_;
}

}

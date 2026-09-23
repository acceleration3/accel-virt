/* The host GPU's name and memory, written where Windows reads an adapter's
 * description, at every StartDevice that read the device's capset.
 *
 * DEVPKEY_Device_DeviceDesc is the name Device Manager and WMI's
 * Win32_VideoController show, and the one DXGI's and DXCore's Description
 * come from: dxgkrnl's DpiFdoInitializeFdo reads DevicePropertyDeviceDescription
 * at AddDevice and DXGADAPTER::GetDeviceDescriptor answers that copy, so a
 * name written here reaches DXGI from the next AddDevice on - dxgkrnl's own
 * GPU-P path writes a host GPU's name into the same property the same way
 * (DpiSetDriverVersion). The INF names the device before the first start;
 * this keeps the name the host's when the package is not the one that named
 * it. FriendlyName is left alone: dxgkrnl never reads it.
 *
 * The HardwareInformation values in the driver key are what WMI reads for
 * VideoProcessor (ChipType), AdapterDACType (DacType) and AdapterRAM
 * (qwMemorySize, else MemorySize), and what dxgkrnl answers
 * KMTQAITYPE_ADAPTERREGISTRYINFO with on every query; BasicDisplay writes the
 * same set at its StartDevice. */
#ifndef AV_ADAPTER_DESCRIPTION_HPP
#define AV_ADAPTER_DESCRIPTION_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GpuIdentity;

class AdapterDescription {
public:
    explicit AdapterDescription(Platform &platform);
    AdapterDescription(const AdapterDescription &) = delete;
    AdapterDescription &operator=(const AdapterDescription &) = delete;

    /* PASSIVE. Every value is attempted whatever an earlier one answered; the
     * first failure is what returns. A GPU with no name leaves the INF's. */
    [[nodiscard]] Status publish(void *physical_device_object, const GpuIdentity &gpu);
    /* How many publishes wrote every value. */
    uint32_t             published() const;

private:
    static constexpr uint64_t largest_u32 = 0xFFFFFFFFu;

    Platform &platform_;
    uint32_t  published_;
};

}  // namespace av

#endif  // AV_ADAPTER_DESCRIPTION_HPP

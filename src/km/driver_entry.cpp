#include <ntddk.h>

#include "miniport/driver/driver_image.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/image_range.hpp"
#include "km/kernel_platform.hpp"
#include "km/kernel_trace_host.hpp"

namespace {
constinit av::DriverImage<av::KernelPlatform, av::KernelTraceHost> driver_image;

constexpr ULONG pool_tag = 0x676B7672u;

void trace_bugcheck(void *, uint32_t)
{
    AV_SCOPE("trace_bugcheck");
    driver_image.bugcheck();
}

}

extern "C" DRIVER_INITIALIZE DriverEntry;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path)
{
    return driver_image.enter(driver_object, registry_path,
                              av::ImageRange(reinterpret_cast<uintptr_t>(driver_object->DriverStart),
                                             driver_object->DriverSize),
                              &trace_bugcheck, pool_tag);
}

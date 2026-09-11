#include <ntddk.h>

#include "driver_image.hpp"
#include "kernel_platform.hpp"

namespace {

/* The kernel runs no global constructors: this storage is trivially
 * initialized, and DriverEntry constructs what lives in it. */
constinit av::DriverImage<av::KernelPlatform> driver_image;

/* "rvkg", as pool-tag tools display it. */
constexpr ULONG pool_tag = 0x676B7672u;

}  // namespace

extern "C" DRIVER_INITIALIZE DriverEntry;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path)
{
    return driver_image.enter(driver_object, registry_path, pool_tag);
}

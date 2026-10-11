/* nvapi64.dll's decisions: the NVAPI calls an application makes to find an
 * NVIDIA GPU, answered for the avgpu adapters that present an NVIDIA GPU
 * (docs/vendor-libraries.md). Each returns an NvAPI_Status.
 *
 * As with ADL, only what can be said truthfully is here: the GPUs, their
 * names, PCI identifiers and location, kind, memory and LUID, the driver
 * package's version, and the display the primary display is. NVAPI reaches a
 * function by its ID through nvapi_QueryInterface, and an ID this library does
 * not answer is null there - what an application already meets on a driver
 * that predates the call. With no NVIDIA adapter, initialisation answers
 * NVAPI_NVIDIA_DEVICE_NOT_FOUND, as NVAPI does on any other machine.
 *
 * A physical GPU handle is the adapter's own entry; a display handle is a
 * token of this root's, one per adapter, standing for the display on it. */
#ifndef AV_VENDOR_NVAPI_ROOT_HPP
#define AV_VENDOR_NVAPI_ROOT_HPP

#include "icd/vendor/vendor_adapters.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}  // namespace av

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::vendor {

struct NvDisplayDriverMemoryInfoRecord;
class VendorAdapter;

class NvapiRoot {
public:
    /* nvapi_lite_common.h's status codes this library answers. */
    static constexpr int ok = 0;
    static constexpr int error = -1;
    static constexpr int not_initialized = -4;
    static constexpr int invalid_argument = -5;
    static constexpr int device_not_found = -6;
    static constexpr int end_enumeration = -7;
    static constexpr int incompatible_struct_version = -9;
    static constexpr int expected_physical_gpu_handle = -101;
    static constexpr int expected_display_handle = -102;
    /* NVAPI_MAX_PHYSICAL_GPUS and NVAPI_SHORT_STRING_MAX. */
    static constexpr uint32_t max_physical_gpus = 64u;
    static constexpr uint32_t short_string = 64u;
    /* NV_SYSTEM_TYPE_IGPU and NV_SYSTEM_TYPE_DGPU. */
    static constexpr int gpu_integrated = 1;
    static constexpr int gpu_discrete = 2;
    static constexpr uint32_t nvidia_vendor = 0x10DEu;

    NvapiRoot(Kmt &kmt, icd::ThreadLock &lock);
    NvapiRoot(const NvapiRoot &) = delete;
    NvapiRoot &operator=(const NvapiRoot &) = delete;

    /* NvAPI_Initialize: learns the GPUs; each is undone by one unload. */
    int initialize();
    int unload();

    /* Handles to every GPU into `gpus`, which holds max_physical_gpus. */
    int enum_physical_gpus(void **gpus, uint32_t *count);
    int full_name(const void *gpu, char *name);
    /* The device and subsystem as NVAPI packs them: the device ID over the
     * vendor's, the subsystem over its vendor's. */
    int pci_identifiers(const void *gpu, uint32_t *device_id, uint32_t *subsystem_id, uint32_t *revision,
                        uint32_t *external_device_id);
    int bus_id(const void *gpu, uint32_t *bus);
    int bus_slot_id(const void *gpu, uint32_t *slot);
    int gpu_type(const void *gpu, int *type);
    /* Version 1, 2 or 3 of the memory record, as its version field says, in
     * KiB. Nothing is evicted and nothing is held back, so the available
     * figures are the whole and the eviction figures zero. */
    int memory_info(const void *gpu, NvDisplayDriverMemoryInfoRecord *memory);
    /* NvAPI_GPU_GetAdapterIdFromPhysicalGpu: the adapter's LUID, eight bytes. */
    int adapter_id(const void *gpu, void *luid);
    /* The driver package's version as NVAPI writes a driver's - major and
     * minor, 35.05 as 3505 - and its full version as the branch. */
    int driver_and_branch_version(uint32_t *version, char *branch);
    /* The `index`th display NVIDIA GPUs drive: the primary display, when it
     * is on one of them. */
    int enum_display(uint32_t index, void **display);
    /* The GPUs driving `display`, into `gpus` holding max_physical_gpus. */
    int gpus_from_display(const void *display, void **gpus, uint32_t *count);

private:
    /* The adapter `gpu` is the handle of, or null - also when uninitialised. */
    const VendorAdapter *adapter(const void *gpu) const;

    icd::ThreadLock &lock_;
    VendorAdapters   adapters_;
    uint32_t         opened_;
    /* One token per adapter, whose address is the display handle. */
    char             displays_[VendorAdapters::max_adapters];
};

}  // namespace av::vendor

#endif  // AV_VENDOR_NVAPI_ROOT_HPP

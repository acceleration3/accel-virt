/* nvapi64.dll's root behind its exports (nvapi_exports.cpp), one per process
 * in their storage. Translation only: what is answered is decided in
 * src/icd/vendor/nvapi_root.cpp. */
#ifndef AV_UM_NVAPI_LIBRARY_HPP
#define AV_UM_NVAPI_LIBRARY_HPP

#include "icd/vendor/nvapi_root.hpp"
#include "um/vendor/vendor_driver.hpp"

#include <stdint.h>

namespace av::vendor {
struct NvDisplayDriverMemoryInfoRecord;
}  // namespace av::vendor

namespace av {

class NvapiLibrary {
public:
    constexpr NvapiLibrary();
    NvapiLibrary(const NvapiLibrary &) = delete;
    NvapiLibrary &operator=(const NvapiLibrary &) = delete;

    int initialize();
    int unload();
    int enum_physical_gpus(void **gpus, uint32_t *count);
    int full_name(const void *gpu, char *name);
    int pci_identifiers(const void *gpu, uint32_t *device_id, uint32_t *subsystem_id, uint32_t *revision,
                        uint32_t *external_device_id);
    int bus_id(const void *gpu, uint32_t *bus);
    int bus_slot_id(const void *gpu, uint32_t *slot);
    int gpu_type(const void *gpu, int *type);
    int memory_info(const void *gpu, vendor::NvDisplayDriverMemoryInfoRecord *memory);
    int adapter_id(const void *gpu, void *luid);
    int driver_and_branch_version(uint32_t *version, char *branch);
    int enum_display(uint32_t index, void **display);
    int gpus_from_display(const void *display, void **gpus, uint32_t *count);

private:
    VendorDriver<vendor::NvapiRoot> driver_;
};

constexpr NvapiLibrary::NvapiLibrary() : driver_() {}

}  // namespace av

#endif  // AV_UM_NVAPI_LIBRARY_HPP

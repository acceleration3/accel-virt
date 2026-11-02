#ifndef AV_VENDOR_NVAPI_ROOT_HPP
#define AV_VENDOR_NVAPI_ROOT_HPP

#include "icd/vendor/vendor_adapters.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class ThreadLock;
}

namespace av::vendor {

struct NvDisplayDriverMemoryInfoRecord;
class VendorAdapter;

class NvapiRoot {
public:
    static constexpr int ok = 0;
    static constexpr int error = -1;
    static constexpr int not_initialized = -4;
    static constexpr int invalid_argument = -5;
    static constexpr int device_not_found = -6;
    static constexpr int end_enumeration = -7;
    static constexpr int incompatible_struct_version = -9;
    static constexpr int expected_physical_gpu_handle = -101;
    static constexpr int expected_display_handle = -102;
    static constexpr uint32_t max_physical_gpus = 64u;
    static constexpr uint32_t short_string = 64u;
    static constexpr int gpu_integrated = 1;
    static constexpr int gpu_discrete = 2;

    NvapiRoot(Kmt &kmt, ThreadLock &lock);
    NvapiRoot(const NvapiRoot &) = delete;
    NvapiRoot &operator=(const NvapiRoot &) = delete;

    int initialize();
    int unload();

    int enum_physical_gpus(void **gpus, uint32_t *count);
    int full_name(const void *gpu, char *name);
    int pci_identifiers(const void *gpu, uint32_t *device_id, uint32_t *subsystem_id, uint32_t *revision,
                        uint32_t *external_device_id);
    int bus_id(const void *gpu, uint32_t *bus);
    int bus_slot_id(const void *gpu, uint32_t *slot);
    int gpu_type(const void *gpu, int *type);
    int memory_info(const void *gpu, NvDisplayDriverMemoryInfoRecord *memory);
    int adapter_id(const void *gpu, void *luid);
    int driver_and_branch_version(uint32_t *version, char *branch);
    int enum_display(uint32_t index, void **display);
    int gpus_from_display(const void *display, void **gpus, uint32_t *count);

private:
    const VendorAdapter *adapter(const void *gpu) const;

    ThreadLock      &lock_;
    VendorAdapters   adapters_;
    uint32_t         opened_;
    char             displays_[VendorAdapters::max_adapters];
};

}

#endif

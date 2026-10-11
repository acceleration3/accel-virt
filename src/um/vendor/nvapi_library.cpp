#include "um/vendor/nvapi_library.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

/* A process whose first call could not be run has no root: NVAPI's answer
 * where its support library is missing. */
namespace {
constexpr int no_library = -2;   /* NVAPI_LIBRARY_NOT_FOUND */
}  // namespace

int NvapiLibrary::initialize()
{
    AV_SCOPE("NvapiLibrary::initialize");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->initialize() : no_library;
}

int NvapiLibrary::unload()
{
    AV_SCOPE("NvapiLibrary::unload");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->unload() : no_library;
}

int NvapiLibrary::enum_physical_gpus(void **gpus, uint32_t *count)
{
    AV_SCOPE("NvapiLibrary::enum_physical_gpus");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->enum_physical_gpus(gpus, count) : no_library;
}

int NvapiLibrary::full_name(const void *gpu, char *name)
{
    AV_SCOPE("NvapiLibrary::full_name");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->full_name(gpu, name) : no_library;
}

int NvapiLibrary::pci_identifiers(const void *gpu, uint32_t *device_id, uint32_t *subsystem_id, uint32_t *revision,
                                  uint32_t *external_device_id)
{
    AV_SCOPE("NvapiLibrary::pci_identifiers");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr
               ? answering->pci_identifiers(gpu, device_id, subsystem_id, revision, external_device_id)
               : no_library;
}

int NvapiLibrary::bus_id(const void *gpu, uint32_t *bus)
{
    AV_SCOPE("NvapiLibrary::bus_id");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->bus_id(gpu, bus) : no_library;
}

int NvapiLibrary::bus_slot_id(const void *gpu, uint32_t *slot)
{
    AV_SCOPE("NvapiLibrary::bus_slot_id");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->bus_slot_id(gpu, slot) : no_library;
}

int NvapiLibrary::gpu_type(const void *gpu, int *type)
{
    AV_SCOPE("NvapiLibrary::gpu_type");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->gpu_type(gpu, type) : no_library;
}

int NvapiLibrary::memory_info(const void *gpu, vendor::NvDisplayDriverMemoryInfoRecord *memory)
{
    AV_SCOPE("NvapiLibrary::memory_info");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->memory_info(gpu, memory) : no_library;
}

int NvapiLibrary::adapter_id(const void *gpu, void *luid)
{
    AV_SCOPE("NvapiLibrary::adapter_id");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->adapter_id(gpu, luid) : no_library;
}

int NvapiLibrary::driver_and_branch_version(uint32_t *version, char *branch)
{
    AV_SCOPE("NvapiLibrary::driver_and_branch_version");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->driver_and_branch_version(version, branch) : no_library;
}

int NvapiLibrary::enum_display(uint32_t index, void **display)
{
    AV_SCOPE("NvapiLibrary::enum_display");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->enum_display(index, display) : no_library;
}

int NvapiLibrary::gpus_from_display(const void *display, void **gpus, uint32_t *count)
{
    AV_SCOPE("NvapiLibrary::gpus_from_display");
    vendor::NvapiRoot *const answering = driver_.root();
    return answering != nullptr ? answering->gpus_from_display(display, gpus, count) : no_library;
}

}  // namespace av

/* nvapi64.dll's entry point (docs/vendor-libraries.md): NVAPI exports one
 * function, nvapi_QueryInterface, which answers each call's ID - the IDs of
 * nvapi_interface.h (NVIDIA NVAPI SDK, MIT) - with the function, or null for a
 * call this library does not make, as NVAPI does for a call a driver predates.
 * Each function is one line into the library in this translation unit's
 * storage, C-linked as an ABI entry point though only nvapi.def's one name is
 * exported - the vendor library's process-lifetime state (constitution III) -
 * and the DLL entry does nothing at attach or detach. NVAPI's functions are
 * cdecl. */
#include "icd/vendor/ddi/nv_display_driver_memory_info_record.hpp"
#include "um/vendor/nvapi_library.hpp"

#include <windows.h>

#include <stdint.h>

/* Each call's ID, as nvapi_interface.h names it. */
namespace {
constexpr uint32_t initialize_call = 0x0150E828u;   /* NvAPI_Initialize */
constexpr uint32_t unload_call = 0xD22BDD7Eu;   /* NvAPI_Unload */
constexpr uint32_t enum_physical_gpus_call = 0xE5AC921Fu;   /* NvAPI_EnumPhysicalGPUs */
constexpr uint32_t full_name_call = 0xCEEE8E9Fu;   /* NvAPI_GPU_GetFullName */
constexpr uint32_t pci_identifiers_call = 0x2DDFB66Eu;   /* NvAPI_GPU_GetPCIIdentifiers */
constexpr uint32_t bus_id_call = 0x1BE0B8E5u;   /* NvAPI_GPU_GetBusId */
constexpr uint32_t bus_slot_id_call = 0x2A0A350Fu;   /* NvAPI_GPU_GetBusSlotId */
constexpr uint32_t gpu_type_call = 0xC33BAEB1u;   /* NvAPI_GPU_GetGPUType */
constexpr uint32_t memory_info_call = 0x07F9B368u;   /* NvAPI_GPU_GetMemoryInfo */
constexpr uint32_t adapter_id_call = 0x0FF07FDEu;   /* NvAPI_GPU_GetAdapterIdFromPhysicalGpu */
constexpr uint32_t driver_and_branch_version_call = 0x2926AAADu;   /* NvAPI_SYS_GetDriverAndBranchVersion */
constexpr uint32_t enum_display_call = 0x9ABDD40Du;   /* NvAPI_EnumNvidiaDisplayHandle */
constexpr uint32_t gpus_from_display_call = 0x34EF9506u;   /* NvAPI_GetPhysicalGPUsFromDisplay */
}  // namespace

static constinit av::NvapiLibrary nvapi_library;

extern "C" BOOL WINAPI avnvapi_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" int __cdecl nvapi_initialize() { return nvapi_library.initialize(); }

extern "C" int __cdecl nvapi_unload() { return nvapi_library.unload(); }

extern "C" int __cdecl nvapi_enum_physical_gpus(void **gpus, uint32_t *count)
{ return nvapi_library.enum_physical_gpus(gpus, count); }

extern "C" int __cdecl nvapi_gpu_full_name(void *gpu, char *name) { return nvapi_library.full_name(gpu, name); }

extern "C" int __cdecl nvapi_gpu_pci_identifiers(void *gpu, uint32_t *device, uint32_t *subsystem, uint32_t *revision,
                                             uint32_t *external)
{ return nvapi_library.pci_identifiers(gpu, device, subsystem, revision, external); }

extern "C" int __cdecl nvapi_gpu_bus_id(void *gpu, uint32_t *bus) { return nvapi_library.bus_id(gpu, bus); }

extern "C" int __cdecl nvapi_gpu_bus_slot_id(void *gpu, uint32_t *slot) { return nvapi_library.bus_slot_id(gpu, slot); }

extern "C" int __cdecl nvapi_gpu_type(void *gpu, int *type) { return nvapi_library.gpu_type(gpu, type); }

extern "C" int __cdecl nvapi_gpu_memory_info(void *gpu, av::vendor::NvDisplayDriverMemoryInfoRecord *memory)
{ return nvapi_library.memory_info(gpu, memory); }

extern "C" int __cdecl nvapi_gpu_adapter_id(void *gpu, void *luid) { return nvapi_library.adapter_id(gpu, luid); }

extern "C" int __cdecl nvapi_driver_and_branch_version(uint32_t *version, char *branch)
{ return nvapi_library.driver_and_branch_version(version, branch); }

extern "C" int __cdecl nvapi_enum_display(uint32_t index, void **display)
{ return nvapi_library.enum_display(index, display); }

extern "C" int __cdecl nvapi_gpus_from_display(void *display, void **gpus, uint32_t *count)
{ return nvapi_library.gpus_from_display(display, gpus, count); }

extern "C" void *__cdecl nvapi_QueryInterface(uint32_t id)
{
    switch (id) {
    case initialize_call: return reinterpret_cast<void *>(&nvapi_initialize);
    case unload_call: return reinterpret_cast<void *>(&nvapi_unload);
    case enum_physical_gpus_call: return reinterpret_cast<void *>(&nvapi_enum_physical_gpus);
    case full_name_call: return reinterpret_cast<void *>(&nvapi_gpu_full_name);
    case pci_identifiers_call: return reinterpret_cast<void *>(&nvapi_gpu_pci_identifiers);
    case bus_id_call: return reinterpret_cast<void *>(&nvapi_gpu_bus_id);
    case bus_slot_id_call: return reinterpret_cast<void *>(&nvapi_gpu_bus_slot_id);
    case gpu_type_call: return reinterpret_cast<void *>(&nvapi_gpu_type);
    case memory_info_call: return reinterpret_cast<void *>(&nvapi_gpu_memory_info);
    case adapter_id_call: return reinterpret_cast<void *>(&nvapi_gpu_adapter_id);
    case driver_and_branch_version_call: return reinterpret_cast<void *>(&nvapi_driver_and_branch_version);
    case enum_display_call: return reinterpret_cast<void *>(&nvapi_enum_display);
    case gpus_from_display_call: return reinterpret_cast<void *>(&nvapi_gpus_from_display);
    default: return nullptr;
    }
}

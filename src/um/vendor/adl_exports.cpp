/* atiadlxx.dll's entry points (docs/vendor-libraries.md): the AMD Display
 * Library calls this library answers, ADL1's and ADL2's, each one line into
 * the library in this translation unit's storage - the vendor library's
 * process-lifetime state (constitution III) - and a DLL entry that does
 * nothing at attach or detach, where the loader lock allows nothing. Every
 * other ADL call is simply not exported: what an application meets on a
 * driver that predates it.
 *
 * ADL's functions are cdecl; its allocation callback alone is stdcall, and
 * this library allocates nothing with it - every answer here fits the
 * caller's own buffer. */
#include "icd/vendor/ddi/adl_adapter_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_x2_record.hpp"
#include "um/vendor/adl_library.hpp"

#include <windows.h>

using AdlAllocate = void *(__stdcall *)(int);

static constinit av::AdlLibrary adl_library;

extern "C" BOOL WINAPI avadl_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" int ADL_Main_Control_Create(AdlAllocate allocate, int)
{ return adl_library.create(allocate != nullptr, nullptr); }

extern "C" int ADL_Main_Control_Destroy() { return adl_library.destroy(nullptr); }

extern "C" int ADL_Main_Control_Refresh() { return adl_library.refresh(nullptr); }

extern "C" int ADL2_Main_Control_Create(AdlAllocate allocate, int, void **context)
{ return adl_library.create(allocate != nullptr, context); }

extern "C" int ADL2_Main_Control_Destroy(void *context) { return adl_library.destroy(context); }

extern "C" int ADL2_Main_Control_Refresh(void *context) { return adl_library.refresh(context); }

extern "C" void *ADL2_Main_Control_GetProcAddress(void *context, void *, char *name)
{ return adl_library.proc_address(context, name); }

extern "C" int ADL_Adapter_NumberOfAdapters_Get(int *count) { return adl_library.number_of_adapters(nullptr, count); }

extern "C" int ADL2_Adapter_NumberOfAdapters_Get(void *context, int *count)
{ return adl_library.number_of_adapters(context, count); }

extern "C" int ADL_Adapter_AdapterInfo_Get(av::vendor::AdlAdapterInfoRecord *infos, int bytes)
{ return adl_library.adapter_info(nullptr, infos, bytes); }

extern "C" int ADL2_Adapter_AdapterInfo_Get(void *context, av::vendor::AdlAdapterInfoRecord *infos, int bytes)
{ return adl_library.adapter_info(context, infos, bytes); }

extern "C" int ADL_Adapter_Active_Get(int index, int *status) { return adl_library.active(nullptr, index, status); }

extern "C" int ADL2_Adapter_Active_Get(void *context, int index, int *status)
{ return adl_library.active(context, index, status); }

extern "C" int ADL_Adapter_ASICFamilyType_Get(int index, int *types, int *valids)
{ return adl_library.asic_family_type(nullptr, index, types, valids); }

extern "C" int ADL2_Adapter_ASICFamilyType_Get(void *context, int index, int *types, int *valids)
{ return adl_library.asic_family_type(context, index, types, valids); }

extern "C" int ADL_Adapter_Primary_Get(int *index) { return adl_library.primary(nullptr, index); }

extern "C" int ADL2_Adapter_Primary_Get(void *context, int *index) { return adl_library.primary(context, index); }

extern "C" int ADL_Graphics_Versions_Get(av::vendor::AdlVersionsInfoRecord *versions)
{ return adl_library.versions(nullptr, versions); }

extern "C" int ADL2_Graphics_Versions_Get(void *context, av::vendor::AdlVersionsInfoRecord *versions)
{ return adl_library.versions(context, versions); }

extern "C" int ADL2_Graphics_VersionsX2_Get(void *context, av::vendor::AdlVersionsInfoX2Record *versions)
{ return adl_library.versions_x2(context, versions); }

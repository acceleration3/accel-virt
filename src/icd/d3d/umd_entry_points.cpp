/* The ABI boundary C requires (constitution III), and the translation unit
 * that defines UmdBinding's one pointer - the D3D user-mode driver's
 * process-lifetime state, for the reason the Vulkan and OpenGL ICDs' roots
 * have theirs. No logic here: each thunk is one line into the root. */
#include "icd/d3d/umd_entry_points.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/ddi/d3d10_2ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_calc_private_device_size_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_table_request_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_adapter_funcs_record.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "icd/d3d/umd_root.hpp"

#include <stddef.h>

namespace av::d3d {

constinit UmdBinding umd_binding;

namespace {

extern "C" Hresult rvkumd_get_supported_versions(void *adapter, uint32_t *entries, uint64_t *words)
{ return umd_binding.root().supported_versions(adapter, entries, words); }

extern "C" Hresult rvkumd_get_caps(void *adapter, D3d102ddiargGetCapsRecord *args)
{ return umd_binding.root().caps(adapter, args); }

extern "C" size_t rvkumd_calc_private_device_size(void *adapter, const D3d10ddiargCalcPrivateDeviceSizeRecord *)
{ return umd_binding.root().private_device_bytes(adapter); }

extern "C" Hresult rvkumd_create_device(void *adapter, D3d10ddiargCreateDeviceRecord *create)
{ return umd_binding.root().create_device(adapter, create); }

extern "C" Hresult rvkumd_create_device12(void *adapter, const void *create)
{ return umd_binding.root().create_device12(adapter, create); }

extern "C" Hresult rvkumd_close_adapter(void *adapter) { return umd_binding.root().close_adapter(adapter); }

extern "C" void rvkumd_destroy_device(void *block) { umd_binding.root().destroy_device(block); }

extern "C" Hresult rvkumd_get_optional_ddi_tables(void *adapter, uint32_t *entries, D3d12ddiTableRequestRecord *requests)
{ return umd_binding.root().optional_ddi_tables(adapter, entries, requests); }

extern "C" Hresult rvkumd_fill_ddi_table(void *adapter, uint32_t table_type, void *, size_t, uint32_t, void *)
{ return umd_binding.root().fill_ddi_table(adapter, table_type); }

}  // namespace

void fill_d3d10_adapter_funcs(D3d102ddiAdapterFuncsRecord &table)
{
    AV_SCOPE("fill_d3d10_adapter_funcs");
    table.pfnCalcPrivateDeviceSize = reinterpret_cast<void *>(&rvkumd_calc_private_device_size);
    table.pfnCreateDevice = reinterpret_cast<void *>(&rvkumd_create_device);
    table.pfnCloseAdapter = reinterpret_cast<void *>(&rvkumd_close_adapter);
    table.pfnGetSupportedVersions = reinterpret_cast<void *>(&rvkumd_get_supported_versions);
    table.pfnGetCaps = reinterpret_cast<void *>(&rvkumd_get_caps);
}

void fill_d3d12_adapter_funcs(D3d12ddiAdapterFuncsRecord &table)
{
    AV_SCOPE("fill_d3d12_adapter_funcs");
    table.pfnCalcPrivateDeviceSize = reinterpret_cast<void *>(&rvkumd_calc_private_device_size);
    table.pfnCreateDevice = reinterpret_cast<void *>(&rvkumd_create_device12);
    table.pfnCloseAdapter = reinterpret_cast<void *>(&rvkumd_close_adapter);
    table.pfnGetSupportedVersions = reinterpret_cast<void *>(&rvkumd_get_supported_versions);
    table.pfnGetCaps = reinterpret_cast<void *>(&rvkumd_get_caps);
    table.pfnGetOptionalDDITables = reinterpret_cast<void *>(&rvkumd_get_optional_ddi_tables);
    table.pfnFillDDITable = reinterpret_cast<void *>(&rvkumd_fill_ddi_table);
    table.pfnDestroyDevice = reinterpret_cast<void *>(&rvkumd_destroy_device);
}

}  // namespace av::d3d

#include "icd/d3d/umd_entry_points.hpp"

#include "core/support/runtime_call.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/ddi/d3d10_2ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_calc_private_device_size_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_table_request_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_funcs_record.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "icd/d3d/umd_root.hpp"

#include <stddef.h>

namespace av::d3d {

constinit UmdBinding umd_binding;

namespace {

extern "C" Hresult AV_RUNTIME_CALL avumd_get_supported_versions(void *adapter, uint32_t *entries, uint64_t *words)
{ return umd_binding.root().supported_versions(adapter, entries, words); }

extern "C" Hresult AV_RUNTIME_CALL avumd_get_caps(void *adapter, D3d102ddiargGetCapsRecord *args)
{ return umd_binding.root().caps(adapter, args); }

extern "C" size_t AV_RUNTIME_CALL avumd_calc_private_device_size(void *adapter, const D3d10ddiargCalcPrivateDeviceSizeRecord *)
{ return umd_binding.root().private_device_bytes(adapter); }

extern "C" Hresult AV_RUNTIME_CALL avumd_create_device(void *adapter, D3d10ddiargCreateDeviceRecord *create)
{ return umd_binding.root().create_device(adapter, create); }

extern "C" Hresult AV_RUNTIME_CALL avumd_create_device12(void *adapter, D3d12ddiargCreateDeviceRecord *create)
{ return umd_binding.root().create_device12(adapter, create); }

extern "C" Hresult AV_RUNTIME_CALL avumd_create_device9(void *adapter, void *create)
{ return umd_binding.root().create_device9(adapter, create); }

extern "C" Hresult AV_RUNTIME_CALL avumd_close_adapter(void *adapter) { return umd_binding.root().close_adapter(adapter); }

extern "C" void AV_RUNTIME_CALL avumd_destroy_device(void *block) { umd_binding.root().destroy_device(block); }

extern "C" Hresult AV_RUNTIME_CALL avumd_get_optional_ddi_tables(void *adapter, uint32_t *entries, D3d12ddiTableRequestRecord *requests)
{ return umd_binding.root().optional_ddi_tables(adapter, entries, requests); }

extern "C" Hresult AV_RUNTIME_CALL avumd_fill_ddi_table(void *adapter, uint32_t table_type, void *table, size_t bytes, uint32_t index, void *runtime_table)
{ return umd_binding.root().fill_ddi_table(adapter, table_type, table, bytes, index, runtime_table); }

}

void fill_d3d10_adapter_funcs(D3d102ddiAdapterFuncsRecord &table)
{
    AV_SCOPE("fill_d3d10_adapter_funcs");
    table.pfnCalcPrivateDeviceSize = reinterpret_cast<void *>(&avumd_calc_private_device_size);
    table.pfnCreateDevice = reinterpret_cast<void *>(&avumd_create_device);
    table.pfnCloseAdapter = reinterpret_cast<void *>(&avumd_close_adapter);
    table.pfnGetSupportedVersions = reinterpret_cast<void *>(&avumd_get_supported_versions);
    table.pfnGetCaps = reinterpret_cast<void *>(&avumd_get_caps);
}

void fill_d3d9_adapter_funcs(D3dddiAdapterFuncsRecord &table)
{
    AV_SCOPE("fill_d3d9_adapter_funcs");
    table.pfnGetCaps = reinterpret_cast<void *>(&avumd_get_caps);
    table.pfnCreateDevice = reinterpret_cast<void *>(&avumd_create_device9);
    table.pfnCloseAdapter = reinterpret_cast<void *>(&avumd_close_adapter);
}

void fill_d3d12_adapter_funcs(D3d12ddiAdapterFuncsRecord &table)
{
    AV_SCOPE("fill_d3d12_adapter_funcs");
    table.pfnCalcPrivateDeviceSize = reinterpret_cast<void *>(&avumd_calc_private_device_size);
    table.pfnCreateDevice = reinterpret_cast<void *>(&avumd_create_device12);
    table.pfnCloseAdapter = reinterpret_cast<void *>(&avumd_close_adapter);
    table.pfnGetSupportedVersions = reinterpret_cast<void *>(&avumd_get_supported_versions);
    table.pfnGetCaps = reinterpret_cast<void *>(&avumd_get_caps);
    table.pfnGetOptionalDDITables = reinterpret_cast<void *>(&avumd_get_optional_ddi_tables);
    table.pfnFillDDITable = reinterpret_cast<void *>(&avumd_fill_ddi_table);
    table.pfnDestroyDevice = reinterpret_cast<void *>(&avumd_destroy_device);
}

}

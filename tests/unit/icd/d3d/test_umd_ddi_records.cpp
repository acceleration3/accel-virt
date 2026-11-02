#include "harness/framework/test_registry.hpp"
#include "icd/d3d/ddi/d3d10_2ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_calc_private_device_size_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_table_request_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_callbacks_record.hpp"
#include "icd/d3d/ddi/dxgi1_6_1_ddi_base_functions_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_base_args_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_base_callbacks_record.hpp"

#include <cstddef>
#include <cstdint>

using namespace av::d3d;

TEST(umd_ddi_records, the_open_adapter_arguments_are_laid_out_as_the_runtimes_pass_them)
{ CHECK_EQ(sizeof(D3d10ddiargOpenAdapterRecord), 40u);
  CHECK_EQ(offsetof(D3d10ddiargOpenAdapterRecord, hRTAdapter), 0u);
  CHECK_EQ(offsetof(D3d10ddiargOpenAdapterRecord, hAdapter), 8u);
  CHECK_EQ(offsetof(D3d10ddiargOpenAdapterRecord, Interface), 16u);
  CHECK_EQ(offsetof(D3d10ddiargOpenAdapterRecord, Version), 20u);
  CHECK_EQ(offsetof(D3d10ddiargOpenAdapterRecord, pAdapterCallbacks), 24u);
  CHECK_EQ(offsetof(D3d10ddiargOpenAdapterRecord, pAdapterFuncs), 32u);

  CHECK_EQ(sizeof(D3d12ddiargOpenAdapterRecord), 32u);
  CHECK_EQ(offsetof(D3d12ddiargOpenAdapterRecord, hRTAdapter), 0u);
  CHECK_EQ(offsetof(D3d12ddiargOpenAdapterRecord, hAdapter), 8u);
  CHECK_EQ(offsetof(D3d12ddiargOpenAdapterRecord, pAdapterCallbacks), 16u);
  CHECK_EQ(offsetof(D3d12ddiargOpenAdapterRecord, pAdapterFuncs), 24u); }

TEST(umd_ddi_records, the_adapter_tables_have_the_slot_counts_the_runtimes_clear)
{ CHECK_EQ(sizeof(D3d102ddiAdapterFuncsRecord), 5u * sizeof(void *));
  CHECK_EQ(offsetof(D3d102ddiAdapterFuncsRecord, pfnGetCaps), 32u);

  CHECK_EQ(sizeof(D3d12ddiAdapterFuncsRecord), 0x40u);
  CHECK_EQ(offsetof(D3d12ddiAdapterFuncsRecord, pfnCloseAdapter), 0x10u);
  CHECK_EQ(offsetof(D3d12ddiAdapterFuncsRecord, pfnGetSupportedVersions), 0x18u);
  CHECK_EQ(offsetof(D3d12ddiAdapterFuncsRecord, pfnGetCaps), 0x20u);
  CHECK_EQ(offsetof(D3d12ddiAdapterFuncsRecord, pfnGetOptionalDDITables), 0x28u);
  CHECK_EQ(offsetof(D3d12ddiAdapterFuncsRecord, pfnFillDDITable), 0x30u);
  CHECK_EQ(offsetof(D3d12ddiAdapterFuncsRecord, pfnDestroyDevice), 0x38u); }

TEST(umd_ddi_records, the_caps_and_size_arguments_are_laid_out_as_the_runtimes_pass_them)
{ CHECK_EQ(sizeof(D3d10ddiargCalcPrivateDeviceSizeRecord), 12u);
  CHECK_EQ(offsetof(D3d10ddiargCalcPrivateDeviceSizeRecord, Flags), 8u);

  CHECK_EQ(sizeof(D3d102ddiargGetCapsRecord), 32u);
  CHECK_EQ(offsetof(D3d102ddiargGetCapsRecord, Type), 0u);
  CHECK_EQ(offsetof(D3d102ddiargGetCapsRecord, pInfo), 8u);
  CHECK_EQ(offsetof(D3d102ddiargGetCapsRecord, pData), 16u);
  CHECK_EQ(offsetof(D3d102ddiargGetCapsRecord, DataSize), 24u);

  CHECK_EQ(sizeof(D3d12ddiTableRequestRecord), 8u); }

TEST(umd_ddi_records, create_device_carries_the_dxgi_seam_by_value)
{ CHECK_EQ(sizeof(D3d10ddiargCreateDeviceRecord), 88u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, Interface), 8u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, Version), 12u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, pKTCallbacks), 16u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, pDeviceFuncs), 24u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, hDrvDevice), 32u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, DXGIBaseDDI), 40u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, hRTCoreLayer), 56u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, pUMCallbacks), 64u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, Flags), 72u);
  CHECK_EQ(offsetof(D3d10ddiargCreateDeviceRecord, ppfnRetrieveSubObject), 80u); }

TEST(umd_ddi_records, the_dxgi_seam_is_two_pointers_and_a_table_of_twenty_two)
{ CHECK_EQ(sizeof(DxgiDdiBaseArgsRecord), 16u);
  CHECK_EQ(offsetof(DxgiDdiBaseArgsRecord, pDXGIDDIBaseFunctions), 8u);

  CHECK_EQ(sizeof(DxgiDdiBaseCallbacksRecord), 5u * sizeof(void *));

  CHECK_EQ(sizeof(Dxgi161DdiBaseFunctionsRecord), 22u * sizeof(void *));
  CHECK_EQ(offsetof(Dxgi161DdiBaseFunctionsRecord, pfnPresent), 0u);
  CHECK_EQ(offsetof(Dxgi161DdiBaseFunctionsRecord, pfnBlt), 6u * sizeof(void *));
  CHECK_EQ(offsetof(Dxgi161DdiBaseFunctionsRecord, pfnPresent1), 16u * sizeof(void *));
  CHECK_EQ(offsetof(Dxgi161DdiBaseFunctionsRecord, pfnReclaimResources1), 21u * sizeof(void *)); }

TEST(umd_ddi_records, the_adapter_callbacks_are_three_and_kept_by_pointer)
{ CHECK_EQ(sizeof(D3dddiAdapterCallbacksRecord), 3u * sizeof(void *));
  CHECK_EQ(offsetof(D3dddiAdapterCallbacksRecord, pfnQueryAdapterInfoCb2), 16u); }

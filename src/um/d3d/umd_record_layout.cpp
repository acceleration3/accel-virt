/* Proves at compile time that each DDI record in src/icd/d3d/ddi/ is laid out
 * byte-for-byte as the structure the Direct3D runtime passes, so the UMD's core
 * can be driven on Linux through the same memory the runtime hands it - what
 * src/km/ddi_record_layout.cpp does for dxgkrnl's side.
 *
 * It also pins the HRESULT values src/icd/d3d/hresult.hpp spells out, because
 * the portable core cannot include winerror.h and a transcription error there
 * would be a wrong answer to the runtime rather than a build failure. */
#include <windows.h>

/* The kit's user-mode headers define no NTSTATUS, and d3dkmddi.h - which
 * d3d10umddi.h includes - is written in it, as src/um/gdi_kmt.cpp does for
 * d3dkmthk.h. */
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;

#include <d3d9types.h>
#include <d3d9caps.h>
#include <d3dumddi.h>
#include <d3d10umddi.h>
#include <d3d12umddi.h>
#include <dxgiddi.h>

#include <stddef.h>

#include "icd/d3d/ddi/d3d10_2ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_calc_private_device_size_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_table_request_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_callbacks_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3dddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/dxgi1_6_1_ddi_base_functions_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_base_args_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_base_callbacks_record.hpp"
#include "icd/d3d/hresult.hpp"

#define AV_SAME_SIZE(record, structure) static_assert(sizeof(av::d3d::record) == sizeof(structure))
#define AV_SAME_OFFSET(record, field, structure, member)                                                           \
    static_assert(offsetof(av::d3d::record, field) == offsetof(structure, member))

AV_SAME_SIZE(D3dddiAdapterCallbacksRecord, D3DDDI_ADAPTERCALLBACKS);
AV_SAME_OFFSET(D3dddiAdapterCallbacksRecord, pfnQueryAdapterInfoCb, D3DDDI_ADAPTERCALLBACKS, pfnQueryAdapterInfoCb);
AV_SAME_OFFSET(D3dddiAdapterCallbacksRecord, pfnGetMultisampleMethodListCb, D3DDDI_ADAPTERCALLBACKS,
               pfnGetMultisampleMethodListCb);
AV_SAME_OFFSET(D3dddiAdapterCallbacksRecord, pfnQueryAdapterInfoCb2, D3DDDI_ADAPTERCALLBACKS, pfnQueryAdapterInfoCb2);

AV_SAME_SIZE(D3dddiargOpenAdapterRecord, D3DDDIARG_OPENADAPTER);
AV_SAME_OFFSET(D3dddiargOpenAdapterRecord, hAdapter, D3DDDIARG_OPENADAPTER, hAdapter);
AV_SAME_OFFSET(D3dddiargOpenAdapterRecord, Interface, D3DDDIARG_OPENADAPTER, Interface);
AV_SAME_OFFSET(D3dddiargOpenAdapterRecord, Version, D3DDDIARG_OPENADAPTER, Version);
AV_SAME_OFFSET(D3dddiargOpenAdapterRecord, pAdapterCallbacks, D3DDDIARG_OPENADAPTER, pAdapterCallbacks);
AV_SAME_OFFSET(D3dddiargOpenAdapterRecord, pAdapterFuncs, D3DDDIARG_OPENADAPTER, pAdapterFuncs);
AV_SAME_OFFSET(D3dddiargOpenAdapterRecord, DriverVersion, D3DDDIARG_OPENADAPTER, DriverVersion);

AV_SAME_SIZE(D3dddiAdapterFuncsRecord, D3DDDI_ADAPTERFUNCS);
AV_SAME_OFFSET(D3dddiAdapterFuncsRecord, pfnGetCaps, D3DDDI_ADAPTERFUNCS, pfnGetCaps);
AV_SAME_OFFSET(D3dddiAdapterFuncsRecord, pfnCreateDevice, D3DDDI_ADAPTERFUNCS, pfnCreateDevice);
AV_SAME_OFFSET(D3dddiAdapterFuncsRecord, pfnCloseAdapter, D3DDDI_ADAPTERFUNCS, pfnCloseAdapter);
/* Direct3D 9's GetCaps argument is Direct3D 10's in all but its name, so one
 * record and one root slot answer both. */
AV_SAME_SIZE(D3d102ddiargGetCapsRecord, D3DDDIARG_GETCAPS);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, Type, D3DDDIARG_GETCAPS, Type);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, pInfo, D3DDDIARG_GETCAPS, pInfo);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, pData, D3DDDIARG_GETCAPS, pData);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, DataSize, D3DDDIARG_GETCAPS, DataSize);
static_assert(D3D_UMD_INTERFACE_VERSION_WIN7 == 0x2003, "the interface d3d9.dll is told the driver speaks");

AV_SAME_SIZE(D3d10ddiargOpenAdapterRecord, D3D10DDIARG_OPENADAPTER);
AV_SAME_OFFSET(D3d10ddiargOpenAdapterRecord, hRTAdapter, D3D10DDIARG_OPENADAPTER, hRTAdapter);
AV_SAME_OFFSET(D3d10ddiargOpenAdapterRecord, hAdapter, D3D10DDIARG_OPENADAPTER, hAdapter);
AV_SAME_OFFSET(D3d10ddiargOpenAdapterRecord, Interface, D3D10DDIARG_OPENADAPTER, Interface);
AV_SAME_OFFSET(D3d10ddiargOpenAdapterRecord, Version, D3D10DDIARG_OPENADAPTER, Version);
AV_SAME_OFFSET(D3d10ddiargOpenAdapterRecord, pAdapterCallbacks, D3D10DDIARG_OPENADAPTER, pAdapterCallbacks);
AV_SAME_OFFSET(D3d10ddiargOpenAdapterRecord, pAdapterFuncs, D3D10DDIARG_OPENADAPTER, pAdapterFuncs_2);

AV_SAME_SIZE(D3d102ddiAdapterFuncsRecord, D3D10_2DDI_ADAPTERFUNCS);
AV_SAME_OFFSET(D3d102ddiAdapterFuncsRecord, pfnCalcPrivateDeviceSize, D3D10_2DDI_ADAPTERFUNCS,
               pfnCalcPrivateDeviceSize);
AV_SAME_OFFSET(D3d102ddiAdapterFuncsRecord, pfnCreateDevice, D3D10_2DDI_ADAPTERFUNCS, pfnCreateDevice);
AV_SAME_OFFSET(D3d102ddiAdapterFuncsRecord, pfnCloseAdapter, D3D10_2DDI_ADAPTERFUNCS, pfnCloseAdapter);
AV_SAME_OFFSET(D3d102ddiAdapterFuncsRecord, pfnGetSupportedVersions, D3D10_2DDI_ADAPTERFUNCS,
               pfnGetSupportedVersions);
AV_SAME_OFFSET(D3d102ddiAdapterFuncsRecord, pfnGetCaps, D3D10_2DDI_ADAPTERFUNCS, pfnGetCaps);

AV_SAME_SIZE(D3d10ddiargCalcPrivateDeviceSizeRecord, D3D10DDIARG_CALCPRIVATEDEVICESIZE);
AV_SAME_OFFSET(D3d10ddiargCalcPrivateDeviceSizeRecord, Interface, D3D10DDIARG_CALCPRIVATEDEVICESIZE, Interface);
AV_SAME_OFFSET(D3d10ddiargCalcPrivateDeviceSizeRecord, Version, D3D10DDIARG_CALCPRIVATEDEVICESIZE, Version);
AV_SAME_OFFSET(D3d10ddiargCalcPrivateDeviceSizeRecord, Flags, D3D10DDIARG_CALCPRIVATEDEVICESIZE, Flags);
/* D3D12's is the same three fields, so one record serves both creates. */
static_assert(sizeof(av::d3d::D3d10ddiargCalcPrivateDeviceSizeRecord) == sizeof(D3D12DDIARG_CALCPRIVATEDEVICESIZE));

AV_SAME_SIZE(D3d102ddiargGetCapsRecord, D3D10_2DDIARG_GETCAPS);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, Type, D3D10_2DDIARG_GETCAPS, Type);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, pInfo, D3D10_2DDIARG_GETCAPS, pInfo);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, pData, D3D10_2DDIARG_GETCAPS, pData);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, DataSize, D3D10_2DDIARG_GETCAPS, DataSize);
/* And D3D12DDIARG_GETCAPS is the same shape, so the one thunk serves both. */
AV_SAME_SIZE(D3d102ddiargGetCapsRecord, D3D12DDIARG_GETCAPS);
AV_SAME_OFFSET(D3d102ddiargGetCapsRecord, DataSize, D3D12DDIARG_GETCAPS, DataSize);

AV_SAME_SIZE(D3d10ddiargCreateDeviceRecord, D3D10DDIARG_CREATEDEVICE);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, hRTDevice, D3D10DDIARG_CREATEDEVICE, hRTDevice);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, Interface, D3D10DDIARG_CREATEDEVICE, Interface);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, Version, D3D10DDIARG_CREATEDEVICE, Version);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, pKTCallbacks, D3D10DDIARG_CREATEDEVICE, pKTCallbacks);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, pDeviceFuncs, D3D10DDIARG_CREATEDEVICE, pDeviceFuncs);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, hDrvDevice, D3D10DDIARG_CREATEDEVICE, hDrvDevice);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, DXGIBaseDDI, D3D10DDIARG_CREATEDEVICE, DXGIBaseDDI);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, hRTCoreLayer, D3D10DDIARG_CREATEDEVICE, hRTCoreLayer);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, pUMCallbacks, D3D10DDIARG_CREATEDEVICE, pUMCallbacks);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, Flags, D3D10DDIARG_CREATEDEVICE, Flags);
AV_SAME_OFFSET(D3d10ddiargCreateDeviceRecord, ppfnRetrieveSubObject, D3D10DDIARG_CREATEDEVICE,
               ppfnRetrieveSubObject);

AV_SAME_SIZE(D3d12ddiargOpenAdapterRecord, D3D12DDIARG_OPENADAPTER);
AV_SAME_OFFSET(D3d12ddiargOpenAdapterRecord, hRTAdapter, D3D12DDIARG_OPENADAPTER, hRTAdapter);
AV_SAME_OFFSET(D3d12ddiargOpenAdapterRecord, hAdapter, D3D12DDIARG_OPENADAPTER, hAdapter);
AV_SAME_OFFSET(D3d12ddiargOpenAdapterRecord, pAdapterCallbacks, D3D12DDIARG_OPENADAPTER, pAdapterCallbacks);
AV_SAME_OFFSET(D3d12ddiargOpenAdapterRecord, pAdapterFuncs, D3D12DDIARG_OPENADAPTER, pAdapterFuncs);

/* Eight slots are the region D3D12Core clears before the open (research.md
 * §12): 0x40 bytes in a 64-bit process, 0x20 in a 32-bit one. */
AV_SAME_SIZE(D3d12ddiAdapterFuncsRecord, D3D12DDI_ADAPTERFUNCS);
static_assert(sizeof(D3D12DDI_ADAPTERFUNCS) == 8u * sizeof(void *));
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnCalcPrivateDeviceSize, D3D12DDI_ADAPTERFUNCS,
               pfnCalcPrivateDeviceSize);
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnCreateDevice, D3D12DDI_ADAPTERFUNCS, pfnCreateDevice);
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnCloseAdapter, D3D12DDI_ADAPTERFUNCS, pfnCloseAdapter);
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnGetSupportedVersions, D3D12DDI_ADAPTERFUNCS, pfnGetSupportedVersions);
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnGetCaps, D3D12DDI_ADAPTERFUNCS, pfnGetCaps);
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnGetOptionalDDITables, D3D12DDI_ADAPTERFUNCS, pfnGetOptionalDDITables);
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnFillDDITable, D3D12DDI_ADAPTERFUNCS, pfnFillDDITable);
AV_SAME_OFFSET(D3d12ddiAdapterFuncsRecord, pfnDestroyDevice, D3D12DDI_ADAPTERFUNCS, pfnDestroyDevice);

AV_SAME_SIZE(D3d12ddiargCreateDeviceRecord, D3D12DDIARG_CREATEDEVICE_0109);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, hRTDevice, D3D12DDIARG_CREATEDEVICE_0109, hRTDevice);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, Interface, D3D12DDIARG_CREATEDEVICE_0109, Interface);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, Version, D3D12DDIARG_CREATEDEVICE_0109, Version);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, pKTCallbacks, D3D12DDIARG_CREATEDEVICE_0109, pKTCallbacks);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, hDrvDevice, D3D12DDIARG_CREATEDEVICE_0109, hDrvDevice);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, p12UMCallbacks, D3D12DDIARG_CREATEDEVICE_0109, p12UMCallbacks);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, Flags, D3D12DDIARG_CREATEDEVICE_0109, Flags);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, pReserveRanges, D3D12DDIARG_CREATEDEVICE_0109, pReserveRanges);
AV_SAME_OFFSET(D3d12ddiargCreateDeviceRecord, NumReserveRanges, D3D12DDIARG_CREATEDEVICE_0109, NumReserveRanges);

AV_SAME_SIZE(D3d12ddiTableRequestRecord, D3D12DDI_TABLE_REQUEST);
AV_SAME_OFFSET(D3d12ddiTableRequestRecord, tableType, D3D12DDI_TABLE_REQUEST, tableType);
AV_SAME_OFFSET(D3d12ddiTableRequestRecord, numTables, D3D12DDI_TABLE_REQUEST, numTables);

AV_SAME_SIZE(DxgiDdiBaseArgsRecord, DXGI_DDI_BASE_ARGS);
AV_SAME_OFFSET(DxgiDdiBaseArgsRecord, pDXGIBaseCallbacks, DXGI_DDI_BASE_ARGS, pDXGIBaseCallbacks);
AV_SAME_OFFSET(DxgiDdiBaseArgsRecord, pDXGIDDIBaseFunctions, DXGI_DDI_BASE_ARGS, pDXGIDDIBaseFunctions6_1);

AV_SAME_SIZE(DxgiDdiBaseCallbacksRecord, DXGI_DDI_BASE_CALLBACKS);
AV_SAME_OFFSET(DxgiDdiBaseCallbacksRecord, pfnPresentCb, DXGI_DDI_BASE_CALLBACKS, pfnPresentCb);
AV_SAME_OFFSET(DxgiDdiBaseCallbacksRecord, pfnSubmitPresentToHwQueueCb, DXGI_DDI_BASE_CALLBACKS,
               pfnSubmitPresentToHwQueueCb);

AV_SAME_SIZE(Dxgi161DdiBaseFunctionsRecord, DXGI1_6_1_DDI_BASE_FUNCTIONS);
AV_SAME_OFFSET(Dxgi161DdiBaseFunctionsRecord, pfnPresent, DXGI1_6_1_DDI_BASE_FUNCTIONS, pfnPresent);
AV_SAME_OFFSET(Dxgi161DdiBaseFunctionsRecord, pfnGetGammaCaps, DXGI1_6_1_DDI_BASE_FUNCTIONS, pfnGetGammaCaps);
AV_SAME_OFFSET(Dxgi161DdiBaseFunctionsRecord, pfnBlt, DXGI1_6_1_DDI_BASE_FUNCTIONS, pfnBlt);
AV_SAME_OFFSET(Dxgi161DdiBaseFunctionsRecord, pfnGetMultiplaneOverlayCaps, DXGI1_6_1_DDI_BASE_FUNCTIONS,
               pfnGetMultiplaneOverlayCaps);
AV_SAME_OFFSET(Dxgi161DdiBaseFunctionsRecord, pfnPresent1, DXGI1_6_1_DDI_BASE_FUNCTIONS, pfnPresent1);
AV_SAME_OFFSET(Dxgi161DdiBaseFunctionsRecord, pfnTrimResidencySet, DXGI1_6_1_DDI_BASE_FUNCTIONS,
               pfnTrimResidencySet);
AV_SAME_OFFSET(Dxgi161DdiBaseFunctionsRecord, pfnReclaimResources1, DXGI1_6_1_DDI_BASE_FUNCTIONS,
               pfnReclaimResources1);

/* The argument views: shorter than the runtime's structures on purpose, so
 * only the fields named here are checked - what is past them is the runtime's
 * and is never written. */
AV_SAME_OFFSET(DxgiDdiArgRecord, hDevice, DXGI_DDI_ARG_PRESENT, hDevice);
AV_SAME_OFFSET(DxgiDdiArgGetGammaControlCapsRecord, hDevice, DXGI_DDI_ARG_GET_GAMMA_CONTROL_CAPS, hDevice);
AV_SAME_OFFSET(DxgiDdiArgGetGammaControlCapsRecord, pGammaCapabilities, DXGI_DDI_ARG_GET_GAMMA_CONTROL_CAPS,
               pGammaCapabilities);
static_assert(offsetof(av::d3d::DxgiGammaControlCapabilitiesRecord, ScaleAndOffsetSupported) ==
              offsetof(DXGI_GAMMA_CONTROL_CAPABILITIES, ScaleAndOffsetSupported));
static_assert(offsetof(av::d3d::DxgiGammaControlCapabilitiesRecord, NumGammaControlPoints) ==
              offsetof(DXGI_GAMMA_CONTROL_CAPABILITIES, NumGammaControlPoints));
static_assert(sizeof(av::d3d::DxgiGammaControlCapabilitiesRecord) <= sizeof(DXGI_GAMMA_CONTROL_CAPABILITIES));
AV_SAME_OFFSET(DxgiDdiArgGetMultiplaneOverlayCapsRecord, hDevice, DXGI_DDI_ARG_GETMULTIPLANEOVERLAYCAPS, hDevice);
AV_SAME_OFFSET(DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord, hDevice, DXGI_DDI_ARG_GETMULTIPLANEOVERLAYGROUPCAPS,
               hDevice);
AV_SAME_SIZE(DxgiDdiArgCheckPresentDurationSupportRecord, DXGI_DDI_ARG_CHECKPRESENTDURATIONSUPPORT);
AV_SAME_OFFSET(DxgiDdiArgCheckPresentDurationSupportRecord, ClosestSmallerDuration,
               DXGI_DDI_ARG_CHECKPRESENTDURATIONSUPPORT, ClosestSmallerDuration);
AV_SAME_OFFSET(DxgiDdiArgCheckPresentDurationSupportRecord, ClosestLargerDuration,
               DXGI_DDI_ARG_CHECKPRESENTDURATIONSUPPORT, ClosestLargerDuration);
AV_SAME_OFFSET(DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord, Supported,
               DXGI_DDI_ARG_CHECKMULTIPLANEOVERLAYCOLORSPACESUPPORT, Supported);
AV_SAME_SIZE(DxgiDdiArgReclaimResourcesRecord, DXGI_DDI_ARG_RECLAIMRESOURCES);
AV_SAME_OFFSET(DxgiDdiArgReclaimResourcesRecord, pResources, DXGI_DDI_ARG_RECLAIMRESOURCES, pResources);
AV_SAME_OFFSET(DxgiDdiArgReclaimResourcesRecord, pDiscarded, DXGI_DDI_ARG_RECLAIMRESOURCES, pDiscarded);
AV_SAME_OFFSET(DxgiDdiArgReclaimResourcesRecord, Resources, DXGI_DDI_ARG_RECLAIMRESOURCES, Resources);
AV_SAME_SIZE(DxgiDdiArgReclaimResources1Record, DXGI_DDI_ARG_RECLAIMRESOURCES1);
AV_SAME_OFFSET(DxgiDdiArgReclaimResources1Record, pResources, DXGI_DDI_ARG_RECLAIMRESOURCES1, pResources);
AV_SAME_OFFSET(DxgiDdiArgReclaimResources1Record, pResults, DXGI_DDI_ARG_RECLAIMRESOURCES1, pResults);
AV_SAME_OFFSET(DxgiDdiArgReclaimResources1Record, Resources, DXGI_DDI_ARG_RECLAIMRESOURCES1, Resources);
static_assert(sizeof(D3DDDI_RECLAIM_RESULT) == sizeof(uint32_t) && D3DDDI_RECLAIM_RESULT_OK == 0);

/* Every slot 055 fills answers an HRESULT, except the two the WDK declares
 * otherwise. This matters because BOOL and HRESULT both mirror to int32_t, so
 * nothing but this catches a decliner handing E_NOTIMPL to a BOOL-returning
 * slot - where it reads as TRUE, and a "not supported" becomes a "yes"
 * (contracts/dxgi_seam.md). The device-functions tables of 056, 057 and 064 do
 * have BOOL slots; they need the same assertion. */
static_assert(__is_same(HRESULT, int32_t) || __is_same(HRESULT, long));
static_assert(sizeof(HRESULT) == sizeof(av::d3d::Hresult));
static_assert(__is_same(decltype(DXGI1_6_1_DDI_BASE_FUNCTIONS{}.pfnPresent(nullptr)), HRESULT));
static_assert(__is_same(decltype(DXGI1_6_1_DDI_BASE_FUNCTIONS{}.pfnGetGammaCaps(nullptr)), HRESULT));
static_assert(__is_same(decltype(DXGI1_6_1_DDI_BASE_FUNCTIONS{}.pfnSetResourcePriority(nullptr)), HRESULT));
static_assert(__is_same(decltype(DXGI1_6_1_DDI_BASE_FUNCTIONS{}.pfnGetMultiplaneOverlayCaps(nullptr)), HRESULT));
static_assert(__is_same(decltype(DXGI1_6_1_DDI_BASE_FUNCTIONS{}.pfnTrimResidencySet(nullptr)), HRESULT));
static_assert(__is_same(decltype(DXGI1_6_1_DDI_BASE_FUNCTIONS{}.pfnCheckPresentDurationSupport(nullptr)), HRESULT));
static_assert(__is_same(decltype(DXGI1_6_1_DDI_BASE_FUNCTIONS{}.pfnReclaimResources1(nullptr)), HRESULT));
/* The two adapter slots that are not HRESULT, and which the thunks match. */
static_assert(__is_same(decltype(D3D10_2DDI_ADAPTERFUNCS{}.pfnCalcPrivateDeviceSize(
                            D3D10DDI_HADAPTER{}, nullptr)), SIZE_T));
static_assert(__is_same(decltype(D3D12DDI_ADAPTERFUNCS{}.pfnDestroyDevice(D3D12DDI_HDEVICE{})), VOID));

static_assert(av::d3d::hr_ok == S_OK);
static_assert(av::d3d::hr_not_impl == E_NOTIMPL);
static_assert(av::d3d::hr_no_interface == E_NOINTERFACE);
static_assert(av::d3d::hr_fail == E_FAIL);
static_assert(av::d3d::hr_out_of_memory == E_OUTOFMEMORY);
static_assert(av::d3d::hr_invalid_arg == E_INVALIDARG);
static_assert(av::d3d::hr_dxgi_invalid_call == DXGI_ERROR_INVALID_CALL);
static_assert(av::d3d::hr_dxgi_not_found == DXGI_ERROR_NOT_FOUND);
static_assert(av::d3d::hr_dxgi_unsupported == DXGI_ERROR_UNSUPPORTED);
static_assert(av::d3d::hr_dxgi_device_removed == DXGI_ERROR_DEVICE_REMOVED);
static_assert(av::d3d::hr_dxgi_device_hung == DXGI_ERROR_DEVICE_HUNG);
static_assert(av::d3d::hr_dxgi_device_reset == DXGI_ERROR_DEVICE_RESET);

/* The WDDM 3.2 D3D11 DDI word DdiVersionList names (d3d10umddi.h:7969). */
static_assert(D3DWDDM3_2_DDI_SUPPORTED == 0x000B002D00010000ull);

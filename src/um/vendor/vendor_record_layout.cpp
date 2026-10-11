/* Proof that the vendor libraries' records are laid out as their SDKs lay
 * out the structures an application hands them: AMD's Display Library SDK
 * (adl_structures.h) and NVIDIA's NVAPI SDK (nvapi_lite_common.h), both MIT.
 * Neither SDK is in the kit, so the offsets are theirs written down: every
 * field is an int, a char[ADL_MAX_PATH = 256] or an NvU32, with no padding
 * between. A failure here is a compile error in the guest build. */
#include "icd/vendor/ddi/adl_adapter_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_x2_record.hpp"
#include "icd/vendor/ddi/nv_display_driver_memory_info_record.hpp"

#include <stddef.h>

namespace av::vendor {

static_assert(sizeof(AdlAdapterInfoRecord) == 0x624u, "AdapterInfo");
static_assert(offsetof(AdlAdapterInfoRecord, iAdapterIndex) == 0x004u, "AdapterInfo::iAdapterIndex");
static_assert(offsetof(AdlAdapterInfoRecord, strUDID) == 0x008u, "AdapterInfo::strUDID");
static_assert(offsetof(AdlAdapterInfoRecord, iBusNumber) == 0x108u, "AdapterInfo::iBusNumber");
static_assert(offsetof(AdlAdapterInfoRecord, iDeviceNumber) == 0x10Cu, "AdapterInfo::iDeviceNumber");
static_assert(offsetof(AdlAdapterInfoRecord, iFunctionNumber) == 0x110u, "AdapterInfo::iFunctionNumber");
static_assert(offsetof(AdlAdapterInfoRecord, iVendorID) == 0x114u, "AdapterInfo::iVendorID");
static_assert(offsetof(AdlAdapterInfoRecord, strAdapterName) == 0x118u, "AdapterInfo::strAdapterName");
static_assert(offsetof(AdlAdapterInfoRecord, strDisplayName) == 0x218u, "AdapterInfo::strDisplayName");
static_assert(offsetof(AdlAdapterInfoRecord, iPresent) == 0x318u, "AdapterInfo::iPresent");
static_assert(offsetof(AdlAdapterInfoRecord, iExist) == 0x31Cu, "AdapterInfo::iExist");
static_assert(offsetof(AdlAdapterInfoRecord, strDriverPath) == 0x320u, "AdapterInfo::strDriverPath");
static_assert(offsetof(AdlAdapterInfoRecord, strDriverPathExt) == 0x420u, "AdapterInfo::strDriverPathExt");
static_assert(offsetof(AdlAdapterInfoRecord, strPNPString) == 0x520u, "AdapterInfo::strPNPString");
static_assert(offsetof(AdlAdapterInfoRecord, iOSDisplayIndex) == 0x620u, "AdapterInfo::iOSDisplayIndex");

static_assert(sizeof(AdlVersionsInfoRecord) == 3u * 256u, "ADLVersionsInfo");
static_assert(offsetof(AdlVersionsInfoRecord, strCatalystWebLink) == 512u, "ADLVersionsInfo::strCatalystWebLink");
static_assert(sizeof(AdlVersionsInfoX2Record) == 4u * 256u, "ADLVersionsInfoX2");
static_assert(offsetof(AdlVersionsInfoX2Record, strCrimsonVersion) == 512u, "ADLVersionsInfoX2::strCrimsonVersion");

static_assert(sizeof(NvDisplayDriverMemoryInfoRecord) == 32u, "NV_DISPLAY_DRIVER_MEMORY_INFO_V3");
static_assert(offsetof(NvDisplayDriverMemoryInfoRecord, curAvailableDedicatedVideoMemory) == 20u,
              "NV_DISPLAY_DRIVER_MEMORY_INFO_V2 ends after this field");
static_assert(offsetof(NvDisplayDriverMemoryInfoRecord, dedicatedVideoMemoryEvictionsSize) == 24u,
              "NV_DISPLAY_DRIVER_MEMORY_INFO_V3's own fields");

}  // namespace av::vendor

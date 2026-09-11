/* The kernel build proves each record against the WDK. These prove the Linux
 * harness lays the same records out the same way, so a test that fills a
 * record fills the bytes dxgkrnl would read. */
#include "../../src/core/ddi/dxgk_child_descriptor_record.hpp"
#include "../../src/core/ddi/dxgk_child_status_record.hpp"
#include "../../src/core/ddi/dxgk_device_info_record.hpp"
#include "../../src/core/ddi/dxgk_driver_caps_record.hpp"
#include "../../src/core/ddi/dxgk_wddm_device_caps_record.hpp"
#include "../../src/core/ddi/dxgkarg_collect_dbg_info_ext_record.hpp"
#include "../../src/core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "../../src/core/ddi/dxgkarg_escape_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "../../src/core/ddi/dxgkrnl_interface_record.hpp"
#include "../../src/core/ddi/linked_device_record.hpp"
#include "../harness/test_registry.hpp"

#include <cstddef>

using av::DxgkChildDescriptorRecord;
using av::DxgkChildStatusRecord;
using av::DxgkDeviceInfoRecord;
using av::DxgkDriverCapsRecord;
using av::DxgkWddmDeviceCapsRecord;
using av::DxgkargCollectDbgInfoExtRecord;
using av::DxgkargCollectDbgInfoRecord;
using av::DxgkargEscapeRecord;
using av::DxgkargQueryAdapterInfoRecord;
using av::DxgkrnlInterfaceRecord;
using av::LinkedDeviceRecord;

TEST(ddi_records, dxgkrnl_interface_record_is_the_wddm_3_2_table)
{ CHECK_EQ(sizeof(DxgkrnlInterfaceRecord), 0x240u);   /* DpiFdoStartAdapter builds 0x240 bytes */
  CHECK_EQ(offsetof(DxgkrnlInterfaceRecord, DeviceHandle), 8u);
  CHECK_EQ(offsetof(DxgkrnlInterfaceRecord, DxgkCbGetDeviceInformation), 24u);
  CHECK_EQ(offsetof(DxgkrnlInterfaceRecord, DxgkCbMapMemory), 40u);
  CHECK_EQ(offsetof(DxgkrnlInterfaceRecord, DxgkCbQueueDpc), 48u);
  CHECK_EQ(offsetof(DxgkrnlInterfaceRecord, DxgkCbReadDeviceSpace), 64u);
  CHECK_EQ(offsetof(DxgkrnlInterfaceRecord, DxgkCbUnmapMemory), 80u); }

TEST(ddi_records, dxgk_device_info_record_matches_the_x64_layout)
{ CHECK_EQ(sizeof(DxgkDeviceInfoRecord), 80u);
  CHECK_EQ(offsetof(DxgkDeviceInfoRecord, DeviceRegistryPathBuffer), 24u);
  CHECK_EQ(offsetof(DxgkDeviceInfoRecord, TranslatedResourceList), 32u);
  CHECK_EQ(offsetof(DxgkDeviceInfoRecord, DockingState), 72u); }

TEST(ddi_records, dxgk_child_descriptor_record_matches_basicdisplay_writes)
{ CHECK_EQ(sizeof(DxgkChildDescriptorRecord), 28u);
  CHECK_EQ(offsetof(DxgkChildDescriptorRecord, InterfaceTechnology), 4u);
  CHECK_EQ(offsetof(DxgkChildDescriptorRecord, HpdAwareness), 16u);
  CHECK_EQ(offsetof(DxgkChildDescriptorRecord, ChildUid), 24u); }

TEST(ddi_records, dxgk_child_status_record_and_linked_device_record)
{ CHECK_EQ(sizeof(DxgkChildStatusRecord), 16u);
  CHECK_EQ(offsetof(DxgkChildStatusRecord, HotPlugConnected), 8u);
  CHECK_EQ(sizeof(LinkedDeviceRecord), 12u);
  CHECK_EQ(offsetof(LinkedDeviceRecord, LeadLink), 8u); }

TEST(ddi_records, dxgkarg_query_adapter_info_record_matches_basicrender_reads)
{ CHECK_EQ(sizeof(DxgkargQueryAdapterInfoRecord), 0x30u);
  CHECK_EQ(offsetof(DxgkargQueryAdapterInfoRecord, InputDataSize), 0x10u);
  CHECK_EQ(offsetof(DxgkargQueryAdapterInfoRecord, pOutputData), 0x18u);
  CHECK_EQ(offsetof(DxgkargQueryAdapterInfoRecord, OutputDataSize), 0x20u);
  CHECK_EQ(offsetof(DxgkargQueryAdapterInfoRecord, Flags), 0x24u); }

/* DXGADAPTER::Initialize copies the answer to DXGADAPTER+0x978 and reads these
 * fields back at +0x9b8, +0x9bc, +0xac8, +0xb92, +0xb97 and +0xbb8. */
TEST(ddi_records, dxgk_driver_caps_record_matches_the_offsets_dxgkrnl_validates)
{ CHECK_EQ(sizeof(DxgkDriverCapsRecord), 0x250u);
  CHECK_EQ(offsetof(DxgkDriverCapsRecord, SchedulingCaps), 0x40u);
  CHECK_EQ(offsetof(DxgkDriverCapsRecord, MemoryManagementCaps), 0x44u);
  CHECK_EQ(offsetof(DxgkDriverCapsRecord, WDDMVersion), 0x150u);
  CHECK_EQ(offsetof(DxgkDriverCapsRecord, SupportPerEngineTDR), 0x21Au);
  CHECK_EQ(offsetof(DxgkDriverCapsRecord, HybridDiscrete), 0x21Fu);
  CHECK_EQ(offsetof(DxgkDriverCapsRecord, MiscCaps), 0x240u);
  CHECK_EQ(sizeof(DxgkWddmDeviceCapsRecord), 4u); }

TEST(ddi_records, dxgkarg_escape_and_collect_dbg_info_records)
{ CHECK_EQ(sizeof(DxgkargEscapeRecord), 0x30u);
  CHECK_EQ(offsetof(DxgkargEscapeRecord, pPrivateDriverData), 0x10u);
  CHECK_EQ(offsetof(DxgkargEscapeRecord, PrivateDriverDataSize), 0x18u);
  CHECK_EQ(offsetof(DxgkargEscapeRecord, hContext), 0x20u);
  CHECK_EQ(sizeof(DxgkargCollectDbgInfoRecord), 0x20u);
  CHECK_EQ(offsetof(DxgkargCollectDbgInfoRecord, BufferSize), 0x10u);
  CHECK_EQ(offsetof(DxgkargCollectDbgInfoRecord, pExtension), 0x18u);
  CHECK_EQ(sizeof(DxgkargCollectDbgInfoExtRecord), 0x20u); }

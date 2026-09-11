/* The kernel build proves each record against the WDK. These prove the Linux
 * harness lays the same records out the same way, so a test that fills a
 * record fills the bytes dxgkrnl would read. */
#include "../../src/core/ddi/dxgk_child_descriptor_record.hpp"
#include "../../src/core/ddi/dxgk_child_status_record.hpp"
#include "../../src/core/ddi/dxgk_device_info_record.hpp"
#include "../../src/core/ddi/dxgkrnl_interface_record.hpp"
#include "../../src/core/ddi/linked_device_record.hpp"
#include "../harness/test_registry.hpp"

#include <cstddef>

using av::DxgkChildDescriptorRecord;
using av::DxgkChildStatusRecord;
using av::DxgkDeviceInfoRecord;
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

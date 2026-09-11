/* The kernel build proves each record against the WDK. These prove the Linux
 * harness lays the same records out the same way, so a test that fills a
 * record fills the bytes dxgkrnl would read. */
#include "core/ddi/d3dddi_patch_location_list_record.hpp"
#include "core/ddi/d3dkmdt_gdi_surface_data_record.hpp"
#include "core/ddi/d3dkmdt_monitor_source_mode_record.hpp"
#include "core/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "core/ddi/d3dkmdt_shared_primary_surface_data_record.hpp"
#include "core/ddi/d3dkmdt_staging_surface_data_record.hpp"
#include "core/ddi/d3dkmdt_video_signal_info_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_target_mode_record.hpp"
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_allocation_list_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_copy_page_table_entries_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_copy_range_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_fill_virtual_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_flush_tlb_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_transfer_virtual_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "core/ddi/dxgk_child_container_id_record.hpp"
#include "core/ddi/dxgk_child_descriptor_record.hpp"
#include "core/ddi/dxgk_child_status_record.hpp"
#include "core/ddi/dxgk_device_descriptor_record.hpp"
#include "core/ddi/dxgk_device_info_record.hpp"
#include "core/ddi/dxgk_display_information_record.hpp"
#include "core/ddi/dxgk_driver_caps_record.hpp"
#include "core/ddi/dxgk_gpu_clock_data_record.hpp"
#include "core/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "core/ddi/dxgk_node_metadata_record.hpp"
#include "core/ddi/dxgk_open_allocation_info_record.hpp"
#include "core/ddi/dxgk_page_table_level_desc_record.hpp"
#include "core/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "core/ddi/dxgk_present_allocation_info_record.hpp"
#include "core/ddi/dxgk_pte_record.hpp"
#include "core/ddi/dxgk_query_page_table_level_desc_in_record.hpp"
#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/ddi/dxgk_vidpn_interface_records.hpp"
#include "core/ddi/dxgk_wddm_device_caps_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/ddi/dxgkarg_cancel_command_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_collect_dbg_info_ext_record.hpp"
#include "core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "core/ddi/dxgkarg_control_interrupt2_record.hpp"
#include "core/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_create_process_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "core/ddi/dxgkarg_escape_record.hpp"
#include "core/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "core/ddi/dxgkarg_get_scan_line_record.hpp"
#include "core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/ddi/dxgkarg_patch_record.hpp"
#include "core/ddi/dxgkarg_preempt_command_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "core/ddi/dxgkarg_query_dependent_engine_group_record.hpp"
#include "core/ddi/dxgkarg_query_engine_status_record.hpp"
#include "core/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
#include "core/ddi/dxgkarg_recommend_functional_vidpn_record.hpp"
#include "core/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "core/ddi/dxgkarg_recommend_vidpn_topology_record.hpp"
#include "core/ddi/dxgkarg_render_record.hpp"
#include "core/ddi/dxgkarg_reset_engine_record.hpp"
#include "core/ddi/dxgkarg_reset_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_resume_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/ddi/dxgkarg_set_stable_power_state_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_address_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_visibility_record.hpp"
#include "core/ddi/dxgkarg_submit_command_record.hpp"
#include "core/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "core/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "core/ddi/dxgkargcb_notify_interrupt_data_record.hpp"
#include "core/ddi/dxgkrnl_interface_record.hpp"
#include "core/ddi/linked_device_record.hpp"
#include "harness/framework/test_registry.hpp"

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

/* The device and allocation records (feature 008). WARP's OpenAllocation steps
 * DXGK_OPENALLOCATIONINFO by 0x20 and writes hDeviceSpecificAllocation at 0x18;
 * VidMm's stride for DXGK_SEGMENTDESCRIPTOR4 is 0x68, wider than the 0x60 a
 * WDDM 3.2 driver writes. */
TEST(ddi_records, device_and_allocation_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkargCreateDeviceRecord), 0x20u);
  CHECK_EQ(offsetof(av::DxgkargCreateDeviceRecord, hKmdProcess), 0x18u);
  CHECK_EQ(sizeof(av::DxgkAllocationInfoRecord), 0x58u);
  CHECK_EQ(offsetof(av::DxgkAllocationInfoRecord, hAllocation), 0x38u);
  CHECK_EQ(offsetof(av::DxgkAllocationInfoRecord, Flags2), 0x54u);
  CHECK_EQ(sizeof(av::DxgkargCreateAllocationRecord), 0x28u);
  CHECK_EQ(offsetof(av::DxgkargCreateAllocationRecord, pAllocationInfo), 0x10u);
  CHECK_EQ(sizeof(av::DxgkargDestroyAllocationRecord), 0x20u);
  CHECK_EQ(sizeof(av::DxgkargDescribeAllocationRecord), 0x30u);
  CHECK_EQ(offsetof(av::DxgkargDescribeAllocationRecord, Rotation), 0x2Cu);
  CHECK_EQ(sizeof(av::DxgkargGetStandardAllocationDriverDataRecord), 0x30u);
  CHECK_EQ(sizeof(av::DxgkOpenAllocationInfoRecord), 0x20u);
  CHECK_EQ(offsetof(av::DxgkOpenAllocationInfoRecord, hDeviceSpecificAllocation), 0x18u);
  CHECK_EQ(sizeof(av::DxgkargOpenAllocationRecord), 0x38u);
  CHECK_EQ(offsetof(av::DxgkargOpenAllocationRecord, Pitch), 0x30u);
  CHECK_EQ(sizeof(av::DxgkargCloseAllocationRecord), 0x10u);
  CHECK_EQ(sizeof(av::DxgkargSetDisplayPrivateDriverFormatRecord), 0x18u);
  CHECK_EQ(sizeof(av::DxgkQuerySegmentOut4Record), 0x28u);
  CHECK_EQ(sizeof(av::DxgkSegmentDescriptor4Record), 0x60u);
  CHECK_EQ(offsetof(av::DxgkSegmentDescriptor4Record, NumUEFIFrameBufferRanges), 0x5Cu);
  CHECK_EQ(sizeof(av::D3dkmdtSharedPrimarySurfaceDataRecord), 0x18u);
  CHECK_EQ(sizeof(av::D3dkmdtShadowSurfaceDataRecord), 0x10u);
  CHECK_EQ(sizeof(av::D3dkmdtStagingSurfaceDataRecord), 0x0Cu);
  CHECK_EQ(sizeof(av::D3dkmdtGdiSurfaceDataRecord), 0x18u); }

/* The context and submission records (feature 009). */
TEST(ddi_records, context_and_submission_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkargCreateContextRecord), 0x48u);
  CHECK_EQ(offsetof(av::DxgkargCreateContextRecord, ContextInfoDmaBufferSize), 0x24u);
  CHECK_EQ(sizeof(av::DxgkAllocationListRecord), 0x18u);
  CHECK_EQ(sizeof(av::D3dddiPatchLocationListRecord), 0x18u);
  CHECK_EQ(offsetof(av::D3dddiPatchLocationListRecord, PatchOffset), 0x10u);
  CHECK_EQ(sizeof(av::DxgkargRenderRecord), 0x70u);
  CHECK_EQ(offsetof(av::DxgkargRenderRecord, pPatchLocationListOut), 0x50u);
  CHECK_EQ(sizeof(av::DxgkargPatchRecord), 0x78u);
  CHECK_EQ(offsetof(av::DxgkargPatchRecord, SubmissionFenceId), 0x6Cu);
  CHECK_EQ(sizeof(av::DxgkargSubmitCommandRecord), 0x60u);
  CHECK_EQ(offsetof(av::DxgkargSubmitCommandRecord, SubmissionFenceId), 0x3Cu);
  CHECK_EQ(sizeof(av::DxgkargPreemptCommandRecord), 0x10u);
  CHECK_EQ(sizeof(av::DxgkargQueryCurrentFenceRecord), 0x0Cu);
  CHECK_EQ(sizeof(av::DxgkargBuildPagingBufferRecord), 0x140u);
  CHECK_EQ(offsetof(av::DxgkargBuildPagingBufferRecord, Operation), 0x1Cu);
  CHECK_EQ(offsetof(av::DxgkargcbNotifyInterruptDataRecord, FenceId), 0x08u);
  CHECK_EQ(offsetof(av::DxgkargcbNotifyInterruptDataRecord, Flags), 0x48u); }

/* The VidPn records (feature 011), at the offsets basicdisplay.sys reads. */
TEST(ddi_records, vidpn_records_match_the_offsets_basicdisplay_reads)
{ CHECK_EQ(sizeof(av::D3dkmdtVidPnSourceModeRecord), 0x28u);
  CHECK_EQ(offsetof(av::D3dkmdtVidPnSourceModeRecord, PixelFormat), 0x1Cu);
  CHECK_EQ(sizeof(av::D3dkmdtVideoSignalInfoRecord), 0x38u);
  CHECK_EQ(sizeof(av::D3dkmdtVidPnTargetModeRecord), 0x50u);
  CHECK_EQ(offsetof(av::D3dkmdtVidPnTargetModeRecord, WireFormatAndPreference), 0x40u);
  CHECK_EQ(sizeof(av::D3dkmdtMonitorSourceModeRecord), 0x60u);
  CHECK_EQ(offsetof(av::D3dkmdtMonitorSourceModeRecord, Origin), 0x54u);
  CHECK_EQ(sizeof(av::D3dkmdtVidPnPresentPathRecord), 0x168u);
  CHECK_EQ(offsetof(av::D3dkmdtVidPnPresentPathRecord, GammaRampType), 0x150u);
  CHECK_EQ(offsetof(av::DxgkVidPnInterfaceRecord, pfnAssignTargetModeSet), 0x50u);
  CHECK_EQ(offsetof(av::DxgkVidPnTopologyInterfaceRecord, pfnReleasePathInfo), 0x40u);
  CHECK_EQ(offsetof(av::DxgkVidPnSourceModeSetInterfaceRecord, pfnAddMode), 0x30u);
  CHECK_EQ(offsetof(av::DxgkVidPnTargetModeSetInterfaceRecord, pfnCreateNewModeInfo), 0x28u);
  CHECK_EQ(offsetof(av::DxgkMonitorSourceModeSetInterfaceRecord, pfnReleaseModeInfo), 0x30u);
  CHECK_EQ(sizeof(av::DxgkargIsSupportedVidPnRecord), 0x10u);
  CHECK_EQ(sizeof(av::DxgkargRecommendFunctionalVidPnRecord), 0x30u);
  CHECK_EQ(sizeof(av::DxgkargEnumVidPnCofuncModalityRecord), 0x18u);
  CHECK_EQ(sizeof(av::DxgkargCommitVidPnRecord), 0x20u);
  CHECK_EQ(sizeof(av::DxgkargRecommendMonitorModesRecord), 0x18u);
  CHECK_EQ(sizeof(av::DxgkargRecommendVidPnTopologyRecord), 0x18u);
  CHECK_EQ(sizeof(av::DxgkargGetScanLineRecord), 0x0Cu);
  CHECK_EQ(sizeof(av::DxgkDeviceDescriptorRecord), 0x10u); }

/* The present and scanout records (feature 012), at the x64 offsets basicrender.sys reads. */
TEST(ddi_records, present_and_scanout_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::RectRecord), 0x10u);
  CHECK_EQ(sizeof(av::DxgkargPresentRecord), 0xA8u);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, pPatchLocationListOut), 0x28u);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, Color), 0x38u);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, DstRect), 0x3Cu);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, SrcRect), 0x4Cu);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, SubRectCnt), 0x5Cu);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, pDstSubRects), 0x60u);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, Flags), 0x6Cu);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, PrivateDriverDataSize), 0x98u);
  CHECK_EQ(offsetof(av::DxgkargPresentRecord, pPrivateDriverData), 0xA0u);
  CHECK_EQ(sizeof(av::DxgkPrimaryDataRecord), 0x18u);
  CHECK_EQ(sizeof(av::DxgkargSetVidPnSourceAddressRecord), 0x840u);
  CHECK_EQ(offsetof(av::DxgkargSetVidPnSourceAddressRecord, hAllocation), 0x10u);
  CHECK_EQ(offsetof(av::DxgkargSetVidPnSourceAddressRecord, Flags), 0x228u);
  CHECK_EQ(offsetof(av::DxgkargSetVidPnSourceAddressRecord, PrimaryData), 0x230u);
  CHECK_EQ(sizeof(av::DxgkargSetVidPnSourceVisibilityRecord), 0x08u);
  CHECK_EQ(offsetof(av::DxgkargcbNotifyInterruptDataRecord, Argument2), 0x10u);
  CHECK_EQ(offsetof(av::DxgkargcbNotifyInterruptDataRecord, UnionRemainder), 0x18u); }

/* The pointer records (feature 013). */
TEST(ddi_records, pointer_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkargSetPointerShapeRecord), 0x28u);
  CHECK_EQ(offsetof(av::DxgkargSetPointerShapeRecord, pPixels), 0x18u);
  CHECK_EQ(offsetof(av::DxgkargSetPointerShapeRecord, YHot), 0x24u);
  CHECK_EQ(sizeof(av::DxgkargSetPointerPositionRecord), 0x10u);
  CHECK_EQ(offsetof(av::DxgkargSetPointerPositionRecord, Flags), 0x0Cu); }

/* WDDM 1.1 (feature 014). */
TEST(ddi_records, query_vidpn_hw_capability_record_matches_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkargQueryVidPnHwCapabilityRecord), 0x18u);
  CHECK_EQ(offsetof(av::DxgkargQueryVidPnHwCapabilityRecord, TargetId), 0x0Cu);
  CHECK_EQ(offsetof(av::DxgkargQueryVidPnHwCapabilityRecord, VidPnHWCaps), 0x10u); }

/* WDDM 1.2 (feature 015). */
TEST(ddi_records, win8_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkargQueryDependentEngineGroupRecord), 0x10u);
  CHECK_EQ(offsetof(av::DxgkargQueryDependentEngineGroupRecord, DependentNodeOrdinalMask), 0x08u);
  CHECK_EQ(sizeof(av::DxgkargQueryEngineStatusRecord), 0x0Cu);
  CHECK_EQ(sizeof(av::DxgkargResetEngineRecord), 0x0Cu);
  CHECK_EQ(sizeof(av::DxgkargCancelCommandRecord), 0x70u);
  CHECK_EQ(offsetof(av::DxgkargCancelCommandRecord, DmaBufferSubmissionStartOffset), 0x14u);
  CHECK_EQ(offsetof(av::DxgkargCancelCommandRecord, DmaBufferSubmissionEndOffset), 0x18u);
  CHECK_EQ(offsetof(av::DxgkargCancelCommandRecord, DmaBufferUmdPrivateDataSize), 0x68u);
  CHECK_EQ(sizeof(av::DxgkChildContainerIdRecord), 0x20u);
  CHECK_EQ(offsetof(av::DxgkChildContainerIdRecord, EldInfoManufacturerName), 0x18u);
  CHECK_EQ(sizeof(av::DxgkDisplayInformationRecord), 0x20u);
  CHECK_EQ(offsetof(av::DxgkDisplayInformationRecord, TargetId), 0x18u); }

/* WDDM 1.3 (feature 016). */
TEST(ddi_records, wddm_1_3_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkNodeMetadataRecord), 0x4Au);   /* pack(1): basicrender.sys zeroes 0x4a bytes */
  CHECK_EQ(offsetof(av::DxgkNodeMetadataRecord, Flags), 0x44u);
  CHECK_EQ(offsetof(av::DxgkNodeMetadataRecord, IoMmuSupported), 0x49u);
  CHECK_EQ(sizeof(av::DxgkGpuClockDataRecord), 0x1Cu);
  CHECK_EQ(sizeof(av::DxgkargControlInterrupt2Record), 0x08u); }

/* WDDM 2.0 (feature 017). */
TEST(ddi_records, wddm_2_0_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkGpuMmuCapsRecord), 0x18u);
  CHECK_EQ(sizeof(av::DxgkPageTableLevelDescRecord), 0x14u);
  CHECK_EQ(sizeof(av::DxgkQueryPageTableLevelDescInRecord), 0x04u);
  CHECK_EQ(sizeof(av::DxgkPhysicalAdapterCapsRecord), 0x20u);
  CHECK_EQ(offsetof(av::DxgkPhysicalAdapterCapsRecord, Flags), 0x10u);
  CHECK_EQ(sizeof(av::DxgkPteRecord), 0x10u);
  CHECK_EQ(sizeof(av::DxgkBuildPagingBufferUpdatePageTableRecord), 0x68u);
  CHECK_EQ(offsetof(av::DxgkBuildPagingBufferUpdatePageTableRecord, pPageTableEntries), 0x20u);
  CHECK_EQ(offsetof(av::DxgkBuildPagingBufferUpdatePageTableRecord, Flags), 0x34u);
  CHECK_EQ(sizeof(av::DxgkBuildPagingBufferFlushTlbRecord), 0x28u);
  CHECK_EQ(sizeof(av::DxgkBuildPagingBufferCopyRangeRecord), 0x20u);
  CHECK_EQ(sizeof(av::DxgkBuildPagingBufferCopyPageTableEntriesRecord), 0x10u);
  CHECK_EQ(sizeof(av::DxgkBuildPagingBufferFillVirtualRecord), 0x28u);
  CHECK_EQ(sizeof(av::DxgkBuildPagingBufferTransferVirtualRecord), 0x40u);
  CHECK_EQ(sizeof(av::DxgkargSubmitCommandVirtualRecord), 0x40u);
  CHECK_EQ(offsetof(av::DxgkargSubmitCommandVirtualRecord, SubmissionFenceId), 0x28u);
  CHECK_EQ(sizeof(av::DxgkargSetRootPageTableRecord), 0x20u);
  CHECK_EQ(sizeof(av::DxgkargGetRootPageTableSizeRecord), 0x08u);
  CHECK_EQ(sizeof(av::DxgkargCreateProcessRecord), 0x38u);
  CHECK_EQ(sizeof(av::DxgkargSetStablePowerStateRecord), 0x04u); }

TEST(ddi_records, wddm_2_1_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkargControlModeBehaviorRecord), 0x0Cu);
  CHECK_EQ(offsetof(av::DxgkargControlModeBehaviorRecord, NotSatisfied), 0x08u);
  CHECK_EQ(sizeof(av::DxgkargUpdateMonitorLinkInfoRecord), 0x10u);
  CHECK_EQ(offsetof(av::DxgkargUpdateMonitorLinkInfoRecord, MonitorLinkInfoDitheringSupport), 0x0Cu); }

TEST(ddi_records, wddm_2_2_records_match_the_x64_layout)
{ CHECK_EQ(sizeof(av::DxgkargCreateHwQueueRecord), 0x30u);
  CHECK_EQ(offsetof(av::DxgkargCreateHwQueueRecord, HwQueueProgressFenceCPUVirtualAddress), 0x20u);
  CHECK_EQ(sizeof(av::DxgkargSubmitCommandToHwQueueRecord), 0x40u);
  CHECK_EQ(offsetof(av::DxgkargSubmitCommandToHwQueueRecord, HwQueueProgressFenceCpuVa), 0x38u);
  CHECK_EQ(sizeof(av::DxgkargResetHwEngineRecord), 0x08u);
  CHECK_EQ(sizeof(av::DxgkargResumeHwEngineRecord), 0x08u);
  CHECK_EQ(sizeof(av::DxgkPresentAllocationInfoRecord), 0x20u);
  CHECK_EQ(offsetof(av::DxgkPresentAllocationInfoRecord, SegmentId), 0x18u); }

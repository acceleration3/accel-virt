#ifndef AV_DXGK_DRIVER_CAPS_RECORD_HPP
#define AV_DXGK_DRIVER_CAPS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkDriverCapsRecord {
    int64_t  HighestAcceptableAddress;
    uint32_t MaxAllocationListSlotId;
    uint32_t MaxAllocationListSlotIdPadding;
    uint64_t ApertureSegmentCommitLimit;
    uint32_t MaxPointerWidth;
    uint32_t MaxPointerHeight;
    uint32_t PointerCaps;
    uint32_t InterruptMessageNumber;
    uint32_t NumberOfSwizzlingRanges;
    uint32_t MaxOverlays;
    uint32_t GammaRampCaps;
    uint32_t PresentationCaps;
    uint32_t MaxQueuedFlipOnVSync;
    uint32_t FlipCaps;
    uint32_t SchedulingCaps;
    uint32_t MemoryManagementCaps;
    uint32_t MemoryManagementCapsPagingNode;
    uint32_t GpuEngineTopologyNbAsymetricProcessingNodes;
    uint32_t GpuEngineTopologyReserved[64];
    int32_t  WDDMVersion;
    uint32_t Reserved[7];
    uint32_t Reserved1[40];
    int32_t  PreemptionCapsGraphicsPreemptionGranularity;
    int32_t  PreemptionCapsComputePreemptionGranularity;
    uint8_t  SupportNonVGA;
    uint8_t  SupportSmoothRotation;
    uint8_t  SupportPerEngineTDR;
    uint8_t  SupportDirectFlip;
    uint8_t  SupportMultiPlaneOverlay;
    uint8_t  SupportRuntimePowerManagement;
    uint8_t  SupportSurpriseRemovalInHibernation;
    uint8_t  HybridDiscrete;
    uint32_t MaxOverlayPlanes;
    uint8_t  HybridIntegrated;
    uint8_t  HybridIntegratedPadding[3];
    uint64_t InternalGpuVirtualAddressRangeStart;
    uint64_t InternalGpuVirtualAddressRangeEnd;
    uint8_t  SupportSurpriseRemoval;
    uint8_t  SupportMultiPlaneOverlayImmediateFlip;
    uint8_t  CursorScaledWithMultiPlaneOverlayPlane0;
    uint8_t  HybridAcpiChainingRequired;
    uint32_t MaxQueuedMultiPlaneOverlayFlipVSync;
    uint32_t MiscCaps;
    uint32_t MaxHwQueuedFlips;
    uint32_t HwQueuedFlipCaps;
    uint32_t Padding;
};

}

#endif

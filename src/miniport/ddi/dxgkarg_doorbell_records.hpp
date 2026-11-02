#ifndef AV_DXGKARG_DOORBELL_RECORDS_HPP
#define AV_DXGKARG_DOORBELL_RECORDS_HPP

#include <stdint.h>

namespace av {
enum class DoorbellStatus : uint32_t {
    connected = 0u,
    connected_notify_kmd = 1u,
    disconnected_retry = 2u,
    disconnected_abort = 3u,
};

struct DxgkargCreateDoorbellRecord {
    void    *hHwQueue;
    void    *hDoorbell;
    uint32_t PrivateDriverDataSize;
    uint32_t PrivateDriverDataSizePadding;
    void    *PrivateDriverData;
    void    *hRingBuffer;
    void    *hRingBufferControl;
    uint32_t Flags;
    uint32_t FlagsPadding;
};

struct DxgkargConnectDoorbellRecord {
    void    *hDoorbell;
    uint32_t Flags;
    uint32_t FlagsPadding;
    void    *KernelCpuVirtualAddress;
    void    *SecondaryKernelCpuVirtualAddress;
    uint32_t Status;
    uint32_t StatusPadding;
};

struct DxgkargDisconnectDoorbellRecord {
    void    *hDoorbell;
    uint32_t Flags;
    uint32_t FlagsPadding;
};

struct DxgkargDestroyDoorbellRecord {
    void *hDoorbell;
};

struct DxgkargNotifyWorkSubmissionRecord {
    void    *hHwQueue;
    uint32_t Flags;
    uint32_t FlagsPadding;
};

struct DxgkUserModeSubmissionCapsRecord {
    uint32_t Flags;
    uint32_t DoorbellSizeInBytes;
    uint32_t SecondaryDoorbellSizeInBytes;
    uint8_t  Reserved[16];
};

}

#endif

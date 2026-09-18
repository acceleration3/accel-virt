/* The doorbell DDIs' arguments, as DxgkDdiCreateDoorbell, ConnectDoorbell,
 * DisconnectDoorbell, DestroyDoorbell and NotifyWorkSubmission receive them
 * (d3dkmddi.h:10270-10390). DDI records; src/km/ddi_record_layout.cpp proves
 * each offset.
 *
 * Five in one header because they are one mechanism's arguments and every one
 * of them is two or three fields: a header apiece would be five headers whose
 * only contents are a handle.
 */
#ifndef AV_DXGKARG_DOORBELL_RECORDS_HPP
#define AV_DXGKARG_DOORBELL_RECORDS_HPP

#include <stdint.h>

namespace av {

/* The doorbell a process may store to, once it is connected
 * (d3dukmdt.h:2063). What a process reads out of its status page, and what
 * tells it whether to store at all. */
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

/* DXGK_USERMODESUBMISSION_CAPS (d3dkmddi.h:10240), what
 * DXGKQAITYPE_USERMODESUBMISSION_CAPS answers: this device's doorbell is a
 * four-byte store at offset 0 of the page, and it has no secondary. */
struct DxgkUserModeSubmissionCapsRecord {
    uint32_t Flags;
    uint32_t DoorbellSizeInBytes;
    uint32_t SecondaryDoorbellSizeInBytes;
    uint8_t  Reserved[16];
};

}  // namespace av

#endif  // AV_DXGKARG_DOORBELL_RECORDS_HPP

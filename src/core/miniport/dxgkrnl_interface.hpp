/* The callbacks dxgkrnl hands one adapter at DxgkDdiStartDevice, copied once so
 * a later change to dxgkrnl's table cannot reach core. Each method is one
 * callback with its fixed arguments filled in. */
#ifndef AV_DXGKRNL_INTERFACE_HPP
#define AV_DXGKRNL_INTERFACE_HPP

#include "core/ddi/dxgk_device_info_record.hpp"
#include "core/ddi/dxgkrnl_interface_record.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkrnlInterfaceProbe;

class DxgkrnlInterface {
public:
    explicit DxgkrnlInterface(Platform &platform);
    DxgkrnlInterface(const DxgkrnlInterface &) = delete;
    DxgkrnlInterface &operator=(const DxgkrnlInterface &) = delete;

    /* not_supported for a table older than WDDM 3.2's; invalid_parameter when a
     * callback core calls is missing. A failure leaves it uninitialized. */
    [[nodiscard]] Status initialize(const DxgkrnlInterfaceRecord *table);

    [[nodiscard]] Status get_device_information(DxgkDeviceInfoRecord &info);
    /* malformed when dxgkrnl reads fewer bytes than asked. */
    [[nodiscard]] Status read_config_space(uint8_t *buffer, uint32_t length);
    /* Non-cached, kernel-only memory space. */
    [[nodiscard]] Status map_memory(uint64_t translated_address, uint32_t length, void *&mapped);
    [[nodiscard]] Status unmap_memory(void *mapped);
    bool                 queue_dpc();
    /* DxgkCbAcquireHandleData for an allocation, legal only inside the DDI
     * dxgkrnl is calling, and paired with release_handle_data: WDDM 2.0
     * replaced DxgkCbGetHandleData, which answers null for an allocation being
     * opened. AcquiredHandleData owns the pair; prefer it to calling these. */
    [[nodiscard]] Status acquire_handle_data(uint32_t allocation_handle, void *&data, void *&release_handle);
    void                 release_handle_data(void *release_handle);
    /* Runs `routine` at the adapter's device IRQL, synchronized with its ISR. */
    [[nodiscard]] Status synchronize_execution(SynchronizeRoutine routine, void *context);
    /* Only inside the ISR or a synchronized routine. */
    void                 notify_interrupt(const DxgkargcbNotifyInterruptDataRecord &data);
    /* From the DPC, after the interrupts it follows. */
    void                 notify_dpc();
    /* DxgkCbQueryVidPnInterface, version 1. */
    [[nodiscard]] Status query_vidpn_interface(void *vidpn, const DxgkVidPnInterfaceRecord *&vidpn_interface);
    /* Whether this OS allows a feature the driver implements to be turned on.
     * A driver that reports a scheduling capability the OS has not enabled
     * fails the whole adapter start, so this is asked first (feature 049,
     * research.md §15). false for a table too old to have the callback, and
     * for any refusal: the answer is then "not allowed", which is the safe
     * one. */
    bool                 feature_enabled(uint32_t feature_id, uint32_t support_state);

    bool initialized() const;
    /* DXGKRNL_INTERFACE.DeviceHandle: null before initialize. */
    void *device_handle() const;
    bool invariants_hold() const;

private:
    static constexpr uint32_t config_space = 0u;  /* DXGK_WHICHSPACE_CONFIG */
    static constexpr int32_t  non_cached   = 0;   /* MmNonCached */
    static constexpr int32_t  handle_allocation = 1;   /* DXGK_HANDLE_ALLOCATION */
    static constexpr int32_t  vidpn_interface_v1 = 1;  /* DXGK_VIDPN_INTERFACE_VERSION_V1 */

    bool has_callbacks(const DxgkrnlInterfaceRecord &table) const;

    Platform              &platform_;
    DxgkrnlInterfaceRecord record_;
    bool                   initialized_;

    friend struct DxgkrnlInterfaceProbe;
};

}  // namespace av

#endif  // AV_DXGKRNL_INTERFACE_HPP

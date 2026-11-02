#ifndef AV_DXGKRNL_INTERFACE_HPP
#define AV_DXGKRNL_INTERFACE_HPP

#include "miniport/ddi/dxgk_device_info_record.hpp"
#include "miniport/ddi/dxgkrnl_interface_record.hpp"
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

    [[nodiscard]] Status initialize(const DxgkrnlInterfaceRecord *table);

    [[nodiscard]] Status get_device_information(DxgkDeviceInfoRecord &info);
    [[nodiscard]] Status read_config_space(uint8_t *buffer, uint32_t length);
    [[nodiscard]] Status map_memory(uint64_t translated_address, uint32_t length, void *&mapped);
    [[nodiscard]] Status unmap_memory(void *mapped);
    bool                 queue_dpc();
    [[nodiscard]] Status acquire_handle_data(uint32_t allocation_handle, void *&data, void *&release_handle);
    void                 release_handle_data(void *release_handle);
    [[nodiscard]] Status synchronize_execution(SynchronizeRoutine routine, void *context);
    void                 notify_interrupt(const DxgkargcbNotifyInterruptDataRecord &data);
    void                 notify_dpc();
    void                 replug(uint32_t child);
    [[nodiscard]] Status query_vidpn_interface(void *vidpn, const DxgkVidPnInterfaceRecord *&vidpn_interface);
    bool                 feature_enabled(uint32_t feature_id, uint32_t support_state);

    bool initialized() const;
    void *device_handle() const;
    bool invariants_hold() const;

private:
    static constexpr uint32_t config_space = 0u;
    static constexpr int32_t  non_cached   = 0;
    static constexpr int32_t  handle_allocation = 1;
    static constexpr int32_t  vidpn_interface_v1 = 1;

    bool has_callbacks(const DxgkrnlInterfaceRecord &table) const;

    Platform              &platform_;
    DxgkrnlInterfaceRecord record_;
    bool                   initialized_;

    friend struct DxgkrnlInterfaceProbe;
};

}

#endif

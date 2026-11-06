#ifndef AV_HOST_CHANNEL_PROBE_HPP
#define AV_HOST_CHANNEL_PROBE_HPP

#include "icd/common/host_channel.hpp"

#include <stdint.h>

namespace av {
class GuestConnection;
class KmtTransport;
}

namespace av::icd {

struct HostChannelProbe {
    explicit HostChannelProbe(HostChannel &target);

    uint8_t *swap_staging(uint8_t *staging);
    uint8_t *swap_batch_block(uint8_t *block);
    void     drop_locked();
    void     restore_locked();
    void     restore_holding();
    void     build_transport(GuestConnection &device);
    void     drop_transport();
    void     drop_handle(uint64_t handle);

    HostChannel &channel;
};

}

#endif

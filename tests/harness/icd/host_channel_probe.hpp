/* Reaches into a HostChannel so the invariant tests of it and of the contexts
 * that hold one can build the states it never builds. Declared a friend by
 * HostChannel; nothing outside the tests uses it. */
#ifndef AV_HOST_CHANNEL_PROBE_HPP
#define AV_HOST_CHANNEL_PROBE_HPP

#include "icd/common/host_channel.hpp"

#include <stdint.h>

namespace av {
class GuestConnection;
class KmtTransport;
}  // namespace av

namespace av::icd {

struct HostChannelProbe {
    explicit HostChannelProbe(HostChannel &target);

    /* Each answers what it replaced, so a test can put it back: the channel's
     * close is what frees it. */
    uint8_t *swap_staging(uint8_t *staging);
    uint8_t *swap_batch_block(uint8_t *block);
    /* The batch taken away and put back over the channel's own block. */
    void     drop_batched();
    void     restore_batched();
    /* A transport over `device` that no open made, never initialized; and
     * the channel's transport taken away, with nothing told. */
    void     build_transport(GuestConnection &device);
    void     drop_transport();

    HostChannel &channel;
};

}  // namespace av::icd

#endif  // AV_HOST_CHANNEL_PROBE_HPP

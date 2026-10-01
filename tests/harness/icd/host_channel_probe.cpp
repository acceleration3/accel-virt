#include "harness/icd/host_channel_probe.hpp"

#include "guest/transport/guest_connection.hpp"

namespace av::icd {

HostChannelProbe::HostChannelProbe(HostChannel &target) : channel(target) {}

uint8_t *HostChannelProbe::swap_staging(uint8_t *staging)
{
    uint8_t *const kept = channel.staging_;
    channel.staging_ = staging;
    return kept;
}

uint8_t *HostChannelProbe::swap_batch_block(uint8_t *block)
{
    uint8_t *const kept = channel.batch_block_;
    channel.batch_block_ = block;
    return kept;
}

void HostChannelProbe::drop_batched() { channel.batched_.destroy(); }

void HostChannelProbe::restore_batched()
{
    static_cast<void>(channel.batched_.construct(channel.transport_.object(), channel.batch_block_,
                                                 channel.batch_bytes_, channel.staging_bytes_));
}

void HostChannelProbe::build_transport(GuestConnection &device)
{
    static_cast<void>(channel.transport_.construct(device, channel.sub_api_, channel.ring_order_,
                                                   channel.reply_bytes_, nullptr, 0u));
}

void HostChannelProbe::drop_transport() { channel.transport_.destroy(); }

}  // namespace av::icd

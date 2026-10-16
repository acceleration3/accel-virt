/* How many virtio-gpu-av devices one VM has at most, and so how many avgpu
 * adapters its guest sees: one per host GPU the VM is given, which the VM's
 * configuration decides, not anything running in it. The QEMU device module
 * keeps that many, and every layer of the guest that holds an adapter each
 * sizes its table by the same number, so no layer refuses an adapter another
 * accepted. */
#ifndef AV_AV_DEVICE_COUNT_HPP
#define AV_AV_DEVICE_COUNT_HPP

#include <stdint.h>

namespace av {

inline constexpr uint32_t most_av_devices = 16u;

}  // namespace av

#endif  // AV_AV_DEVICE_COUNT_HPP

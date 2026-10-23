/* Orders the GPU's Direct3D 11 queue after one producer's presents: a timeline
 * the producer signals on its own queue - a Direct3D 9 device's copy through
 * its Vulkan lane, a Direct3D 12 queue's shared fence - imported onto DXVK's
 * device, which waits for each present's value before anything it runs
 * after: DWM's composition, the scanout's export.
 *
 * The caller holds the Direct3D 11 device's lock: inserting a wait flushes
 * DXVK's immediate context as well as taking its queue submission lock. */
#ifndef AV_COMPOSE_ORDER_HPP
#define AV_COMPOSE_ORDER_HPP

#include "executor/shim/vulkan_timeline.hpp"

#include <stdint.h>

namespace av {

class OwnedFd;
class D3d11HostDevice;

class ComposeOrder {
public:
    ComposeOrder();
    ComposeOrder(const ComposeOrder &) = delete;
    ComposeOrder &operator=(const ComposeOrder &) = delete;

    /* The timeline over the OPAQUE_FD `descriptor` the producer signals,
     * imported onto `device11`'s DXVK device; the descriptor is the
     * timeline's once it is made. False where DXVK will not take it. */
    [[nodiscard]] bool connect(D3d11HostDevice &device11, OwnedFd &descriptor);
    bool               connected() const;
    /* The value the producer's next present signals. */
    uint64_t           next() const;
    /* The value the producer's latest present signalled; 0 before one. */
    uint64_t           value() const;
    /* The producer signalled next(): DXVK's queue waits for it. Called with
     * the Direct3D 11 device's lock held. */
    [[nodiscard]] bool presented();
    /* What DXVK queued waiting on the timeline waited for, and the timeline
     * let go; values start again. */
    void               release();

private:
    D3d11HostDevice *device11_;
    VulkanTimeline   waited_;
    uint64_t         value_;
};

}  // namespace av

#endif  // AV_COMPOSE_ORDER_HPP

/* What a Direct3D 12 session shows through its GPU's Direct3D 11 device, where
 * the compositor is: its swap chain buffers, published under the keys the
 * guest shares them by and made on DXVK's device over the same memory the
 * first time something opens them (D3d11HostDevice::open_shared), and the
 * order a present puts DXVK's queue in - after everything the presenting
 * queue executed.
 *
 * The order is a shared fence: the present signals it on the Direct3D 12
 * queue, in that queue's order, and DXVK's queue waits on the GPU for the
 * value, through the fence's timeline semaphore imported onto DXVK's device.
 * Nothing waits on the CPU, and nothing drains the Direct3D 12 queue - a queue
 * waiting on a fence the session's next command signals would never drain. */
#ifndef AV_D3D12_PRESENTATION_HPP
#define AV_D3D12_PRESENTATION_HPP

#include <stdint.h>
#include "executor/shim/vulkan_timeline.hpp"

#include <vulkan/vulkan.h>

struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12Resource;

namespace av {

class D3d11HostDevice;
class D3d12HostDevice;
class SessionShares;

class D3d12Presentation {
public:
    D3d12Presentation();
    ~D3d12Presentation();
    D3d12Presentation(const D3d12Presentation &) = delete;
    D3d12Presentation &operator=(const D3d12Presentation &) = delete;

    /* `device12`'s surfaces shown through `device11`, published through the
     * session's `shares`; with any null, none is. All must outlive this. */
    void               bind(D3d12HostDevice *device12, D3d11HostDevice *device11, SessionShares *shares);
    /* `resource` - a committed 2D texture made shared - marked as the surface
     * `key` (SurfaceIdentity) and published under it. False for anything
     * else, and for a key already shared. */
    [[nodiscard]] bool share(ID3D12Resource *resource, uint64_t key);
    /* What `queue` executed so far is done before what DXVK runs next. */
    [[nodiscard]] bool present(ID3D12CommandQueue *queue);
    /* The fence and DXVK's side of it go, once the session's queues have
     * finished: what DXVK queued waiting on it is waited for first. */
    void               release();

private:
    /* The shared fence and its timeline imported onto DXVK's device, made at
     * the first present. */
    [[nodiscard]] bool connect();

    D3d12HostDevice *device12_;
    D3d11HostDevice *device11_;
    SessionShares   *shares_;
    ID3D12Fence     *fence_;
    VulkanTimeline   waited_;
    uint64_t         value_;
};

}  // namespace av

#endif  // AV_D3D12_PRESENTATION_HPP

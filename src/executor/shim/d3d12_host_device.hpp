/* One host GPU's Direct3D 12 device, which every Direct3D 12 session on that
 * GPU runs on.
 *
 * A vkd3d-proton device of its own per GPU (D3dRuntime::create_device12), made
 * once when the host is asked, as the Direct3D 11 one is, because making it
 * costs a pipeline cache load that must not land inside a guest's first
 * batch. Unlike Direct3D 11's there is no immediate context to share: every
 * session records its own command lists and submits to its own queues, which
 * vkd3d-proton's device serializes itself, so there is no lock here.
 *
 * Shared, it is lost for every session that opened it at once, as Direct3D
 * 11's is (D3d11HostDevice): those sessions are its tenants (DeviceTenants),
 * woken by whoever sees the loss first, and the device is made again on the
 * same GPU once the last of them is gone and a session opens it again. What
 * still holds the old device - a resource a share kept, a frame the present
 * engine reads - keeps it alive, since vkd3d-proton's objects hold their
 * device, and lets go of it in its own time. */
#ifndef AV_D3D12_HOST_DEVICE_HPP
#define AV_D3D12_HOST_DEVICE_HPP

#include "core/log/driver_log.hpp"
#include "device/host/stdio_log.hpp"
#include "executor/host_device_renewal.hpp"
#include "executor/renewable_device.hpp"
#include "executor/shim/d3d12_capabilities.hpp"
#include "executor/shim/d3d12_frame_pool.hpp"
#include "executor/shim/host_heap.hpp"
#include "proto/context/device_uuid.hpp"

#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12Heap;
struct ID3D12Resource;
struct ID3D12RootSignature;

namespace av {

class OwnedFd;
class D3d11HostDevice;
class D3dRuntime;
class WakeSignal;
class StagedFrame;
class Vkd3dImageExport;

class D3d12HostDevice final : public RenewableDevice {
public:
    D3d12HostDevice();
    ~D3d12HostDevice();
    D3d12HostDevice(const D3d12HostDevice &) = delete;
    D3d12HostDevice &operator=(const D3d12HostDevice &) = delete;

    /* The device on the GPU `uuid` names, and what it can do. False for a GPU
     * vkd3d-proton does not run on. `runtime` must outlive the device. */
    [[nodiscard]] bool open(const D3dRuntime &runtime, const uint8_t *uuid);
    void               close();
    bool               opened() const;
    /* Whether open found the GPU: a device lost, or one renew could not
     * make again, still stands for it. */
    bool               placed() const;

    /* A session that opened Direct3D 12 joins the device, woken through
     * `wake` should it be lost: false for one lost and not made again. */
    [[nodiscard]] bool admit(WakeSignal &wake);
    void               dismiss(WakeSignal &wake);
    /* Whether the device is lost, as D3d11HostDevice::lost says it of its own:
     * vkd3d-proton's GetDeviceRemovedReason. A tenant's thread. */
    bool               lost();
    /* The GPU's Direct3D 11 device, which shows this one's swap chains and
     * holds the shares; set by its pair(). */
    void               pair(D3d11HostDevice *shown_by);
    /* A lost device made again (HostDeviceRenewal): the last tenant waited
     * for up to `wait_ms`, what the shares held of the old device let go of
     * through the Direct3D 11 device's lock, which drains them, and
     * vkd3d-proton's device made anew. True for a device not lost, or made
     * again. */
    [[nodiscard]] bool renew(uint32_t wait_ms);
    /* For tests: vkd3d-proton's Vulkan device made lost, as a GPU fault
     * would lose it, and the loss declared. False where the host cannot. */
    [[nodiscard]] bool lose();

    ID3D12Device            *device() const;
    const D3d12Capabilities &capabilities() const;
    /* Host memory of the GPU's memory source, the dma-buf `fd` (the caller
     * keeps it) mapped here at `address`, as a heap buffers can be placed in,
     * with a reference the caller owns; null where the runtime cannot import
     * (a stock vkd3d-proton) or refuses the memory. */
    ID3D12Heap                *import_dmabuf_heap(void *address, int fd, uint64_t size) const;
    /* A root signature from a D3D12_VERSIONED_ROOT_SIGNATURE_DESC, with a
     * reference the caller owns; null when the runtime refuses it. */
    ID3D12RootSignature       *root_signature(const void *desc) const;
    /* A shared committed texture's memory as an fd, and the image it was made
     * as (tools/vkd3d-proton-patches); false where vkd3d-proton cannot say. */
    [[nodiscard]] bool         export_image(ID3D12Resource *resource, Vkd3dImageExport &image) const;
    /* A shared fence's timeline semaphore as an fd the caller owns. */
    [[nodiscard]] bool         export_fence(ID3D12Fence *fence, OwnedFd &descriptor) const;
    /* `source` staged for the present engine on `queue`, in that queue's
     * order (D3d12FramePool::stage); null when it is not. */
    StagedFrame               *stage_frame(ID3D12CommandQueue *queue, ID3D12Resource *source);

private:
    /* vkd3d-proton's device made on the GPU open found. */
    [[nodiscard]] bool make();
    /* RenewableDevice: the device, what vkd3d-proton says of it, and the
     * device made anew. Only a tenant reads device_ outside renew, and there
     * is none while it is remade. */
    bool               present() const override;
    bool               removed() override;
    [[nodiscard]] bool remake() override;

    const D3dRuntime  *runtime_;
    DeviceUuid         uuid_;
    /* Where the tenants' list grows from: before it, as it is destroyed after. */
    HostHeap           tenant_heap_;
    /* QEMU's stderr, where a loss and a renewal are seen. */
    StdioLog           sink_;
    DriverLog          lines_;
    HostDeviceRenewal  renewal_;
    D3d11HostDevice   *shown_by_;
    ID3D12Device      *device_;
    D3d12Capabilities  capabilities_;
    D3d12FramePool     frames_;
};

}  // namespace av

#endif  // AV_D3D12_HOST_DEVICE_HPP

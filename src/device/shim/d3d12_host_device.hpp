/* One host GPU's Direct3D 12 device, which every Direct3D 12 session on that
 * GPU runs on.
 *
 * A vkd3d-proton device of its own per GPU (D3dRuntime::create_device12), made
 * once when the host is asked, as the Direct3D 11 one is, because making it
 * costs a pipeline cache load that must not land inside a guest's first
 * batch. Unlike Direct3D 11's there is no immediate context to share: every
 * session records its own command lists and submits to its own queues, which
 * vkd3d-proton's device serializes itself, so there is no lock here. */
#ifndef AV_D3D12_HOST_DEVICE_HPP
#define AV_D3D12_HOST_DEVICE_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12Heap;
struct ID3D12Resource;
struct ID3D12RootSignature;

namespace av {

class D3dRuntime;
class Vkd3dImageExport;

class D3d12HostDevice {
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

    ID3D12Device              *device() const;
    const d3d12proto::Caps12 &caps() const;
    /* Host memory as a heap buffers can be placed in, with a reference the
     * caller owns; null where the runtime cannot import (a stock
     * vkd3d-proton) or refuses these pages. */
    ID3D12Heap                *import_heap(void *address, uint64_t size) const;
    /* The same, over the dma-buf `fd` of those pages (the caller keeps it). */
    ID3D12Heap                *import_dmabuf_heap(void *address, int fd, uint64_t size) const;
    /* A root signature from a D3D12_VERSIONED_ROOT_SIGNATURE_DESC, with a
     * reference the caller owns; null when the runtime refuses it. */
    ID3D12RootSignature       *root_signature(const void *desc) const;
    /* A shared committed texture's memory as an fd, and the image it was made
     * as (tools/vkd3d-proton-patches); false where vkd3d-proton cannot say. */
    [[nodiscard]] bool         export_image(ID3D12Resource *resource, Vkd3dImageExport &image) const;
    /* A shared fence's timeline semaphore as an fd the caller owns. */
    [[nodiscard]] bool         export_fence(ID3D12Fence *fence, int &fd) const;

private:
    void measure(uint32_t vendor_id, uint32_t device_id, uint64_t video_memory);

    const D3dRuntime  *runtime_;
    ID3D12Device      *device_;
    d3d12proto::Caps12 caps_;
};

}  // namespace av

#endif  // AV_D3D12_HOST_DEVICE_HPP

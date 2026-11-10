#ifndef AV_D3D12_HOST_DEVICE_HPP
#define AV_D3D12_HOST_DEVICE_HPP

#include "executor/host_device_renewal.hpp"
#include "executor/renewable_device.hpp"
#include "executor/shim/d3d12_capabilities.hpp"
#include "executor/shim/d3d12_dispatch_limit.hpp"
#include "executor/shim/d3d12_frame_pool.hpp"
#include "executor/shim/host_heap.hpp"
#include "proto/context/device_uuid.hpp"

#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
struct ID3D12Heap;
struct D3D12_HEAP_DESC;
struct ID3D12Resource;
struct ID3D12RootSignature;

namespace av {

class DriverLog;
class OwnedFd;
class D3d11HostDevice;
class D3dRuntime;
class WakeSignal;
class StagedFrame;
class Vkd3dImageExport;
struct D3d12HostDeviceProbe;

class D3d12HostDevice final : public RenewableDevice {
public:
    explicit D3d12HostDevice(DriverLog &lines);
    ~D3d12HostDevice();
    D3d12HostDevice(const D3d12HostDevice &) = delete;
    D3d12HostDevice &operator=(const D3d12HostDevice &) = delete;

    [[nodiscard]] bool open(const D3dRuntime &runtime, const uint8_t *uuid);
    void               close();
    bool               opened() const;
    bool               placed() const;

    [[nodiscard]] bool admit(WakeSignal &wake);
    void               dismiss(WakeSignal &wake);
    bool               lost();
    void               pair(D3d11HostDevice *shown_by);
    [[nodiscard]] bool renew(uint32_t wait_ms);
    [[nodiscard]] bool lose();

    ID3D12Device            *device() const;
    const D3d12Capabilities &capabilities() const;
    const D3d12DispatchLimit &dispatch_limit() const;
    ID3D12Heap                *import_dmabuf_heap(void *address, int fd, uint64_t size) const;
    ID3D12Heap                *create_exported_heap(const D3D12_HEAP_DESC &desc, OwnedFd &descriptor) const;
    bool                       exports_heaps() const;
    ID3D12RootSignature       *root_signature(const void *desc) const;
    [[nodiscard]] bool         export_image(ID3D12Resource *resource, Vkd3dImageExport &image) const;
    bool                       translates_indirect_addresses() const;
    void                       set_indirect_va_table(ID3D12GraphicsCommandList *list, uint64_t table) const;
    [[nodiscard]] bool         export_fence(ID3D12Fence *fence, OwnedFd &descriptor) const;
    StagedFrame               *stage_frame(ID3D12CommandQueue *queue, ID3D12Resource *source);

private:
    [[nodiscard]] bool make();
    bool               present() const override;
    bool               removed() override;
    [[nodiscard]] bool remake() override;

    DriverLog         &lines_;
    const D3dRuntime  *runtime_;
    DeviceUuid         uuid_;
    HostHeap           tenant_heap_;
    HostDeviceRenewal  renewal_;
    D3d11HostDevice   *shown_by_;
    ID3D12Device      *device_;
    D3d12Capabilities  capabilities_;
    D3d12DispatchLimit dispatch_limit_;
    D3d12FramePool     frames_;

    friend struct D3d12HostDeviceProbe;
};

}

#endif

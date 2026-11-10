#ifndef AV_D3D11_HOST_DEVICE_HPP
#define AV_D3D11_HOST_DEVICE_HPP

#include "device/shim/posix_lock.hpp"
#include "core/support/object_pool.hpp"
#include "executor/host_device_renewal.hpp"
#include "executor/renewable_device.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/shim/carried_share.hpp"
#include "executor/shim/context_gate.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/shim/d3d11_capabilities.hpp"
#include "proto/context/device_uuid.hpp"
#include "executor/shim/d3d11_frame_pool.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"
#include "executor/shim/exportable_images.hpp"
#include "executor/shim/host_image_export.hpp"
#include "executor/shim/host_format_converter.hpp"
#include "executor/shim/host_surface_copy.hpp"
#include "executor/shim/plane_composer.hpp"
#include "executor/shim/slice_importer.hpp"

#include <stdint.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3DDeviceContextState;
struct D3D11_TEXTURE2D_DESC;
struct D3D11_TEXTURE2D_DESC1;
struct ID3D11Texture2D;
struct ID3D11Fence;
struct ID3D12Resource;
struct IUnknown;
struct ID3D11Buffer;
struct ID3D11VkExtContext2;

namespace av {

class DriverLog;
class OwnedFd;
class VulkanBuffer;

class D3dRuntime;
class HostShares;
class StagedFrame;
class StateWriter;
class CompositorDevice;
class D3d12HostDevice;
class Vkd3dImageExport;
class WakeSignal;

class D3d11HostDevice final : public RenewableDevice, public SliceImporter, private ContextGate {
public:
    explicit D3d11HostDevice(DriverLog &lines);
    ~D3d11HostDevice();
    D3d11HostDevice(const D3d11HostDevice &) = delete;
    D3d11HostDevice &operator=(const D3d11HostDevice &) = delete;

    [[nodiscard]] bool open(const D3dRuntime &runtime, const uint8_t *uuid, HostShares &shares);
    void               close();
    bool               opened() const;
    bool               placed() const;

    [[nodiscard]] bool admit(WakeSignal &wake);
    void               dismiss(WakeSignal &wake);
    bool               lost();
    bool               refused_memory() const;
    [[nodiscard]] bool renew(uint32_t wait_ms);
    [[nodiscard]] bool lose();

    void lock(ID3DDeviceContextState *state);
    void select(ID3DDeviceContextState *state);
    void unlock();
    ID3DDeviceContextState *make_state(uint32_t &feature_level);
    void                    forget_state(ID3DDeviceContextState *state);
    [[nodiscard]] bool      show(uint32_t domain, uint64_t id, ID3D11Texture2D *presented);
    [[nodiscard]] bool      prepare_shown(uint32_t domain, uint64_t id, ID3D11Texture2D *presented);
    IUnknown               *open_shared(uint32_t domain, uint64_t id);
    ID3D11Fence            *make_fence(uint64_t initial);
    [[nodiscard]] bool      signal(ID3D11Fence *fence, uint64_t value);

    [[nodiscard]] bool read_image(uint32_t domain, uint64_t id, void *pixels, uint32_t stride, uint32_t width,
                                  uint32_t height);
    [[nodiscard]] bool export_image(uint32_t domain, uint64_t id, AvExportedImage &image);
    [[nodiscard]] bool compose_planes(uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                      uint32_t height, AvExportedImage &image);
    [[nodiscard]] bool export_surface(uint32_t domain, uint64_t surface, uint32_t flags, AvExportedImage &image);
    [[nodiscard]] bool map_surface(uint32_t domain, uint64_t surface, uint32_t wait_ms, AvFrameView &view);
    void               unmap_surface(uint64_t surface);
    [[nodiscard]] bool copy_surface(uint32_t domain, uint64_t source, uint64_t destination, const AvCopyRegion *regions,
                                    uint32_t count, uint32_t wait_ms);
    [[nodiscard]] bool sync_surface(uint32_t domain, uint64_t surface, uint32_t flags, uint64_t &writes, int &fd);
    StagedFrame       *stage_frame(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                                   uint32_t format);
    void               retire_shares();
    void               forget_direct3d12();
    void               pair(D3d12HostDevice *device12);
    void               use_compositor(CompositorDevice *compositor);
    CompositorDevice  *compositor() const;
    ID3D11Texture2D   *adopt_exportable(OwnedFd &descriptor, const D3D11_TEXTURE2D_DESC1 &desc);
    ID3D11Texture2D   *adopt_rows(OwnedFd &descriptor, const D3D11_TEXTURE2D_DESC1 &desc, uint32_t pitch);
    [[nodiscard]] bool exportable_pitch(ID3D11Texture2D *texture, uint32_t &pitch);
    ID3D11Texture2D   *make_rows(const D3D11_TEXTURE2D_DESC1 &desc);
    ID3D11Texture2D   *make_tiled(const D3D11_TEXTURE2D_DESC1 &desc);
    [[nodiscard]] bool export_memory(ID3D11Texture2D *texture, OwnedFd &descriptor);
    bool               slices() const;
    [[nodiscard]] bool import_to_draw(OwnedFd &descriptor, uint64_t bytes, VulkanBuffer &buffer) override;
    [[nodiscard]] bool bind_storage(ID3D11Buffer *buffer, const VulkanBuffer &storage, uint64_t offset,
                                    uint64_t bytes, void *host);
    DxvkVulkanDevice  &vulkan();
    HostFormatConverter &converter();

    ID3D11Device5        *device() const;
    ID3D11DeviceContext4 *context() const;
    const D3d11Capabilities &capabilities() const;
    void                  report(StateWriter &writer) const;

private:
    [[nodiscard]] bool make();
    void               unmake();
    bool               present() const override;
    void               enter() override;
    void               leave() override;
    bool               inside() const override;
    uint64_t           current_owner() const;
    bool               removed() override;
    [[nodiscard]] bool remake() override;
    void               carry_shares();
    uint32_t           restore_shares();
    bool               usable();
    ID3D11Texture2D   *presented_texture(uint32_t domain, uint64_t id);
    [[nodiscard]] bool exported(uint32_t slot);
    ID3D11Texture2D   *view_of(ID3D12Resource *resource);
    ID3D11Texture2D   *shared_texture(uint32_t domain, uint64_t id);
    IUnknown          *viewed(uint32_t domain, uint64_t id, IUnknown *found);
    ID3D11Texture2D   *made_shown(uint32_t domain, uint64_t id, ID3D11Texture2D *presented);
    void               retire_locked();
    ID3D11Texture2D   *readable_like(ID3D11Texture2D *texture);
    void               drop_read();
    [[nodiscard]] bool stage(uint32_t domain, uint64_t id, D3D11_TEXTURE2D_DESC &wanted);
    [[nodiscard]] bool drain(const D3D11_TEXTURE2D_DESC &wanted, uint8_t *pixels, uint32_t stride, uint32_t width,
                             uint32_t height);

    const D3dRuntime       *runtime_;
    DeviceUuid              uuid_;
    HostHeap                tenant_heap_;
    HostDeviceRenewal       renewal_;
    ID3D11Device5          *device_;
    D3d12HostDevice        *device12_;
    CompositorDevice *compositor_;
    ID3D11DeviceContext4   *context_;
    ID3DDeviceContextState *active_;
    uint32_t                feature_level_;
    D3d11Capabilities       capabilities_;
    HostShares             *shares_;
    DxvkVulkanDevice        vulkan_;
    ID3D11VkExtContext2    *slices_;
    HostImageExport         exports_;
    PlaneComposer           composer_;
    ExportableImages        images_;
    HostSurfaceCopy         copies_;
    HostFormatConverter     converter_;
    D3d11FramePool          frames_;
    ID3D11Texture2D        *staging_;
    ID3D11Texture2D        *read_;
    bool                    read_mapped_;
    PosixLock               lock_;
    uint64_t                owner_;
    uint32_t                waiting_;
    uint64_t                taken_;
    DriverLog              &lines_;
    MonotonicClock          clock_;
    HostHeap                carry_heap_;
    ObjectPool<CarriedShare> carried_;
};

}

#endif

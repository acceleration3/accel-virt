/* One host GPU's Direct3D 11 device, which every Direct3D 11 session on that
 * GPU runs on.
 *
 * Shared rather than one per session, because the desktop is several guest
 * processes drawing into each other: an application's swap chain buffer is a
 * texture the compositor samples, and two host devices could only hand it
 * across through external memory DXVK's native build does not offer. On one
 * device a shared resource is the same object in both sessions.
 *
 * So the device's immediate context is the one thing sessions contend for.
 * Each session keeps its pipeline state in a context state object of its own
 * (ID3DDeviceContextState), and whoever holds the lock swaps theirs in before
 * running a command - a swap only when the session changes, which is rare
 * inside a batch. Made once, at realize, because DXVK's device creation
 * costs a pipeline cache load that must not land inside a guest's first
 * batch (dxgkrnl's timeout, feature 044). */
#ifndef AV_D3D11_HOST_DEVICE_HPP
#define AV_D3D11_HOST_DEVICE_HPP

#include "core/log/driver_log.hpp"
#include "device/host/stdio_log.hpp"
#include "executor/shim/d3d11_capabilities.hpp"
#include "executor/shim/d3d11_frame_pool.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"
#include "executor/shim/exportable_images.hpp"
#include "executor/shim/host_image_export.hpp"
#include "executor/shim/plane_composer.hpp"

#include <pthread.h>
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

class FrameTarget;
class VulkanBuffer;

class D3dRuntime;
class HostShares;
class StagedFrame;
class StateWriter;
class CompositorDevice;
class D3d12HostDevice;
class Vkd3dImageExport;

class D3d11HostDevice {
public:
    D3d11HostDevice();
    ~D3d11HostDevice();
    D3d11HostDevice(const D3d11HostDevice &) = delete;
    D3d11HostDevice &operator=(const D3d11HostDevice &) = delete;

    /* The device on the GPU `uuid` names, and what it can do, sharing through
     * that GPU's `shares`. False for a GPU DXVK does not find, or one it will
     * not make a device on. */
    [[nodiscard]] bool open(const D3dRuntime &runtime, const uint8_t *uuid, HostShares &shares);
    void               close();
    bool               opened() const;

    /* Held around every command a session runs. `state` is the session's
     * own: swapped in when it is not the one in force. */
    void lock(ID3DDeviceContextState *state);
    /* Under the lock already held: `state` becomes the context's, if it is not. */
    void select(ID3DDeviceContextState *state);
    void unlock();
    /* A context state of the device's own, for a new session; null when the
     * device refuses one. The caller owns the reference. */
    ID3DDeviceContextState *make_state(uint32_t &feature_level);
    /* A session's state is going: if it is in force it stops being, so the
     * next lock swaps rather than trusting a pointer that was released. */
    void                    forget_state(ID3DDeviceContextState *state);
    /* Under the lock a session's command already holds: `presented`, a
     * flip-model buffer just presented, copied - in the presenting session's
     * order - into the texture its compositor reads in its place, made the
     * first time and kept with the share. DWM learns of the present only once
     * the guest's kernel has run it, and composes on its own clock: it read
     * the buffer itself after the kernel had handed it back to the
     * application, which was drawing its next frame there. A copy taken at
     * the present is what a flip shows, and nothing drawn afterwards reaches
     * it - as UTM's driver copies a composed present inside the application's
     * own stream. False for an id not shared, or a copy the device will not
     * make. */
    [[nodiscard]] bool      show(uint32_t domain, uint64_t id, ID3D11Texture2D *presented);
    /* Under the lock: what a session opening `id` gets (HostShares::
     * open_resource), with a reference the caller owns; null for none. A
     * Direct3D 12 resource is made a texture of DXVK's over the same memory
     * the first time (ExportableImages), kept with the share as its view -
     * only the surfaces something opens cost an import. */
    IUnknown               *open_shared(uint32_t domain, uint64_t id);
    /* A fence of the device's, starting at `initial`, the caller's; null when
     * the device makes none (D3d11BatchProgress). */
    ID3D11Fence            *make_fence(uint64_t initial);
    /* Under the lock a session's command already holds: the immediate context
     * signals `fence` with `value` behind everything recorded so far, and is
     * flushed so the GPU starts on it. False for no fence, or a refused
     * signal. */
    [[nodiscard]] bool      signal(ID3D11Fence *fence, uint64_t value);

    /* The texture shared as `id`, read into `pixels` as B8G8R8A8 rows
     * `stride` bytes apart, as much of it as fits `width` by `height`. False
     * for an id nothing is shared under. Takes the device's lock. */
    [[nodiscard]] bool read_image(uint32_t domain, uint64_t id, void *pixels, uint32_t stride, uint32_t width,
                                  uint32_t height);
    /* The image shared as `id` as a dma-buf (HostImageExport). */
    [[nodiscard]] bool export_image(uint32_t domain, uint64_t id, AvExportedImage &image);
    /* A head's `count` planes composed (PlaneComposer) and handed over as
     * export_image hands an image over. */
    [[nodiscard]] bool compose_planes(uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                      uint32_t height, AvExportedImage &image);
    /* The surface shared as `surface` as its own memory, a dma-buf the caller
     * owns, for a process outside QEMU (ExportableImages::export_image).
     * False for a surface nothing shares, or one made over memory no other
     * API can open. Takes the lock. */
    [[nodiscard]] bool export_surface(uint32_t domain, uint64_t surface, AvExportedImage &image);
    /* The copies into `surface` the host has run (HostShares::writes) and a
     * sync_file, the caller's, that signals once the GPU is done with them
     * (DxvkVulkanDevice::sync_file). False for a surface nothing shares, or a
     * device that makes no sync_file. Takes the lock. */
    [[nodiscard]] bool sync_surface(uint32_t domain, uint64_t surface, uint64_t &writes, int &fd);
    /* Under the lock a session's command already holds: subresource
     * `subresource` of `source` staged for the present engine
     * (D3d11FramePool::stage); null when no frame is free. */
    StagedFrame       *stage_frame(ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                                   uint32_t format);
    /* `texture`, a `width` by `height` frame the pool staged, written by the
     * GPU into `target` and finished (DxvkVulkanDevice::write): false where a
     * region lies outside either, or Vulkan cannot have the image or the
     * memory. Takes the lock to queue the write, and waits without it. */
    [[nodiscard]] bool write_frame(ID3D11Texture2D *texture, uint32_t width, uint32_t height,
                                   const FrameTarget &target);
    /* The COM references of shares HostShares freed given back, under the
     * lock, which this takes; and the images whose textures went with them
     * freed. After anything that may free a share: a guest's release, a
     * session's end. */
    void               retire_shares();
    /* The GPU's Direct3D 12 device, whose surfaces this one makes views of
     * (open_shared); null for none. */
    void               pair(D3d12HostDevice *device12);
    /* The GPU's compositing context, whose images the shared textures of
     * other APIs' pictures are made over (CompositorDevice); null for none. */
    void               use_compositor(CompositorDevice *compositor);
    CompositorDevice  *compositor() const;
    /* Under the lock: a shared texture described as `desc` over the
     * compositor's dma-buf `fd` (ExportableImages::adopt), with a reference
     * the caller owns; the fd is taken whatever happens. */
    ID3D11Texture2D   *adopt_exportable(int fd, const D3D11_TEXTURE2D_DESC1 &desc);
    /* Under the lock: the row pitch of the linear image make_exportable made
     * `texture` over (ExportableImages::row_pitch). */
    [[nodiscard]] bool exportable_pitch(ID3D11Texture2D *texture, uint32_t &pitch);
    /* Under the lock: a shared texture described as `desc`, made over memory
     * OpenGL and Vulkan can open (ExportableImages::make), with a reference
     * the caller owns; null for one only DXVK's own allocation can hold. */
    ID3D11Texture2D   *make_exportable(const D3D11_TEXTURE2D_DESC1 &desc);
    /* Under the lock: a dma-buf of the memory make_exportable made `texture`
     * over, which the caller owns; false for any other texture. */
    [[nodiscard]] bool export_memory(ID3D11Texture2D *texture, int &fd);
    /* DXVK's Vulkan device, which a Direct3D 12 session's presents wait on. */
    /* Whether a buffer can be drawn from guest memory in place: DXVK renames
     * buffers to storage imported here (tools/dxvk-patches/0003) and offers
     * Vulkan interop to import it with. */
    bool               slices() const;
    /* `bytes` of the dma-buf `fd` imported on DXVK's device, to draw from;
     * the fd is the buffer's whatever happens. */
    [[nodiscard]] bool import_to_draw(int fd, uint64_t bytes, VulkanBuffer &buffer);
    /* `buffer` from now on is `bytes` at `offset` of `storage`, which the CPU
     * reaches at `host`: recorded on the immediate context, so what was
     * recorded before reads its storage before. Under the device's lock. */
    [[nodiscard]] bool bind_storage(ID3D11Buffer *buffer, const VulkanBuffer &storage, uint64_t offset,
                                    uint64_t bytes, void *host);
    DxvkVulkanDevice  &vulkan();

    ID3D11Device5        *device() const;
    ID3D11DeviceContext4 *context() const;
    const D3d11Capabilities &capabilities() const;
    /* The exportable images, for the executor's state dump. */
    void                  report(StateWriter &writer) const;

private:
    /* Under the lock: the copy a present of `id` showed, or the texture
     * itself where none was taken; a reference the caller owns. */
    ID3D11Texture2D   *presented_texture(uint32_t domain, uint64_t id);
    /* Whether the export copy into `slot` finished, waiting as long as a slow
     * frame takes. */
    [[nodiscard]] bool exported(uint32_t slot);
    /* Under the lock: a texture of DXVK's over `resource`'s memory, with a
     * reference the caller owns; null for one DXVK cannot import - not one
     * 2D image of one mip and one sample. */
    ID3D11Texture2D   *view_of(ID3D12Resource *resource);
    /* Under the lock: what `id` shares, as a texture of this device's, with a
     * reference the caller owns; null for none. Not an open: the scanout and
     * the export socket read it. */
    ID3D11Texture2D   *shared_texture(uint32_t domain, uint64_t id);
    /* Under the lock: `found`, what `id` shares, as this device sees it - a
     * Direct3D 12 resource made its view, kept with the share, the first time
     * - with the reference `found` came with; null for none. */
    IUnknown          *viewed(uint32_t domain, uint64_t id, IUnknown *found);
    /* Under the lock: what retire_shares does. */
    void               retire_locked();
    /* The shared texture copied into staging_, queued, under the lock. */
    [[nodiscard]] bool stage(uint32_t domain, uint64_t id, D3D11_TEXTURE2D_DESC &wanted);
    /* staging_ read once the GPU is done with it, taking the lock only to look. */
    [[nodiscard]] bool drain(const D3D11_TEXTURE2D_DESC &wanted, uint8_t *pixels, uint32_t stride, uint32_t width,
                             uint32_t height);

    ID3D11Device5          *device_;
    D3d12HostDevice        *device12_;
    CompositorDevice *compositor_;
    ID3D11DeviceContext4   *context_;
    ID3DDeviceContextState *active_;
    uint32_t                feature_level_;
    D3d11Capabilities       capabilities_;
    HostShares             *shares_;
    DxvkVulkanDevice        vulkan_;
    ID3D11VkExtContext2    *slices_;   /* held; null where DXVK lacks it */
    HostImageExport         exports_;
    PlaneComposer           composer_;
    ExportableImages        images_;
    D3d11FramePool          frames_;
    /* The staging texture images are read through, kept while the size and
     * format the scanout asks for stay the same. */
    ID3D11Texture2D        *staging_;
    pthread_mutex_t         mutex_;
    /* Workers waiting for the lock, and how often it was taken: a worker that
     * lets go while another waits holds off until that one has it, since a
     * mutex handed back at once goes to whoever asks first - the worker that
     * just let go - and a session with batches queued starved every other,
     * the desktop's among them. */
    uint32_t                waiting_;
    uint64_t                taken_;
    /* QEMU's stderr, where a surface the host would not share is seen. */
    StdioLog                sink_;
    DriverLog               lines_;
};

}  // namespace av

#endif  // AV_D3D11_HOST_DEVICE_HPP

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

#include "device/shim/d3d11_capabilities.hpp"
#include "device/shim/d3d_shared_resources.hpp"
#include "device/shim/host_image_export.hpp"

#include <pthread.h>
#include <stdint.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3DDeviceContextState;
struct D3D11_TEXTURE2D_DESC;
struct ID3D11Texture2D;

namespace av {

class D3dRuntime;

class D3d11HostDevice {
public:
    D3d11HostDevice();
    ~D3d11HostDevice();
    D3d11HostDevice(const D3d11HostDevice &) = delete;
    D3d11HostDevice &operator=(const D3d11HostDevice &) = delete;

    /* The device on the GPU `uuid` names, and what it can do. False for a GPU
     * DXVK does not find, or one it will not make a device on. */
    [[nodiscard]] bool open(const D3dRuntime &runtime, const uint8_t *uuid);
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

    /* The texture published under `key`, read into `pixels` as B8G8R8A8 rows
     * `stride` bytes apart, as much of it as fits `width` by `height`. False
     * for a key nothing is published under. Takes the device's lock. */
    [[nodiscard]] bool read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height);
    /* The image published under `key` as a dma-buf (HostImageExport). */
    [[nodiscard]] bool export_image(uint64_t key, RvkExportedImage &image);

    ID3D11Device5        *device() const;
    ID3D11DeviceContext4 *context() const;
    const D3d11Capabilities &capabilities() const;
    D3dSharedResources   &shared();

private:
    /* The published texture copied into staging_, queued, under the lock. */
    [[nodiscard]] bool stage(uint64_t key, D3D11_TEXTURE2D_DESC &wanted);
    /* staging_ read once the GPU is done with it, taking the lock only to look. */
    [[nodiscard]] bool drain(const D3D11_TEXTURE2D_DESC &wanted, uint8_t *pixels, uint32_t stride, uint32_t width,
                             uint32_t height);

    ID3D11Device5          *device_;
    ID3D11DeviceContext4   *context_;
    ID3DDeviceContextState *active_;
    uint32_t                feature_level_;
    D3d11Capabilities       capabilities_;
    D3dSharedResources      shared_;
    HostImageExport         exports_;
    /* The staging texture images are read through, kept while the size and
     * format the scanout asks for stay the same. */
    ID3D11Texture2D        *staging_;
    pthread_mutex_t         mutex_;
};

}  // namespace av

#endif  // AV_D3D11_HOST_DEVICE_HPP

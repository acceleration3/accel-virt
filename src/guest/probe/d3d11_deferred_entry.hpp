/* The Direct3D 11 the deferred-map scenario calls (DeferredMapWork): one
 * method per call Dark Souls Remastered makes to draw a frame through its
 * deferred contexts, each answering the HRESULT as it stands. In the guest it
 * is the system's d3d11.dll on this driver; in the probe's unit tests a
 * scripted one. Virtual for the reason Platform is. */
#ifndef AV_D3D11_DEFERRED_ENTRY_HPP
#define AV_D3D11_DEFERRED_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11DeferredEntry {
public:
    D3d11DeferredEntry(const D3d11DeferredEntry &) = delete;
    D3d11DeferredEntry &operator=(const D3d11DeferredEntry &) = delete;

    /* D3D11CreateDeviceAndSwapChain on `window`, `width` by `height`, two
     * buffers flipped. */
    virtual int32_t  create_device(void *window, uint32_t width, uint32_t height) = 0;
    /* Dynamic constant buffers of `sizes` bytes each, `count` of them, one
     * dynamic buffer of `geometry_bytes` bound as both vertex and index
     * buffer, every one written by the CPU, and a staging buffer of
     * `readback_bytes` to read the geometry buffer's head back through. */
    virtual int32_t  create_buffers(const uint32_t *sizes, uint32_t count, uint32_t geometry_bytes,
                                    uint32_t readback_bytes) = 0;
    /* CreateDeferredContext, `count` times. */
    virtual int32_t  create_contexts(uint32_t count) = 0;
    /* On deferred context `context`: constant buffer `buffer` mapped with
     * WRITE_DISCARD, every byte of it written `value`, and unmapped. */
    virtual int32_t  write_constants(uint32_t context, uint32_t buffer, uint8_t value) = 0;
    /* On deferred context `context`: the geometry buffer mapped with
     * WRITE_DISCARD and its first `bytes` written `value`. */
    virtual int32_t  discard_geometry(uint32_t context, uint32_t bytes, uint8_t value) = 0;
    /* On deferred context `context`: the geometry buffer mapped with
     * WRITE_NO_OVERWRITE and `bytes` at `offset` written `value`. */
    virtual int32_t  append_geometry(uint32_t context, uint32_t offset, uint32_t bytes, uint8_t value) = 0;
    /* FinishCommandList on deferred context `context`, the list kept for
     * execute. */
    virtual int32_t  finish(uint32_t context) = 0;
    /* ExecuteCommandList of `context`'s list on the immediate context, and
     * the list released, as a game lets go of each frame's. */
    virtual int32_t  execute(uint32_t context) = 0;
    virtual int32_t  present() = 0;
    /* The geometry buffer's first `bytes` copied into the staging buffer on
     * the immediate context, mapped for reading and copied out. */
    virtual int32_t  read_geometry(uint8_t *bytes, uint32_t count) = 0;
    /* The memory the process has committed for itself (its private bytes). */
    virtual uint64_t committed_bytes() = 0;
    virtual void     release() = 0;

protected:
    D3d11DeferredEntry() = default;
    ~D3d11DeferredEntry() = default;
};

}  // namespace av

#endif  // AV_D3D11_DEFERRED_ENTRY_HPP

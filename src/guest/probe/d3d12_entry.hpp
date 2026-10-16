/* The Direct3D 12 the D3D probe calls (feature 047, US2), on one device the
 * implementation owns: one method per call the probe makes, each answering the
 * HRESULT as it stands. In the guest it is the d3d12.dll beside the probe -
 * vkd3d-proton's, which is the point of the run - and in the probe's unit
 * tests a scripted D3D12. Virtual for the reason Platform is.
 *
 * A cleared render target rather than a drawn triangle, for the reason
 * D3d11Entry says: a triangle needs DXBC and no shader compiler is pinned
 * here. A clear still goes through the device, the queue, a committed
 * resource, a command list, a fence and a readback, which is everything a
 * layer has to get right before it draws anything. */
#ifndef AV_D3D12_ENTRY_HPP
#define AV_D3D12_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d12Entry {
public:
    D3d12Entry(const D3d12Entry &) = delete;
    D3d12Entry &operator=(const D3d12Entry &) = delete;

    /* D3D12CreateDevice on the first adapter DXGI lists, answering the
     * feature level the device was made at. */
    virtual int32_t create_device(uint32_t &feature_level) = 0;
    /* The adapter's LUID, which is what the runner compares with the
     * driver's. */
    virtual int32_t adapter_luid(uint64_t &luid) = 0;
    /* What a game engine asks of a new device before anything else (Resident
     * Evil 4's): where the device reports a library-capable shader cache, an
     * empty pipeline library made, serialized into `serialized` bytes and
     * made again from them; then the device asked whether it is still there.
     * S_OK with `serialized` 0 where the device reports no library. */
    virtual int32_t pipeline_library(uint32_t &serialized) = 0;
    /* A direct command queue, a command allocator and one command list. */
    virtual int32_t create_queue() = 0;
    /* A `width` by `height` RGBA8 render target, a descriptor heap for its
     * view, and a readback buffer the size of its rows. */
    virtual int32_t create_render_target(uint32_t width, uint32_t height) = 0;
    /* One recorded and executed list: the target cleared to `colour` - four
     * bytes, red first - and copied into the readback buffer. */
    virtual int32_t clear_and_copy(const uint8_t *colour) = 0;
    /* The fence the queue signalled after that list. */
    virtual int32_t wait() = 0;
    /* `bytes` of the readback buffer's mapping into `pixels`: RGBA rows, top
     * row first, as the copy left them. */
    virtual int32_t read_back(uint8_t *pixels, uint32_t bytes) = 0;
    /* A heap over 2 * `bytes` of the probe's own memory
     * (ID3D12Device3::OpenExistingHeapFromAddress): `pattern` written into its
     * first half through the probe's pointer, a buffer placed over each half,
     * the first copied into the second on the queue, and the second half read
     * through the probe's pointer into `copied`. `aliased` is 1 when Map of
     * the first buffer answered the probe's own pointer. */
    virtual int32_t existing_heap(const uint8_t *pattern, uint8_t *copied, uint32_t bytes, uint32_t &aliased) = 0;
    /* A committed buffer on a CUSTOM write-back L0 heap made with
     * D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH, as GFXReconstruct's capture makes
     * every upload buffer: mapped, two of its pages written, and the count
     * of written pages GetWriteWatch answered for the mapping in `pages`, or
     * 0xFFFFFFFF when it refused the mapping. The result is the buffer's
     * creation and its Map. */
    virtual int32_t write_watch(uint32_t &pages) = 0;
    virtual void    release() = 0;

protected:
    D3d12Entry() = default;
    ~D3d12Entry() = default;
};

}  // namespace av

#endif  // AV_D3D12_ENTRY_HPP

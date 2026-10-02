/* The Direct3D 9 the D3D probe calls (feature 047, US1), on one window and one
 * device the implementation owns: one method per call the probe makes, each
 * answering the HRESULT as it stands. In the guest it is the d3d9.dll beside
 * the probe - DXVK's - and in the probe's unit tests a scripted D3D9. Virtual
 * for the reason Platform is.
 *
 * A cleared frame rather than a drawn triangle, for the reason D3d11Entry
 * says. D3D9 has no LUID to answer: what a device names its adapter by is the
 * driver's own identifier, which the probe reports as it is given. */
#ifndef AV_D3D9_ENTRY_HPP
#define AV_D3D9_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d9Entry {
public:
    D3d9Entry(const D3d9Entry &) = delete;
    D3d9Entry &operator=(const D3d9Entry &) = delete;

    /* Direct3DCreate9 and CreateDevice on `window` with a `width` by `height`
     * back buffer. */
    virtual int32_t create_device(void *window, uint32_t width, uint32_t height) = 0;
    /* What GetAdapterIdentifier says the adapter is: its vendor and device
     * ids, which is as close to an adapter's identity as D3D9 gets. */
    virtual int32_t adapter_ids(uint32_t &vendor, uint32_t &device) = 0;
    /* Clear of the whole target to `colour`, four bytes red first, between
     * BeginScene and EndScene. */
    virtual int32_t clear(const uint8_t *colour) = 0;
    /* GetRenderTargetData into a system-memory surface, LockRect, `bytes` of
     * its rows into `pixels` - BGRA, top row first, as a D3D9 surface holds
     * them - and UnlockRect. */
    virtual int32_t read_back(uint8_t *pixels, uint32_t bytes) = 0;
    virtual int32_t present() = 0;
    virtual void    release() = 0;

protected:
    D3d9Entry() = default;
    ~D3d9Entry() = default;
};

}  // namespace av

#endif  // AV_D3D9_ENTRY_HPP

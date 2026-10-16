/* The Direct3D 11 the target-independent rasterization scenario calls
 * (TirCoverageWork): a device of its own and the pipeline Direct2D draws its
 * coverage atlas with, each call answering the HRESULT as it stands. In the
 * guest it is the system's d3d11.dll on this driver, the shaders hand-
 * assembled DXBC; in the probe's unit tests a scripted one. Virtual for the
 * reason Platform is. */
#ifndef AV_D3D11_TIR_ENTRY_HPP
#define AV_D3D11_TIR_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11TirEntry {
public:
    D3d11TirEntry(const D3d11TirEntry &) = delete;
    D3d11TirEntry &operator=(const D3d11TirEntry &) = delete;

    /* D3D11CreateDevice on the default adapter at feature level 11_1, the
     * level target-independent rasterization needs, with no window. */
    virtual int32_t create_device() = 0;
    /* An `extent` square R32_UINT render target and a staging texture like
     * it; a vertex shader passing clip-space x and y through and a pixel
     * shader writing SV_Coverage to the target; a rasterizer state forcing
     * `samples` samples, culling nothing; a blend state whose logic op ORs
     * into the target; and everything bound, the viewport the target's. */
    virtual int32_t create_pipeline(uint32_t extent, uint32_t samples) = 0;
    /* The target cleared to zero and `count` vertices - clip-space x, y
     * pairs - drawn as a triangle list in one call. */
    virtual int32_t draw(const float *positions, uint32_t count) = 0;
    /* The target copied to the staging texture, mapped for reading, and
     * its `count` texels written to `texels`, rows from the top. */
    virtual int32_t read_back(uint32_t *texels, uint32_t count) = 0;
    /* GetDeviceRemovedReason: S_OK while the device stands. */
    virtual int32_t removed_reason() = 0;
    /* Whether the device is a layer's - DXVK beside the probe, on the Vulkan
     * ICD - rather than the system's runtime on this driver's avumd, whose
     * devices run on the host's DXVK. */
    virtual bool    layered() = 0;
    /* Everything the device made, and the device. */
    virtual void    release() = 0;

protected:
    D3d11TirEntry() = default;
    ~D3d11TirEntry() = default;
};

}  // namespace av

#endif  // AV_D3D11_TIR_ENTRY_HPP

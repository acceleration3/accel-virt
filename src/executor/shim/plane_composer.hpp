/* A display head's overlay planes composed into one image on the host GPU,
 * as a display controller layers them at scanout (docs/protocol.md,
 * "Scanout planes"): a render target of the head's size, cleared to black,
 * and every plane drawn into it by one video processor blit, a stream per
 * plane in layer order - each its source rectangle scaled into its
 * destination, and a YCbCr plane converted on the way. DXVK's video processor
 * draws the streams one over another in the order given and leaves the target
 * outside them as it was, which is what layering is. Translation only; what
 * the guest may ask for is decided by avgpu (OverlaySupport). */
#ifndef AV_PLANE_COMPOSER_HPP
#define AV_PLANE_COMPOSER_HPP

#include "device/abi/av_executor.h"

#include <stdint.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11VideoDevice;
struct ID3D11VideoContext;
struct ID3D11VideoProcessorEnumerator;
struct ID3D11VideoProcessor;
struct ID3D11VideoProcessorOutputView;

namespace av {

class PlaneComposer {
public:
    /* The most planes one frame layers: avgpu's OverlayPlane::max_planes. */
    static constexpr uint32_t max_planes = 4u;

    PlaneComposer();
    ~PlaneComposer();
    PlaneComposer(const PlaneComposer &) = delete;
    PlaneComposer &operator=(const PlaneComposer &) = delete;

    /* Under the device's lock: `count` planes, each over its texture in
     * `textures`, composed into the head's `width` by `height` target, whose
     * texture - B8G8R8A8, the composer's - is returned; null where the device
     * has no video processor or a plane cannot be read. The work is queued,
     * not waited for. */
    ID3D11Texture2D *compose(ID3D11Device5 *device, ID3D11DeviceContext4 *context, const AvScanoutPlane *planes,
                             ID3D11Texture2D *const *textures, uint32_t count, uint32_t width, uint32_t height);
    void             release();

private:
    /* The target and the processor for a head this size, made once per size. */
    [[nodiscard]] bool prepare(ID3D11Device5 *device, ID3D11DeviceContext4 *context, uint32_t width, uint32_t height);

    ID3D11VideoDevice              *video_;
    ID3D11VideoContext             *video_context_;
    ID3D11VideoProcessorEnumerator *enumerator_;
    ID3D11VideoProcessor           *processor_;
    ID3D11Texture2D                *target_;
    ID3D11RenderTargetView         *target_view_;
    ID3D11VideoProcessorOutputView *output_;
    uint32_t                        width_;
    uint32_t                        height_;
};

}  // namespace av

#endif  // AV_PLANE_COMPOSER_HPP

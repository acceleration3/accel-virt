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
    static constexpr uint32_t max_planes = 4u;

    PlaneComposer();
    ~PlaneComposer();
    PlaneComposer(const PlaneComposer &) = delete;
    PlaneComposer &operator=(const PlaneComposer &) = delete;

    ID3D11Texture2D *compose(ID3D11Device5 *device, ID3D11DeviceContext4 *context, const AvScanoutPlane *planes,
                             ID3D11Texture2D *const *textures, uint32_t count, uint32_t width, uint32_t height);
    void             release();

private:
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

}

#endif

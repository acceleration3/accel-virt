#ifndef AV_FLOAT_BLIT_PASS_HPP
#define AV_FLOAT_BLIT_PASS_HPP

#include <stdint.h>

struct ID3D11Buffer;
struct ID3D11Device5;
struct ID3D11DeviceContext;
struct ID3D11DeviceContext4;
struct ID3D11PixelShader;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11VertexShader;

namespace av {

class FloatBlitPass {
public:
    struct Rectangle {
        uint32_t left;
        uint32_t top;
        uint32_t right;
        uint32_t bottom;
    };

    FloatBlitPass();
    ~FloatBlitPass();
    FloatBlitPass(const FloatBlitPass &) = delete;
    FloatBlitPass &operator=(const FloatBlitPass &) = delete;

    ID3D11Texture2D *draw(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                          uint32_t subresource, const Rectangle &from, uint32_t width, uint32_t height,
                          uint32_t target_format, bool encode);
    void             release();

private:
    [[nodiscard]] bool arm(ID3D11Device5 *device);
    [[nodiscard]] bool prepare_target(ID3D11Device5 *device, uint32_t width, uint32_t height, uint32_t format);
    [[nodiscard]] bool prepare_copy(ID3D11Device5 *device, uint32_t width, uint32_t height, uint32_t format);

    ID3D11VertexShader     *vertex_;
    ID3D11PixelShader      *pixel_;
    ID3D11Buffer           *constants_;
    ID3D11DeviceContext    *recorder_;
    ID3D11Texture2D        *target_;
    ID3D11RenderTargetView *target_view_;
    ID3D11Texture2D        *copy_;
    uint32_t                target_width_;
    uint32_t                target_height_;
    uint32_t                target_format_;
    uint32_t                copy_width_;
    uint32_t                copy_height_;
    uint32_t                copy_format_;
};

}

#endif

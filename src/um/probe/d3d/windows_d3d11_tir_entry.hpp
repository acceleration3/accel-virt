#ifndef AV_WINDOWS_D3D11_TIR_ENTRY_HPP
#define AV_WINDOWS_D3D11_TIR_ENTRY_HPP

#include "guest/probe/d3d11_tir_entry.hpp"

#include <d3d11_1.h>

namespace av {

class WindowsD3d11TirEntry final : public D3d11TirEntry {
public:
    WindowsD3d11TirEntry();
    ~WindowsD3d11TirEntry();
    WindowsD3d11TirEntry(const WindowsD3d11TirEntry &) = delete;
    WindowsD3d11TirEntry &operator=(const WindowsD3d11TirEntry &) = delete;

    int32_t create_device() override;
    int32_t create_pipeline(uint32_t extent, uint32_t samples) override;
    int32_t draw(const float *positions, uint32_t count) override;
    int32_t read_back(uint32_t *texels, uint32_t count) override;
    int32_t removed_reason() override;
    bool    layered() override;
    void    release() override;

private:
    int32_t create_targets(uint32_t extent);
    int32_t create_shaders();
    int32_t create_states(uint32_t samples);

    ID3D11Device1          *device_;
    ID3D11DeviceContext    *context_;
    ID3D11Texture2D        *target_;
    ID3D11RenderTargetView *view_;
    ID3D11Texture2D        *staging_;
    ID3D11VertexShader     *vertex_shader_;
    ID3D11PixelShader      *pixel_shader_;
    ID3D11InputLayout      *layout_;
    ID3D11RasterizerState1 *rasterizer_;
    ID3D11BlendState1      *blend_;
    ID3D11Buffer           *vertices_;
    uint32_t                extent_;
};

}

#endif

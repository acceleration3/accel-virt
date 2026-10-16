/* D3d11TirEntry over the system's d3d11.dll, which in the guest is
 * Microsoft's runtime on this driver: one device with no window, the
 * coverage atlas Direct2D draws into (an R32_UINT target here, to keep all
 * 16 bits of a mask and more), the shaders in tir_coverage_shaders.hpp, and
 * the states that make it target-independent rasterization. Translation
 * only - what the probe decides is in src/guest/probe/tir_coverage_work.cpp. */
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
    /* The target and its staging copy, `extent` square. */
    int32_t create_targets(uint32_t extent);
    /* The shaders and the input layout their vertices take. */
    int32_t create_shaders();
    /* The rasterizer state forcing `samples` and the OR'ing blend state. */
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

}  // namespace av

#endif  // AV_WINDOWS_D3D11_TIR_ENTRY_HPP

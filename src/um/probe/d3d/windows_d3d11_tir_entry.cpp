#include "um/probe/d3d/windows_d3d11_tir_entry.hpp"

#include "core/trace/function_scope.hpp"
#include "um/probe/d3d/layered_device.hpp"
#include "um/probe/d3d/tir_coverage_shaders.hpp"

namespace av {

namespace {

constexpr HRESULT  invalid_argument = static_cast<HRESULT>(0x80070057u);
constexpr uint32_t texel_bytes = 4u;
constexpr uint32_t vertex_bytes = 2u * sizeof(float);

}

WindowsD3d11TirEntry::WindowsD3d11TirEntry()
    : device_(nullptr), context_(nullptr), target_(nullptr), view_(nullptr), staging_(nullptr),
      vertex_shader_(nullptr), pixel_shader_(nullptr), layout_(nullptr), rasterizer_(nullptr), blend_(nullptr),
      vertices_(nullptr), extent_(0u)
{
    AV_SCOPE("WindowsD3d11TirEntry::WindowsD3d11TirEntry");
}

WindowsD3d11TirEntry::~WindowsD3d11TirEntry()
{
    AV_SCOPE("WindowsD3d11TirEntry::~WindowsD3d11TirEntry");
    release();
}

int32_t WindowsD3d11TirEntry::create_device()
{
    AV_SCOPE("WindowsD3d11TirEntry::create_device");
    const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_1;
    ID3D11Device           *device = nullptr;
    const HRESULT           created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0u, &level, 1u,
                                                        D3D11_SDK_VERSION, &device, nullptr, &context_);
    if (created < 0) return created;
    const HRESULT asked = device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void **>(&device_));
    device->Release();
    return asked;
}

int32_t WindowsD3d11TirEntry::create_pipeline(uint32_t extent, uint32_t samples)
{
    AV_SCOPE("WindowsD3d11TirEntry::create_pipeline");
    HRESULT made = create_targets(extent);
    if (made >= 0) made = create_shaders();
    if (made >= 0) made = create_states(samples);
    if (made < 0) return made;
    const D3D11_VIEWPORT viewport = {0.0F, 0.0F, static_cast<float>(extent), static_cast<float>(extent), 0.0F, 1.0F};
    context_->OMSetRenderTargets(1u, &view_, nullptr);
    context_->OMSetBlendState(blend_, nullptr, 0xffffffffu);
    context_->RSSetState(rasterizer_);
    context_->RSSetViewports(1u, &viewport);
    context_->IASetInputLayout(layout_);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertex_shader_, nullptr, 0u);
    context_->PSSetShader(pixel_shader_, nullptr, 0u);
    return made;
}

int32_t WindowsD3d11TirEntry::create_targets(uint32_t extent)
{
    AV_SCOPE("WindowsD3d11TirEntry::create_targets");
    extent_ = extent;
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = extent;
    described.Height = extent;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = DXGI_FORMAT_R32_UINT;
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = D3D11_BIND_RENDER_TARGET;
    HRESULT made = device_->CreateTexture2D(&described, nullptr, &target_);
    if (made < 0) return made;
    made = device_->CreateRenderTargetView(target_, nullptr, &view_);
    if (made < 0) return made;
    described.Usage = D3D11_USAGE_STAGING;
    described.BindFlags = 0u;
    described.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return device_->CreateTexture2D(&described, nullptr, &staging_);
}

int32_t WindowsD3d11TirEntry::create_shaders()
{
    AV_SCOPE("WindowsD3d11TirEntry::create_shaders");
    HRESULT made = device_->CreateVertexShader(tir_coverage_vs, sizeof(tir_coverage_vs), nullptr, &vertex_shader_);
    if (made < 0) return made;
    made = device_->CreatePixelShader(tir_coverage_ps, sizeof(tir_coverage_ps), nullptr, &pixel_shader_);
    if (made < 0) return made;
    const D3D11_INPUT_ELEMENT_DESC position = {"POSITION", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u, 0u,
                                               D3D11_INPUT_PER_VERTEX_DATA, 0u};
    return device_->CreateInputLayout(&position, 1u, tir_coverage_vs, sizeof(tir_coverage_vs), &layout_);
}

int32_t WindowsD3d11TirEntry::create_states(uint32_t samples)
{
    AV_SCOPE("WindowsD3d11TirEntry::create_states");
    D3D11_RASTERIZER_DESC1 rasterizer = {};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.ForcedSampleCount = samples;
    const HRESULT made = device_->CreateRasterizerState1(&rasterizer, &rasterizer_);
    if (made < 0) return made;
    D3D11_BLEND_DESC1 blend = {};
    for (D3D11_RENDER_TARGET_BLEND_DESC1 &target : blend.RenderTarget) {
        target.LogicOpEnable = TRUE;
        target.SrcBlend = D3D11_BLEND_ONE;
        target.DestBlend = D3D11_BLEND_ZERO;
        target.BlendOp = D3D11_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D11_BLEND_ONE;
        target.DestBlendAlpha = D3D11_BLEND_ZERO;
        target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        target.LogicOp = D3D11_LOGIC_OP_OR;
        target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    return device_->CreateBlendState1(&blend, &blend_);
}

int32_t WindowsD3d11TirEntry::draw(const float *positions, uint32_t count)
{
    AV_SCOPE("WindowsD3d11TirEntry::draw");
    if (vertices_ != nullptr) vertices_->Release();
    vertices_ = nullptr;
    D3D11_BUFFER_DESC described = {};
    described.ByteWidth = count * vertex_bytes;
    described.Usage = D3D11_USAGE_IMMUTABLE;
    described.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = positions;
    const HRESULT made = device_->CreateBuffer(&described, &initial, &vertices_);
    if (made < 0) return made;
    const UINT  stride = vertex_bytes;
    const UINT  offset = 0u;
    const float cleared[4] = {0.0F, 0.0F, 0.0F, 0.0F};
    context_->ClearRenderTargetView(view_, cleared);
    context_->IASetVertexBuffers(0u, 1u, &vertices_, &stride, &offset);
    context_->Draw(count, 0u);
    return made;
}

int32_t WindowsD3d11TirEntry::read_back(uint32_t *texels, uint32_t count)
{
    AV_SCOPE("WindowsD3d11TirEntry::read_back");
    if (staging_ == nullptr || count != extent_ * extent_) return invalid_argument;
    context_->CopyResource(staging_, target_);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT            asked = context_->Map(staging_, 0u, D3D11_MAP_READ, 0u, &mapped);
    if (asked < 0) return asked;
    for (uint32_t row = 0u; row < extent_; row++) {
        const uint8_t *const from = static_cast<const uint8_t *>(mapped.pData) + row * mapped.RowPitch;
        for (uint32_t column = 0u; column < extent_; column++) {
            uint32_t texel = 0u;
            for (uint32_t byte = 0u; byte < texel_bytes; byte++)
                texel |= static_cast<uint32_t>(from[column * texel_bytes + byte]) << (8u * byte);
            texels[row * extent_ + column] = texel;
        }
    }
    context_->Unmap(staging_, 0u);
    return asked;
}

int32_t WindowsD3d11TirEntry::removed_reason()
{
    AV_SCOPE("WindowsD3d11TirEntry::removed_reason");
    return device_->GetDeviceRemovedReason();
}

bool WindowsD3d11TirEntry::layered()
{
    AV_SCOPE("WindowsD3d11TirEntry::layered");
    return LayeredDevice(device_).layered();
}

void WindowsD3d11TirEntry::release()
{
    AV_SCOPE("WindowsD3d11TirEntry::release");
    IUnknown *const made[] = {vertices_, blend_,  rasterizer_, layout_,  pixel_shader_, vertex_shader_,
                              staging_,  view_,   target_,     context_, device_};
    for (IUnknown *object : made)
        if (object != nullptr) object->Release();
    vertices_ = nullptr;
    blend_ = nullptr;
    rasterizer_ = nullptr;
    layout_ = nullptr;
    pixel_shader_ = nullptr;
    vertex_shader_ = nullptr;
    staging_ = nullptr;
    view_ = nullptr;
    target_ = nullptr;
    context_ = nullptr;
    device_ = nullptr;
    extent_ = 0u;
}

}

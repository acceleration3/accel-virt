// The D3D11 samples' framework: what a sample is, what it is handed, and the
// few services the two platforms give it. The guest build (Windows, no C
// runtime) compiles HLSL with d3dcompiler_47 and keeps the bytecode; the host
// build (Linux, DXVK native) loads that bytecode, so both run the same shaders.
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <stddef.h>
#include <stdint.h>

#include "smath.h"

namespace samples {

// Declared rather than a using-directive: the host's C library has its own
// sin, cos, abs... and a directive would make every call ambiguous.
using sm::pi; using sm::sqrt; using sm::abs; using sm::floor; using sm::fract; using sm::sin; using sm::cos;
using sm::tan; using sm::radians; using sm::min; using sm::max; using sm::clamp; using sm::mix; using sm::Random;
using sm::float2; using sm::float3; using sm::float4; using sm::float4x4; using sm::dot; using sm::cross;
using sm::length; using sm::normalize; using sm::translate; using sm::scale; using sm::rotate_x; using sm::rotate_y;
using sm::rotate_z; using sm::perspective; using sm::orthographic; using sm::look_at;

// ---- platform (guest_platform.cpp / host_platform.cpp)
struct Blob {
    void  *data = nullptr;
    size_t size = 0;
};
void   log(const char *text);
void   log_number(const char *label, double value);
void  *allocate(size_t bytes);   // zeroed
void   release_memory(void *memory);
double seconds();
bool   write_file(const char *path, const void *data, size_t size);
bool   read_file(const char *path, Blob &blob);
// Bytecode for `entry` of `source` under `target`, cached as <dir>/<name>.dxbc:
// compiled on the guest, loaded on the host.
bool   shader(const char *name, const char *source, const char *entry, const char *target, Blob &bytecode);
void   set_shader_directory(const char *directory);

// ---- text
void  copy_text(char *to, size_t capacity, const char *from);
void  append_text(char *to, size_t capacity, const char *from);
void  append_number(char *to, size_t capacity, double value, int decimals);
bool  same_text(const char *a, const char *b);

template <typename T> void safe_release(T *&object)
{
    if (object != nullptr) object->Release();
    object = nullptr;
}

// ---- meshes
struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    float3 color;
};
struct Mesh {
    Vertex   *vertices = nullptr;
    uint32_t *indices = nullptr;
    uint32_t  vertex_count = 0;
    uint32_t  index_count = 0;
    void      release();
};
Mesh cube_mesh(float size);
Mesh sphere_mesh(float radius, uint32_t slices, uint32_t stacks);
Mesh torus_mesh(float radius, float tube, uint32_t rings, uint32_t sides);
Mesh plane_mesh(float size, uint32_t cells, float uv_scale);
Mesh gear_mesh(float inner, float outer, float width, uint32_t teeth, float tooth_depth, float3 color);

// ---- GPU helpers
struct DrawMesh {
    ID3D11Buffer *vertices = nullptr;
    ID3D11Buffer *indices = nullptr;
    uint32_t      index_count = 0;
    void          release();
    void          draw(ID3D11DeviceContext *context, uint32_t instances = 1) const;
};

class Gpu {
public:
    ID3D11Device        *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    uint32_t             width = 0;
    uint32_t             height = 0;
    ID3D11RenderTargetView *target = nullptr;   // the frame the runner reads back
    ID3D11DepthStencilView *depth = nullptr;
    ID3D11Texture2D        *color = nullptr;

    // The layout every mesh vertex shader takes: position, normal, uv, color.
    static const D3D11_INPUT_ELEMENT_DESC mesh_layout[4];

    ID3D11VertexShader   *vertex_shader(const char *name, const char *source, const char *entry = "vs",
                                        ID3D11InputLayout **layout = nullptr,
                                        const D3D11_INPUT_ELEMENT_DESC *elements = mesh_layout, uint32_t count = 4);
    ID3D11PixelShader    *pixel_shader(const char *name, const char *source, const char *entry = "ps");
    ID3D11GeometryShader *geometry_shader(const char *name, const char *source, const char *entry = "gs");
    ID3D11HullShader     *hull_shader(const char *name, const char *source, const char *entry = "hs");
    ID3D11DomainShader   *domain_shader(const char *name, const char *source, const char *entry = "ds");
    ID3D11ComputeShader  *compute_shader(const char *name, const char *source, const char *entry = "cs");
    ID3D11Buffer *constant_buffer(uint32_t bytes);
    ID3D11Buffer *buffer(const void *data, uint32_t bytes, uint32_t bind, uint32_t stride = 0, uint32_t misc = 0);
    DrawMesh      upload(const Mesh &mesh);
    void          update(ID3D11Buffer *buffer, const void *data, uint32_t bytes);
    // A 2D texture from 32-bit RGBA texels, with a full mip chain generated.
    ID3D11ShaderResourceView *texture(const uint32_t *texels, uint32_t width, uint32_t height,
                                      uint32_t layers = 1, bool cube = false);
    ID3D11SamplerState *sampler(D3D11_FILTER filter = D3D11_FILTER_ANISOTROPIC,
                                D3D11_TEXTURE_ADDRESS_MODE address = D3D11_TEXTURE_ADDRESS_WRAP);
    // Binds the frame's target and depth, clears them, sets a full viewport.
    void begin_frame(float r, float g, float b);
    void viewport(float x, float y, float w, float h);
    float aspect() const { return static_cast<float>(width) / static_cast<float>(height); }
    bool failed = false;
};

// ---- samples
class Sample {
public:
    virtual const char *name() const = 0;
    virtual bool        init(Gpu &gpu) = 0;
    // One frame at `time` seconds into the sample's own timeline.
    virtual void        render(Gpu &gpu, float time) = 0;
    virtual void        release() = 0;
};

// Every sample, in the report's order.
Sample **registry(uint32_t &count);

}  // namespace samples

namespace samples {
// ---- platform: the device, and the program's arguments
bool create_device(ID3D11Device **device, ID3D11DeviceContext **context);
// The runner (runner.cpp), given the program's arguments as narrow text.
int  run(int argc, const char *const *argv);
}  // namespace samples

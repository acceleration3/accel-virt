#pragma once

#include <windows.h>
#include <d3d11.h>
#include <stddef.h>
#include <stdint.h>

#include "smath.h"

namespace samples {
using sm::pi; using sm::sqrt; using sm::abs; using sm::floor; using sm::fract; using sm::sin; using sm::cos;
using sm::tan; using sm::radians; using sm::min; using sm::max; using sm::clamp; using sm::mix; using sm::Random;
using sm::float2; using sm::float3; using sm::float4; using sm::float4x4; using sm::dot; using sm::cross;
using sm::length; using sm::normalize; using sm::translate; using sm::scale; using sm::rotate_x; using sm::rotate_y;
using sm::rotate_z; using sm::perspective; using sm::orthographic; using sm::look_at;

struct Blob {
    void  *data = nullptr;
    size_t size = 0;
};
void   log(const char *text);
void   log_number(const char *label, double value);
void  *allocate(size_t bytes);
void   release_memory(void *memory);
double seconds();
bool   write_file(const char *path, const void *data, size_t size);
bool   read_file(const char *path, Blob &blob);
bool   shader(const char *name, const char *source, const char *entry, const char *target, Blob &bytecode);
void   set_shader_directory(const char *directory);

void  copy_text(char *to, size_t capacity, const char *from);
void  append_text(char *to, size_t capacity, const char *from);
void  append_number(char *to, size_t capacity, double value, int decimals);
bool  same_text(const char *a, const char *b);

template <typename T> void safe_release(T *&object)
{
    if (object != nullptr) object->Release();
    object = nullptr;
}

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
    ID3D11RenderTargetView *target = nullptr;
    ID3D11DepthStencilView *depth = nullptr;
    ID3D11Texture2D        *color = nullptr;

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
    ID3D11ShaderResourceView *texture(const uint32_t *texels, uint32_t width, uint32_t height,
                                      uint32_t layers = 1, bool cube = false);
    ID3D11SamplerState *sampler(D3D11_FILTER filter = D3D11_FILTER_ANISOTROPIC,
                                D3D11_TEXTURE_ADDRESS_MODE address = D3D11_TEXTURE_ADDRESS_WRAP);
    void begin_frame(float r, float g, float b);
    void viewport(float x, float y, float w, float h);
    float aspect() const { return static_cast<float>(width) / static_cast<float>(height); }
    bool failed = false;
};

class Sample {
public:
    virtual const char *name() const = 0;
    virtual bool        init(Gpu &gpu) = 0;
    virtual void        render(Gpu &gpu, float time) = 0;
    virtual void        release() = 0;
};

Sample **registry(uint32_t &count);

}

namespace samples {
bool create_device(ID3D11Device **device, ID3D11DeviceContext **context);
int  run(int argc, const char *const *argv);
}

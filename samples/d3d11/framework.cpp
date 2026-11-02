#include "framework.h"

namespace samples {

void copy_text(char *to, size_t capacity, const char *from)
{
    size_t i = 0;
    for (; i + 1 < capacity && from[i] != '\0'; i++) to[i] = from[i];
    if (capacity != 0) to[i] = '\0';
}

void append_text(char *to, size_t capacity, const char *from)
{
    size_t at = 0;
    while (at < capacity && to[at] != '\0') at++;
    if (at < capacity) copy_text(to + at, capacity - at, from);
}

void append_number(char *to, size_t capacity, double value, int decimals)
{
    char digits[48] = {};
    int  n = 0;
    if (value < 0) {
        digits[n++] = '-';
        value = -value;
    }
    double scale = 1.0;
    for (int d = 0; d < decimals; d++) scale *= 10.0;
    unsigned long long whole = static_cast<unsigned long long>(value * scale + 0.5);
    char reversed[32] = {};
    int  r = 0;
    for (int d = 0; d < decimals; d++) {
        reversed[r++] = static_cast<char>('0' + whole % 10u);
        whole /= 10u;
    }
    if (decimals > 0) reversed[r++] = '.';
    do {
        reversed[r++] = static_cast<char>('0' + whole % 10u);
        whole /= 10u;
    } while (whole != 0u && r < 30);
    while (r > 0) digits[n++] = reversed[--r];
    append_text(to, capacity, digits);
}

bool same_text(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

void Mesh::release()
{
    release_memory(vertices);
    release_memory(indices);
    vertices = nullptr;
    indices = nullptr;
}

namespace {

Mesh allocate_mesh(uint32_t vertices, uint32_t indices)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
    return mesh;
}

void push_vertex(Mesh &mesh, float3 p, float3 n, float2 uv, float3 color)
{
    mesh.vertices[mesh.vertex_count++] = Vertex{p, n, uv, color};
}

void push_quad(Mesh &mesh, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    const Vertex *v = mesh.vertices;
    const float3 facing = cross(v[b].position - v[a].position, v[c].position - v[a].position);
    if (dot(facing, v[a].normal + v[b].normal + v[c].normal + v[d].normal) < 0) {
        const uint32_t swap = b;
        b = d;
        d = swap;
    }
    uint32_t *i = mesh.indices + mesh.index_count;
    i[0] = a; i[1] = b; i[2] = c;
    i[3] = a; i[4] = c; i[5] = d;
    mesh.index_count += 6;
}

}

Mesh cube_mesh(float size)
{
    Mesh mesh = allocate_mesh(24, 36);
    const float h = size * 0.5f;
    const float3 normals[6] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const float3 colors[6] = {{1, 0.3f, 0.3f}, {0.3f, 1, 0.3f}, {0.3f, 0.3f, 1}, {1, 1, 0.3f}, {0.3f, 1, 1}, {1, 0.3f, 1}};
    for (int f = 0; f < 6; f++) {
        const float3 n = normals[f];
        const float3 u = abs(n.y) > 0.5f ? float3{1, 0, 0} : normalize(cross({0, 1, 0}, n));
        const float3 v = cross(n, u);
        const uint32_t base = mesh.vertex_count;
        const float2 uvs[4] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
        const float su[4] = {-1, -1, 1, 1}, sv[4] = {-1, 1, 1, -1};
        for (int k = 0; k < 4; k++)
            push_vertex(mesh, (n + u * su[k] + v * sv[k]) * h, n, uvs[k], colors[f]);
        push_quad(mesh, base, base + 1, base + 2, base + 3);
    }
    return mesh;
}

Mesh sphere_mesh(float radius, uint32_t slices, uint32_t stacks)
{
    Mesh mesh = allocate_mesh((slices + 1) * (stacks + 1), slices * stacks * 6);
    for (uint32_t j = 0; j <= stacks; j++) {
        const float v = static_cast<float>(j) / static_cast<float>(stacks);
        const float phi = v * pi;
        for (uint32_t i = 0; i <= slices; i++) {
            const float u = static_cast<float>(i) / static_cast<float>(slices);
            const float theta = u * 2.0f * pi;
            const float3 n = {sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta)};
            push_vertex(mesh, n * radius, n, {u, v}, {0.5f + 0.5f * n.x, 0.5f + 0.5f * n.y, 0.5f + 0.5f * n.z});
        }
    }
    for (uint32_t j = 0; j < stacks; j++)
        for (uint32_t i = 0; i < slices; i++) {
            const uint32_t a = j * (slices + 1) + i, b = a + slices + 1;
            push_quad(mesh, a, a + 1, b + 1, b);
        }
    return mesh;
}

Mesh torus_mesh(float radius, float tube, uint32_t rings, uint32_t sides)
{
    Mesh mesh = allocate_mesh((rings + 1) * (sides + 1), rings * sides * 6);
    for (uint32_t r = 0; r <= rings; r++) {
        const float u = static_cast<float>(r) / static_cast<float>(rings), a = u * 2.0f * pi;
        const float3 center = {cos(a) * radius, 0, sin(a) * radius};
        for (uint32_t s = 0; s <= sides; s++) {
            const float v = static_cast<float>(s) / static_cast<float>(sides), b = v * 2.0f * pi;
            const float3 n = {cos(a) * cos(b), sin(b), sin(a) * cos(b)};
            push_vertex(mesh, center + n * tube, n, {u * 4.0f, v}, {0.8f, 0.6f + 0.4f * v, 0.3f});
        }
    }
    for (uint32_t r = 0; r < rings; r++)
        for (uint32_t s = 0; s < sides; s++) {
            const uint32_t a = r * (sides + 1) + s, b = a + sides + 1;
            push_quad(mesh, a, b, b + 1, a + 1);
        }
    return mesh;
}

Mesh plane_mesh(float size, uint32_t cells, float uv_scale)
{
    Mesh mesh = allocate_mesh((cells + 1) * (cells + 1), cells * cells * 6);
    for (uint32_t j = 0; j <= cells; j++)
        for (uint32_t i = 0; i <= cells; i++) {
            const float u = static_cast<float>(i) / static_cast<float>(cells);
            const float v = static_cast<float>(j) / static_cast<float>(cells);
            push_vertex(mesh, {(u - 0.5f) * size, 0, (v - 0.5f) * size}, {0, 1, 0}, {u * uv_scale, v * uv_scale},
                        {0.7f, 0.7f, 0.7f});
        }
    for (uint32_t j = 0; j < cells; j++)
        for (uint32_t i = 0; i < cells; i++) {
            const uint32_t a = j * (cells + 1) + i, b = a + cells + 1;
            push_quad(mesh, a, b, b + 1, a + 1);
        }
    return mesh;
}

Mesh gear_mesh(float inner, float outer, float width, uint32_t teeth, float tooth_depth, float3 color)
{
    Mesh mesh = allocate_mesh(teeth * 40, teeth * 60);
    const float r0 = inner, r1 = outer - tooth_depth * 0.5f, r2 = outer + tooth_depth * 0.5f;
    const float da = 2.0f * pi / static_cast<float>(teeth) / 4.0f;
    const float hw = width * 0.5f;
    auto quad = [&](float3 a, float3 b, float3 c, float3 d, float3 n) {
        if (dot(cross(b - a, c - a), n) < 0) {
            const float3 swap = b;
            b = d;
            d = swap;
        }
        const uint32_t base = mesh.vertex_count;
        push_vertex(mesh, a, n, {0, 0}, color);
        push_vertex(mesh, b, n, {1, 0}, color);
        push_vertex(mesh, c, n, {1, 1}, color);
        push_vertex(mesh, d, n, {0, 1}, color);
        push_quad(mesh, base, base + 1, base + 2, base + 3);
    };
    for (uint32_t i = 0; i < teeth; i++) {
        const float a = static_cast<float>(i) * 2.0f * pi / static_cast<float>(teeth);
        const float c0 = cos(a), s0 = sin(a), c1 = cos(a + da), s1 = sin(a + da);
        const float c2 = cos(a + 2 * da), s2 = sin(a + 2 * da), c3 = cos(a + 3 * da), s3 = sin(a + 3 * da);
        const float c4 = cos(a + 4 * da), s4 = sin(a + 4 * da);
        for (int side = 0; side < 2; side++) {
            const float z = side == 0 ? hw : -hw;
            const float3 n = {0, 0, side == 0 ? 1.0f : -1.0f};
            quad({r0 * c0, r0 * s0, z}, {r1 * c0, r1 * s0, z}, {r1 * c4, r1 * s4, z}, {r0 * c4, r0 * s4, z}, n);
            quad({r1 * c0, r1 * s0, z}, {r2 * c1, r2 * s1, z}, {r2 * c2, r2 * s2, z}, {r1 * c3, r1 * s3, z}, n);
        }
        const float3 p[5] = {{r1 * c0, r1 * s0, 0}, {r2 * c1, r2 * s1, 0}, {r2 * c2, r2 * s2, 0},
                             {r1 * c3, r1 * s3, 0}, {r1 * c4, r1 * s4, 0}};
        for (int k = 0; k < 4; k++) {
            const float3 e = p[k + 1] - p[k];
            const float3 n = normalize({e.y, -e.x, 0});
            quad({p[k].x, p[k].y, hw}, {p[k].x, p[k].y, -hw}, {p[k + 1].x, p[k + 1].y, -hw},
                 {p[k + 1].x, p[k + 1].y, hw}, n);
        }
        quad({r0 * c4, r0 * s4, hw}, {r0 * c4, r0 * s4, -hw}, {r0 * c0, r0 * s0, -hw}, {r0 * c0, r0 * s0, hw},
             {-c0, -s0, 0});
    }
    return mesh;
}

const D3D11_INPUT_ELEMENT_DESC Gpu::mesh_layout[4] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

void DrawMesh::release()
{
    safe_release(vertices);
    safe_release(indices);
}

void DrawMesh::draw(ID3D11DeviceContext *context, uint32_t instances) const
{
    const UINT stride = sizeof(Vertex), offset = 0;
    context->IASetVertexBuffers(0, 1, &vertices, &stride, &offset);
    context->IASetIndexBuffer(indices, DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (instances == 1) context->DrawIndexed(index_count, 0, 0);
    else context->DrawIndexedInstanced(index_count, instances, 0, 0, 0);
}

ID3D11VertexShader *Gpu::vertex_shader(const char *name, const char *source, const char *entry,
                                       ID3D11InputLayout **layout, const D3D11_INPUT_ELEMENT_DESC *elements,
                                       uint32_t count)
{
    Blob code;
    ID3D11VertexShader *made = nullptr;
    if (!shader(name, source, entry, "vs_5_0", code) ||
        FAILED(device->CreateVertexShader(code.data, code.size, nullptr, &made))) {
        failed = true;
        return nullptr;
    }
    if (layout != nullptr && FAILED(device->CreateInputLayout(elements, count, code.data, code.size, layout)))
        failed = true;
    release_memory(code.data);
    return made;
}

#define SAMPLES_SHADER_STAGE(Method, Type, target, Create)                                       \
    Type *Gpu::Method(const char *name, const char *source, const char *entry)                  \
    {                                                                                            \
        Blob code;                                                                               \
        Type *made = nullptr;                                                                    \
        if (!shader(name, source, entry, target, code) ||                                        \
            FAILED(device->Create(code.data, code.size, nullptr, &made)))                         \
            failed = true;                                                                       \
        release_memory(code.data);                                                               \
        return made;                                                                             \
    }
SAMPLES_SHADER_STAGE(pixel_shader, ID3D11PixelShader, "ps_5_0", CreatePixelShader)
SAMPLES_SHADER_STAGE(geometry_shader, ID3D11GeometryShader, "gs_5_0", CreateGeometryShader)
SAMPLES_SHADER_STAGE(hull_shader, ID3D11HullShader, "hs_5_0", CreateHullShader)
SAMPLES_SHADER_STAGE(domain_shader, ID3D11DomainShader, "ds_5_0", CreateDomainShader)
SAMPLES_SHADER_STAGE(compute_shader, ID3D11ComputeShader, "cs_5_0", CreateComputeShader)

ID3D11Buffer *Gpu::constant_buffer(uint32_t bytes)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = (bytes + 15u) & ~15u;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer *made = nullptr;
    if (FAILED(device->CreateBuffer(&desc, nullptr, &made))) failed = true;
    return made;
}

ID3D11Buffer *Gpu::buffer(const void *data, uint32_t bytes, uint32_t bind, uint32_t stride, uint32_t misc)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = bytes;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bind;
    desc.MiscFlags = misc;
    desc.StructureByteStride = stride;
    D3D11_SUBRESOURCE_DATA initial = {data, 0, 0};
    ID3D11Buffer *made = nullptr;
    if (FAILED(device->CreateBuffer(&desc, data != nullptr ? &initial : nullptr, &made))) failed = true;
    return made;
}

DrawMesh Gpu::upload(const Mesh &mesh)
{
    DrawMesh drawn;
    drawn.vertices = buffer(mesh.vertices, mesh.vertex_count * static_cast<uint32_t>(sizeof(Vertex)),
                            D3D11_BIND_VERTEX_BUFFER);
    drawn.indices = buffer(mesh.indices, mesh.index_count * 4u, D3D11_BIND_INDEX_BUFFER);
    drawn.index_count = mesh.index_count;
    return drawn;
}

void Gpu::update(ID3D11Buffer *target_buffer, const void *data, uint32_t bytes)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(target_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        failed = true;
        return;
    }
    const uint8_t *from = static_cast<const uint8_t *>(data);
    uint8_t       *to = static_cast<uint8_t *>(mapped.pData);
    for (uint32_t i = 0; i < bytes; i++) to[i] = from[i];
    context->Unmap(target_buffer, 0);
}

ID3D11ShaderResourceView *Gpu::texture(const uint32_t *texels, uint32_t w, uint32_t h, uint32_t layers, bool cube)
{
    uint32_t mips = 1;
    while ((w >> mips) > 0 || (h >> mips) > 0) mips++;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = mips;
    desc.ArraySize = layers;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | (cube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0u);
    ID3D11Texture2D *made = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &made))) {
        failed = true;
        return nullptr;
    }
    for (uint32_t layer = 0; layer < layers; layer++)
        context->UpdateSubresource(made, D3D11CalcSubresource(0, layer, mips), nullptr, texels + layer * w * h,
                                   w * 4u, 0);
    ID3D11ShaderResourceView *view = nullptr;
    if (FAILED(device->CreateShaderResourceView(made, nullptr, &view))) failed = true;
    else context->GenerateMips(view);
    made->Release();
    return view;
}

ID3D11SamplerState *Gpu::sampler(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE address)
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = filter;
    desc.AddressU = desc.AddressV = desc.AddressW = address;
    desc.MaxAnisotropy = 8;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState *made = nullptr;
    if (FAILED(device->CreateSamplerState(&desc, &made))) failed = true;
    return made;
}

void Gpu::begin_frame(float r, float g, float b)
{
    const float clear[4] = {r, g, b, 1.0f};
    context->OMSetRenderTargets(1, &target, depth);
    context->ClearRenderTargetView(target, clear);
    context->ClearDepthStencilView(depth, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    viewport(0, 0, static_cast<float>(width), static_cast<float>(height));
}

void Gpu::viewport(float x, float y, float w, float h)
{
    const D3D11_VIEWPORT view = {x, y, w, h, 0.0f, 1.0f};
    context->RSSetViewports(1, &view);
}

}

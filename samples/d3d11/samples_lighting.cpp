#include "framework.h"

namespace samples {

namespace {

uint32_t mix_bits(uint32_t x)
{
    x ^= x >> 16u;
    x *= 0x7FEB352Du;
    x ^= x >> 15u;
    x *= 0x846CA68Bu;
    x ^= x >> 16u;
    return x;
}

float lattice(int x, int y, int z, uint32_t seed)
{
    const uint32_t h = mix_bits(static_cast<uint32_t>(x) * 0x8DA6B343u ^
                                mix_bits(static_cast<uint32_t>(y) * 0xD8163841u ^
                                         mix_bits(static_cast<uint32_t>(z) * 0xCB1AB31Fu ^ seed)));
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

int wrap_index(int v, int period)
{
    if (period <= 0) return v;
    v %= period;
    return v < 0 ? v + period : v;
}

float saturate(float x) { return clamp(x, 0.0f, 1.0f); }
float smoothstep(float a, float b, float x)
{
    const float t = saturate((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
float power(float base, int exponent)
{
    float r = 1.0f;
    for (int i = 0; i < exponent; i++) r *= base;
    return r;
}
float3 mix3(float3 a, float3 b, float t) { return a + (b - a) * t; }
float3 mul3(float3 a, float3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

float value_noise(float x, float y, float z, uint32_t seed, int period)
{
    const float fx = floor(x), fy = floor(y), fz = floor(z);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy), iz = static_cast<int>(fz);
    const float tx = x - fx, ty = y - fy, tz = z - fz;
    const float sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty), sz = tz * tz * (3 - 2 * tz);
    const int x0 = wrap_index(ix, period), x1 = wrap_index(ix + 1, period);
    const int y0 = wrap_index(iy, period), y1 = wrap_index(iy + 1, period);
    float corners[2];
    for (int k = 0; k < 2; k++) {
        const int zz = iz + k;
        const float a = mix(lattice(x0, y0, zz, seed), lattice(x1, y0, zz, seed), sx);
        const float b = mix(lattice(x0, y1, zz, seed), lattice(x1, y1, zz, seed), sx);
        corners[k] = mix(a, b, sy);
    }
    return mix(corners[0], corners[1], sz);
}

float fbm(float x, float y, float z, uint32_t seed, int period, int octaves)
{
    float sum = 0.0f, amplitude = 0.5f, total = 0.0f;
    for (int o = 0; o < octaves; o++) {
        sum += value_noise(x, y, z, seed + static_cast<uint32_t>(o) * 131u, period) * amplitude;
        total += amplitude;
        x *= 2.0f;
        y *= 2.0f;
        z *= 2.0f;
        if (period > 0) period *= 2;
        amplitude *= 0.5f;
    }
    return sum / total;
}

uint32_t rgba(float r, float g, float b, float a = 1.0f)
{
    const uint32_t R = static_cast<uint32_t>(saturate(r) * 255.0f + 0.5f);
    const uint32_t G = static_cast<uint32_t>(saturate(g) * 255.0f + 0.5f);
    const uint32_t B = static_cast<uint32_t>(saturate(b) * 255.0f + 0.5f);
    const uint32_t A = static_cast<uint32_t>(saturate(a) * 255.0f + 0.5f);
    return (A << 24u) | (B << 16u) | (G << 8u) | R;
}

struct StoneField {
    float    *height = nullptr;
    uint32_t *stone = nullptr;
    uint32_t  size = 0;

    void make(uint32_t texels, int cells, uint32_t seed)
    {
        size = texels;
        height = static_cast<float *>(allocate(sizeof(float) * texels * texels));
        stone = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * texels * texels));
        const float per_texel = static_cast<float>(cells) / static_cast<float>(texels);
        for (uint32_t y = 0; y < texels; y++)
            for (uint32_t x = 0; x < texels; x++) {
                const float px = (static_cast<float>(x) + 0.5f) * per_texel;
                const float py = (static_cast<float>(y) + 0.5f) * per_texel;
                const int cx = static_cast<int>(floor(px)), cy = static_cast<int>(floor(py));
                float f1 = 1e9f, f2 = 1e9f;
                uint32_t id = 0;
                for (int j = -1; j <= 1; j++)
                    for (int i = -1; i <= 1; i++) {
                        const int wx = wrap_index(cx + i, cells), wy = wrap_index(cy + j, cells);
                        const float jx = 0.15f + 0.7f * lattice(wx, wy, 0, seed);
                        const float jy = 0.15f + 0.7f * lattice(wx, wy, 1, seed);
                        const float dx = static_cast<float>(cx + i) + jx - px, dy = static_cast<float>(cy + j) + jy - py;
                        const float d = sqrt(dx * dx + dy * dy);
                        if (d < f1) {
                            f2 = f1;
                            f1 = d;
                            id = static_cast<uint32_t>(wy * cells + wx);
                        } else if (d < f2) {
                            f2 = d;
                        }
                    }
                const float edge = saturate((f2 - f1) / 0.55f);
                const float dome = 1.0f - (1.0f - edge) * (1.0f - edge);
                const float grain = fbm(px * 4.0f, py * 4.0f, 0.0f, seed + 7u, cells * 4, 4);
                const float pits = smoothstep(0.62f, 0.75f, fbm(px * 9.0f, py * 9.0f, 3.0f, seed + 3u, cells * 9, 3));
                height[y * texels + x] = saturate(dome * (0.8f + 0.25f * grain) - 0.12f * pits * dome);
                stone[y * texels + x] = id;
            }
    }

    uint32_t *normal_height(float strength) const
    {
        uint32_t *texels = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * size * size));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const uint32_t l = (x + size - 1) % size, r = (x + 1) % size;
                const uint32_t u = (y + size - 1) % size, d = (y + 1) % size;
                const float dx = (height[y * size + r] - height[y * size + l]) * 0.5f * strength;
                const float dy = (height[d * size + x] - height[u * size + x]) * 0.5f * strength;
                const float3 n = normalize({-dx, -dy, 1.0f});
                texels[y * size + x] = rgba(n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, n.z * 0.5f + 0.5f, height[y * size + x]);
            }
        return texels;
    }

    uint32_t *colors(float3 tint, uint32_t seed) const
    {
        uint32_t *texels = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * size * size));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const uint32_t id = stone[y * size + x];
                const float shade = 0.55f + 0.45f * lattice(static_cast<int>(id), 5, 9, seed);
                const float grain = fbm(static_cast<float>(x) * 0.12f, static_cast<float>(y) * 0.12f, 1.0f, seed,
                                        static_cast<int>(size) * 12 / 100, 4);
                const float h = height[y * size + x];
                const float mortar = smoothstep(0.0f, 0.25f, h);
                const float3 stone_color = tint * (shade * (0.6f + 0.7f * grain));
                const float3 c = mix3({0.05f, 0.05f, 0.05f}, stone_color, mortar);
                texels[y * size + x] = rgba(c.x, c.y, c.z, 0.3f + 0.7f * grain);
            }
        return texels;
    }

    void release()
    {
        release_memory(height);
        release_memory(stone);
        height = nullptr;
        stone = nullptr;
    }
};

float3 transform_point(float3 p, const float4x4 &m)
{
    return {p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
            p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
            p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]};
}

void orient(Mesh &mesh)
{
    for (uint32_t t = 0; t + 2 < mesh.index_count; t += 3) {
        uint32_t *i = mesh.indices + t;
        const Vertex &a = mesh.vertices[i[0]], &b = mesh.vertices[i[1]], &c = mesh.vertices[i[2]];
        const float3 n = a.normal + b.normal + c.normal;
        if (dot(cross(b.position - a.position, c.position - a.position), n) < 0.0f) {
            const uint32_t swap = i[1];
            i[1] = i[2];
            i[2] = swap;
        }
    }
}

Mesh new_mesh(uint32_t vertices, uint32_t indices)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
    return mesh;
}

void grid_indices(Mesh &mesh, uint32_t base, uint32_t rows, uint32_t columns)
{
    for (uint32_t r = 0; r < rows; r++)
        for (uint32_t c = 0; c < columns; c++) {
            const uint32_t a = base + r * (columns + 1) + c, b = a + columns + 1;
            uint32_t *i = mesh.indices + mesh.index_count;
            i[0] = a; i[1] = b; i[2] = b + 1;
            i[3] = a; i[4] = b + 1; i[5] = a + 1;
            mesh.index_count += 6;
        }
}

Mesh cylinder_mesh(float radius, float height, uint32_t segments)
{
    Mesh mesh = new_mesh((segments + 1) * 4 + 2, segments * 12);
    const float h = height * 0.5f;
    for (uint32_t k = 0; k < 2; k++)
        for (uint32_t s = 0; s <= segments; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(segments) * 2.0f * pi;
            const float3 n = {cos(a), 0.0f, sin(a)};
            mesh.vertices[mesh.vertex_count++] =
                Vertex{{n.x * radius, k == 0 ? -h : h, n.z * radius}, n,
                       {static_cast<float>(s) / static_cast<float>(segments), static_cast<float>(k)}, {1, 1, 1}};
        }
    grid_indices(mesh, 0, 1, segments);
    for (uint32_t k = 0; k < 2; k++) {
        const float y = k == 0 ? -h : h;
        const float3 n = {0.0f, k == 0 ? -1.0f : 1.0f, 0.0f};
        const uint32_t center = mesh.vertex_count;
        mesh.vertices[mesh.vertex_count++] = Vertex{{0, y, 0}, n, {0.5f, 0.5f}, {1, 1, 1}};
        const uint32_t ring = mesh.vertex_count;
        for (uint32_t s = 0; s <= segments; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(segments) * 2.0f * pi;
            mesh.vertices[mesh.vertex_count++] =
                Vertex{{cos(a) * radius, y, sin(a) * radius}, n, {0.5f + 0.5f * cos(a), 0.5f + 0.5f * sin(a)}, {1, 1, 1}};
        }
        for (uint32_t s = 0; s < segments; s++) {
            uint32_t *i = mesh.indices + mesh.index_count;
            i[0] = center; i[1] = ring + s; i[2] = ring + s + 1;
            mesh.index_count += 3;
        }
    }
    orient(mesh);
    return mesh;
}

Mesh knot_mesh(float scale_by, float tube, uint32_t p, uint32_t q, uint32_t rings, uint32_t sides)
{
    Mesh mesh = new_mesh((rings + 1) * (sides + 1), rings * sides * 6);
    auto curve = [&](float t) {
        const float r = 2.0f + cos(static_cast<float>(q) * t);
        return float3{r * cos(static_cast<float>(p) * t), r * sin(static_cast<float>(p) * t),
                      sin(static_cast<float>(q) * t)} * (scale_by / 3.0f);
    };
    const float e = 0.02f;
    for (uint32_t r = 0; r <= rings; r++) {
        const float t = static_cast<float>(r % rings) / static_cast<float>(rings) * 2.0f * pi;
        const float3 c = curve(t), before = curve(t - e), after = curve(t + e);
        const float3 tangent = normalize(after - before);
        const float3 bend = normalize(after + before - c * 2.0f);
        const float3 binormal = normalize(cross(tangent, bend));
        const float3 normal = cross(binormal, tangent);
        for (uint32_t s = 0; s <= sides; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(sides) * 2.0f * pi;
            const float3 n = normal * cos(a) + binormal * sin(a);
            const float cavity = 0.75f + 0.25f * (0.5f - 0.5f * cos(a));
            mesh.vertices[mesh.vertex_count++] =
                Vertex{c + n * tube, n, {static_cast<float>(r) / static_cast<float>(rings) * 16.0f,
                                         static_cast<float>(s) / static_cast<float>(sides)}, {cavity, cavity, cavity}};
        }
    }
    grid_indices(mesh, 0, rings, sides);
    orient(mesh);
    return mesh;
}

Mesh capsule_mesh(float3 a, float3 b, float r0, float r1, uint32_t body, uint32_t sides, float3 color)
{
    const uint32_t cap = 5, total = cap * 2 + body;
    Mesh mesh = new_mesh((total + 1) * (sides + 1), total * sides * 6);
    const float3 axis = normalize(b - a);
    const float3 reference = abs(axis.y) < 0.9f ? float3{0, 1, 0} : float3{1, 0, 0};
    const float3 u = normalize(cross(reference, axis)), w = cross(axis, u);
    for (uint32_t k = 0; k <= total; k++) {
        float3 center;
        float radius, along;
        if (k <= cap) {
            const float theta = -0.5f * pi + 0.5f * pi * static_cast<float>(k) / static_cast<float>(cap);
            center = a + axis * (r0 * sin(theta));
            radius = r0 * cos(theta);
            along = sin(theta);
        } else if (k >= cap + body) {
            const float theta = 0.5f * pi * static_cast<float>(k - cap - body) / static_cast<float>(cap);
            center = b + axis * (r1 * sin(theta));
            radius = r1 * cos(theta);
            along = sin(theta);
        } else {
            const float t = static_cast<float>(k - cap) / static_cast<float>(body);
            center = a + (b - a) * t;
            radius = mix(r0, r1, t);
            along = 0.0f;
        }
        const float across = sqrt(max(0.0f, 1.0f - along * along));
        for (uint32_t s = 0; s <= sides; s++) {
            const float angle = static_cast<float>(s) / static_cast<float>(sides) * 2.0f * pi;
            const float3 radial = u * cos(angle) + w * sin(angle);
            mesh.vertices[mesh.vertex_count++] =
                Vertex{center + radial * radius, normalize(radial * across + axis * along),
                       {static_cast<float>(s) / static_cast<float>(sides), static_cast<float>(k) / static_cast<float>(total)},
                       color};
        }
    }
    grid_indices(mesh, 0, total, sides);
    orient(mesh);
    return mesh;
}

class MeshBuilder {
public:
    MeshBuilder(uint32_t vertices, uint32_t indices) : vertex_capacity_(vertices), index_capacity_(indices)
    {
        mesh_ = new_mesh(vertices, indices);
    }
    void add(const Mesh &part, const float4x4 &t, float3 color, bool inside = false)
    {
        if (mesh_.vertex_count + part.vertex_count > vertex_capacity_ ||
            mesh_.index_count + part.index_count > index_capacity_) {
            log("lighting: MeshBuilder overflow\n");
            return;
        }
        const float3 r0 = {t.m[0][0], t.m[0][1], t.m[0][2]};
        const float3 r1 = {t.m[1][0], t.m[1][1], t.m[1][2]};
        const float3 r2 = {t.m[2][0], t.m[2][1], t.m[2][2]};
        const float3 c0 = cross(r1, r2), c1 = cross(r2, r0), c2 = cross(r0, r1);
        const float sign = (dot(r0, c0) < 0.0f ? -1.0f : 1.0f) * (inside ? -1.0f : 1.0f);
        const uint32_t base = mesh_.vertex_count;
        for (uint32_t i = 0; i < part.vertex_count; i++) {
            Vertex v = part.vertices[i];
            v.position = transform_point(v.position, t);
            v.normal = normalize((c0 * v.normal.x + c1 * v.normal.y + c2 * v.normal.z) * sign);
            v.color = color;
            mesh_.vertices[mesh_.vertex_count++] = v;
        }
        for (uint32_t i = 0; i < part.index_count; i++) {
            mesh_.indices[mesh_.index_count + i] = base + part.indices[i];
        }
        Mesh added;
        added.vertices = mesh_.vertices;
        added.indices = mesh_.indices + mesh_.index_count;
        added.index_count = part.index_count;
        orient(added);
        mesh_.index_count += part.index_count;
    }
    void take(Mesh part, const float4x4 &t, float3 color, bool inside = false)
    {
        add(part, t, color, inside);
        part.release();
    }
    Mesh &mesh() { return mesh_; }

private:
    Mesh     mesh_;
    uint32_t vertex_capacity_;
    uint32_t index_capacity_;
};

const float3 white = {1, 1, 1};

ID3D11RasterizerState *make_rasterizer(Gpu &gpu, D3D11_CULL_MODE cull, int bias = 0, float slope = 0.0f)
{
    D3D11_RASTERIZER_DESC desc = {};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = cull;
    desc.DepthBias = bias;
    desc.SlopeScaledDepthBias = slope;
    desc.DepthClipEnable = TRUE;
    ID3D11RasterizerState *made = nullptr;
    if (FAILED(gpu.device->CreateRasterizerState(&desc, &made))) gpu.failed = true;
    return made;
}

ID3D11DepthStencilState *make_depth_state(Gpu &gpu, bool test, bool write)
{
    D3D11_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = test ? TRUE : FALSE;
    desc.DepthWriteMask = write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ID3D11DepthStencilState *made = nullptr;
    if (FAILED(gpu.device->CreateDepthStencilState(&desc, &made))) gpu.failed = true;
    return made;
}

ID3D11SamplerState *make_compare_sampler(Gpu &gpu)
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
    desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState *made = nullptr;
    if (FAILED(gpu.device->CreateSamplerState(&desc, &made))) gpu.failed = true;
    return made;
}

struct DepthMap {
    ID3D11DepthStencilView   *view = nullptr;
    ID3D11ShaderResourceView *shader_view = nullptr;

    void create(Gpu &gpu, uint32_t w, uint32_t h, uint32_t layers, bool readable)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = layers;
        desc.Format = readable ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | (readable ? D3D11_BIND_SHADER_RESOURCE : 0u);
        ID3D11Texture2D *texture = nullptr;
        if (FAILED(gpu.device->CreateTexture2D(&desc, nullptr, &texture))) {
            gpu.failed = true;
            return;
        }
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format = readable ? DXGI_FORMAT_D16_UNORM : DXGI_FORMAT_D32_FLOAT;
        if (layers > 1) {
            dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsv.Texture2DArray.ArraySize = layers;
        } else {
            dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        }
        if (FAILED(gpu.device->CreateDepthStencilView(texture, &dsv, &view))) gpu.failed = true;
        if (readable) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
            srv.Format = DXGI_FORMAT_R16_UNORM;
            srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv.Texture2D.MipLevels = 1;
            if (FAILED(gpu.device->CreateShaderResourceView(texture, &srv, &shader_view))) gpu.failed = true;
        }
        texture->Release();
    }
    void release()
    {
        safe_release(view);
        safe_release(shader_view);
    }
};

struct ColorMap {
    ID3D11RenderTargetView   *view = nullptr;
    ID3D11ShaderResourceView *shader_view = nullptr;

    void create(Gpu &gpu, uint32_t w, uint32_t h, DXGI_FORMAT format, uint32_t layers = 1, bool cube = false)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = layers;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = cube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0u;
        ID3D11Texture2D *texture = nullptr;
        if (FAILED(gpu.device->CreateTexture2D(&desc, nullptr, &texture))) {
            gpu.failed = true;
            return;
        }
        D3D11_RENDER_TARGET_VIEW_DESC rtv = {};
        rtv.Format = format;
        if (layers > 1) {
            rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtv.Texture2DArray.ArraySize = layers;
        } else {
            rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        }
        if (FAILED(gpu.device->CreateRenderTargetView(texture, &rtv, &view))) gpu.failed = true;
        if (FAILED(gpu.device->CreateShaderResourceView(texture, nullptr, &shader_view))) gpu.failed = true;
        texture->Release();
    }
    void release()
    {
        safe_release(view);
        safe_release(shader_view);
    }
};

void unbind_pixel_resources(ID3D11DeviceContext *c, uint32_t count)
{
    ID3D11ShaderResourceView *none[4] = {};
    c->PSSetShaderResources(0, count, none);
}

void draw_fullscreen(ID3D11DeviceContext *c)
{
    c->IASetInputLayout(nullptr);
    c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    c->Draw(3, 0);
}

const char *const shadowmapping_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 view_projection;
    row_major float4x4 light_view_projection;
    float4 light_position;
    float4 shadow_params;   // x: one shadow texel, y/z: the light's near and far planes
};
Texture2D<float> shadow_map : register(t0);
SamplerComparisonState shadow_compare : register(s0);
SamplerState clamp_sampler : register(s1);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float3 world : WORLD; float4 shadow : SHADOW; };
float4 depth_vs(In v) : SV_Position { return mul(float4(v.position, 1), light_view_projection); }
Out vs(In v) {
    Out o;
    o.position = mul(float4(v.position, 1), view_projection);
    o.normal = v.normal;
    o.color = v.color;
    o.world = v.position;
    o.shadow = mul(float4(v.position, 1), light_view_projection);
    return o;
}
float lit_fraction(float4 s) {
    if (s.w <= 0) return 1;
    float3 c = s.xyz / s.w;
    if (c.z <= 0 || c.z >= 1) return 1;
    float2 uv = c.xy * float2(0.5, -0.5) + 0.5;
    float sum = 0;
    [unroll] for (int y = -1; y <= 1; y++)
        [unroll] for (int x = -1; x <= 1; x++)
            sum += shadow_map.SampleCmpLevelZero(shadow_compare, uv + float2(x, y) * shadow_params.x * 1.5, c.z);
    return sum / 9;
}
float4 ps(Out p) : SV_Target {
    float3 n = normalize(p.normal);
    float3 l = normalize(light_position.xyz - p.world);
    float shade = lerp(0.1, 1.0, lit_fraction(p.shadow));
    return float4(max(dot(n, l), 0.1) * p.color * shade, 1);
}
struct Quad { float4 position : SV_Position; float2 uv : TEXCOORD; };
Quad quad_vs(uint id : SV_VertexID) {
    Quad o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float4 quad_ps(Quad q) : SV_Target {
    float d = shadow_map.SampleLevel(clamp_sampler, q.uv, 0);
    float zn = shadow_params.y, zf = shadow_params.z;
    float z = zn * zf / (zf - d * (zf - zn));
    float v = 1 - saturate((z - zn) / (zf - zn));
    return float4(v, v, v, 1);
}
)";

struct ShadowFrame {
    float4x4 view_projection;
    float4x4 light_view_projection;
    float4   light_position;
    float4   shadow_params;
};

class ShadowMapping final : public Sample {
public:
    const char *name() const override { return "shadowmapping"; }
    bool init(Gpu &gpu) override
    {
        vs_ = gpu.vertex_shader("shadowmapping_vs", shadowmapping_source, "vs", &layout_);
        ps_ = gpu.pixel_shader("shadowmapping_ps", shadowmapping_source);
        depth_vs_ = gpu.vertex_shader("shadowmapping_depth_vs", shadowmapping_source, "depth_vs");
        quad_vs_ = gpu.vertex_shader("shadowmapping_quad_vs", shadowmapping_source, "quad_vs");
        quad_ps_ = gpu.pixel_shader("shadowmapping_quad_ps", shadowmapping_source, "quad_ps");
        MeshBuilder scene(40000, 200000);
        const float3 grey = {0.62f, 0.62f, 0.62f};
        Mesh cube = cube_mesh(1.0f);
        scene.add(cube, scale({16.0f, 0.6f, 12.0f}) * translate({0, -0.3f, 0}), grey);
        scene.take(knot_mesh(2.1f, 0.42f, 2, 3, 240, 20), rotate_x(0.5f * pi) * translate({0.4f, 2.7f, 1.4f}), grey);
        scene.take(sphere_mesh(1.0f, 40, 20), scale({1.1f, 0.9f, 1.1f}) * translate({0.3f, 0.9f, -2.2f}), grey);
        scene.take(cylinder_mesh(0.35f, 3.6f, 24), rotate_z(0.35f) * translate({3.6f, 1.6f, -0.6f}), grey);
        scene.add(cube, scale({1.2f, 1.2f, 1.9f}) * rotate_y(0.4f) * translate({4.3f, 3.4f, -0.4f}), grey);
        scene.take(torus_mesh(0.9f, 0.35f, 48, 20), rotate_x(0.3f) * translate({-3.6f, 1.1f, -1.0f}), grey);
        scene.take(sphere_mesh(0.8f, 32, 16), translate({-3.2f, 0.8f, 2.4f}), grey);
        scene.add(cube, scale({1.3f, 2.6f, 1.3f}) * rotate_y(0.7f) * translate({-0.3f, 1.3f, 5.0f}), grey);
        cube.release();
        mesh_ = gpu.upload(scene.mesh());
        scene.mesh().release();
        shadow_.create(gpu, shadow_size, shadow_size, 1, true);
        depth_bias_ = make_rasterizer(gpu, D3D11_CULL_BACK, 2, 1.75f);
        compare_ = make_compare_sampler(gpu);
        clamp_ = gpu.sampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
        constants_ = gpu.constant_buffer(sizeof(ShadowFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *c = gpu.context;
        const float a = time * 0.8f;
        const float3 light = {cos(a) * 11.0f, 20.0f + sin(a) * 4.0f, -9.0f + sin(a) * 3.0f};
        ShadowFrame frame;
        frame.light_view_projection = look_at(light, {0, 0, 0}, {0, 1, 0}) * perspective(radians(55), 1.0f, z_near, z_far);
        frame.view_projection =
            look_at({8.5f, 8.0f, -12.0f}, {0.5f, 1.2f, 0.8f}, {0, 1, 0}) * perspective(radians(45), gpu.aspect(), 0.5f, 100);
        frame.light_position = {light.x, light.y, light.z, 1};
        frame.shadow_params = {1.0f / static_cast<float>(shadow_size), z_near, z_far, 0};
        gpu.update(constants_, &frame, sizeof frame);

        unbind_pixel_resources(c, 1);
        c->OMSetRenderTargets(0, nullptr, shadow_.view);
        c->ClearDepthStencilView(shadow_.view, D3D11_CLEAR_DEPTH, 1.0f, 0);
        gpu.viewport(0, 0, static_cast<float>(shadow_size), static_cast<float>(shadow_size));
        c->RSSetState(depth_bias_);
        c->IASetInputLayout(layout_);
        c->VSSetShader(depth_vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShader(nullptr, nullptr, 0);
        mesh_.draw(c);

        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        c->RSSetState(nullptr);
        c->VSSetShader(vs_, nullptr, 0);
        c->PSSetShader(ps_, nullptr, 0);
        c->PSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShaderResources(0, 1, &shadow_.shader_view);
        ID3D11SamplerState *samplers[2] = {compare_, clamp_};
        c->PSSetSamplers(0, 2, samplers);
        mesh_.draw(c);

        const float inset = static_cast<float>(gpu.height) * 0.36f;
        gpu.viewport(12.0f, 12.0f, inset, inset);
        c->VSSetShader(quad_vs_, nullptr, 0);
        c->PSSetShader(quad_ps_, nullptr, 0);
        draw_fullscreen(c);
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(ps_);
        safe_release(depth_vs_);
        safe_release(quad_vs_);
        safe_release(quad_ps_);
        safe_release(layout_);
        mesh_.release();
        shadow_.release();
        safe_release(depth_bias_);
        safe_release(compare_);
        safe_release(clamp_);
        safe_release(constants_);
    }

private:
    static constexpr uint32_t shadow_size = 2048;
    static constexpr float    z_near = 1.0f;
    static constexpr float    z_far = 60.0f;
    ID3D11VertexShader    *vs_ = nullptr;
    ID3D11PixelShader     *ps_ = nullptr;
    ID3D11VertexShader    *depth_vs_ = nullptr;
    ID3D11VertexShader    *quad_vs_ = nullptr;
    ID3D11PixelShader     *quad_ps_ = nullptr;
    ID3D11InputLayout     *layout_ = nullptr;
    DrawMesh               mesh_;
    DepthMap               shadow_;
    ID3D11RasterizerState *depth_bias_ = nullptr;
    ID3D11SamplerState    *compare_ = nullptr;
    ID3D11SamplerState    *clamp_ = nullptr;
    ID3D11Buffer          *constants_ = nullptr;
};

const char *const shadowmappingomni_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 view_projection;
    row_major float4x4 faces[6];
    float4 light_position;
};
TextureCube<float> distances : register(t0);
SamplerState point_sampler : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float3 world : WORLD; };
Out vs(In v) {
    Out o;
    o.position = mul(float4(v.position, 1), view_projection);
    o.normal = v.normal;
    o.color = v.color;
    o.world = v.position;
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 to_light = light_position.xyz - p.world;
    float dist = length(to_light);
    float3 n = normalize(p.normal);
    float diffuse = max(dot(n, to_light / dist), 0);
    float attenuation = 1.6 / (1 + 0.006 * dist * dist);
    float stored = distances.SampleLevel(point_sampler, -to_light, 0);
    float shadow = dist <= stored + 0.15 ? 1 : 0.3;
    return float4((0.05 + diffuse * attenuation) * p.color * shadow, 1);
}
cbuffer Face : register(b1) { uint face; };
struct Face { float4 position : SV_Position; float3 world : WORLD; };
Face depth_vs(In v) {
    Face o;
    o.position = mul(float4(v.position, 1), faces[face]);
    o.world = v.position;
    return o;
}
float distance_ps(Face p) : SV_Target { return length(p.world - light_position.xyz); }
)";

struct OmniFrame {
    float4x4 view_projection;
    float4x4 faces[6];
    float4   light_position;
};

class ShadowMappingOmni final : public Sample {
public:
    const char *name() const override { return "shadowmappingomni"; }
    bool init(Gpu &gpu) override
    {
        vs_ = gpu.vertex_shader("shadowmappingomni_vs", shadowmappingomni_source, "vs", &layout_);
        ps_ = gpu.pixel_shader("shadowmappingomni_ps", shadowmappingomni_source);
        depth_vs_ = gpu.vertex_shader("shadowmappingomni_depth_vs", shadowmappingomni_source, "depth_vs");
        distance_ps_ = gpu.pixel_shader("shadowmappingomni_distance_ps", shadowmappingomni_source, "distance_ps");
        MeshBuilder scene(20000, 100000);
        Mesh cube = cube_mesh(1.0f);
        scene.add(cube, scale({24.0f, 15.0f, 24.0f}) * translate({0, 7.5f, 0}), {0.85f, 0.8f, 0.7f}, true);
        const float3 iron = {0.2f, 0.2f, 0.2f};
        const uint32_t bars = 14;
        for (uint32_t i = 0; i < bars; i++) {
            const float a = static_cast<float>(i) / static_cast<float>(bars) * 2.0f * pi;
            scene.add(cube, scale({0.16f, 5.2f, 0.16f}) * translate({cos(a) * 1.5f, 2.6f, sin(a) * 1.5f}), iron);
        }
        Mesh ring = torus_mesh(1.5f, 0.1f, 64, 10);
        const float heights[4] = {0.15f, 1.6f, 3.9f, 5.2f};
        for (float y : heights) scene.add(ring, translate({0, y, 0}), iron);
        ring.release();
        scene.take(cylinder_mesh(0.6f, 0.3f, 32), translate({0, 0.15f, 0}), iron);
        cube.release();
        mesh_ = gpu.upload(scene.mesh());
        scene.mesh().release();
        distances_.create(gpu, cube_size, cube_size, DXGI_FORMAT_R32_FLOAT, 6, true);
        depth_.create(gpu, cube_size, cube_size, 1, false);
        for (uint32_t f = 0; f < 6; f++) {
            const uint32_t index[4] = {f, 0, 0, 0};
            face_constants_[f] = gpu.buffer(index, sizeof index, D3D11_BIND_CONSTANT_BUFFER);
            face_views_[f] = face_view(gpu, f);
        }
        no_cull_ = make_rasterizer(gpu, D3D11_CULL_NONE);
        point_ = gpu.sampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
        constants_ = gpu.constant_buffer(sizeof(OmniFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *c = gpu.context;
        const float a = time * 1.2f;
        const float3 light = {sin(a) * 0.35f, 2.8f, cos(a) * 0.35f};
        OmniFrame frame;
        frame.view_projection =
            look_at({-17.0f, 19.0f, -17.0f}, {0, 3.5f, 0}, {0, 1, 0}) * perspective(radians(50), gpu.aspect(), 0.5f, 200);
        const float3 directions[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        const float3 ups[6] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
        const float4x4 projection = perspective(0.5f * pi, 1.0f, 0.1f, 64.0f);
        for (int f = 0; f < 6; f++) frame.faces[f] = look_at(light, light + directions[f], ups[f]) * projection;
        frame.light_position = {light.x, light.y, light.z, 1};
        gpu.update(constants_, &frame, sizeof frame);

        unbind_pixel_resources(c, 1);
        const float far_away[4] = {1000.0f, 1000.0f, 1000.0f, 1000.0f};
        c->ClearRenderTargetView(distances_.view, far_away);
        gpu.viewport(0, 0, static_cast<float>(cube_size), static_cast<float>(cube_size));
        c->RSSetState(no_cull_);
        c->IASetInputLayout(layout_);
        c->VSSetShader(depth_vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShader(distance_ps_, nullptr, 0);
        c->PSSetConstantBuffers(0, 1, &constants_);
        for (uint32_t f = 0; f < 6; f++) {
            c->OMSetRenderTargets(1, &face_views_[f], depth_.view);
            c->ClearDepthStencilView(depth_.view, D3D11_CLEAR_DEPTH, 1.0f, 0);
            c->VSSetConstantBuffers(1, 1, &face_constants_[f]);
            mesh_.draw(c);
        }

        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        c->RSSetState(nullptr);
        c->VSSetShader(vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShader(ps_, nullptr, 0);
        c->PSSetShaderResources(0, 1, &distances_.shader_view);
        c->PSSetSamplers(0, 1, &point_);
        mesh_.draw(c);
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(ps_);
        safe_release(depth_vs_);
        safe_release(distance_ps_);
        for (uint32_t f = 0; f < 6; f++) {
            safe_release(face_constants_[f]);
            safe_release(face_views_[f]);
        }
        safe_release(layout_);
        mesh_.release();
        distances_.release();
        depth_.release();
        safe_release(no_cull_);
        safe_release(point_);
        safe_release(constants_);
    }

private:
    static constexpr uint32_t cube_size = 1024;

    ID3D11RenderTargetView *face_view(Gpu &gpu, uint32_t face)
    {
        ID3D11Resource *texture = nullptr;
        if (distances_.view != nullptr) distances_.view->GetResource(&texture);
        D3D11_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.FirstArraySlice = face;
        desc.Texture2DArray.ArraySize = 1;
        ID3D11RenderTargetView *view = nullptr;
        if (texture == nullptr || FAILED(gpu.device->CreateRenderTargetView(texture, &desc, &view))) gpu.failed = true;
        safe_release(texture);
        return view;
    }

    ID3D11Buffer           *face_constants_[6] = {};
    ID3D11RenderTargetView *face_views_[6] = {};
    ID3D11VertexShader    *vs_ = nullptr;
    ID3D11PixelShader     *ps_ = nullptr;
    ID3D11VertexShader    *depth_vs_ = nullptr;
    ID3D11PixelShader     *distance_ps_ = nullptr;
    ID3D11InputLayout     *layout_ = nullptr;
    DrawMesh               mesh_;
    ColorMap               distances_;
    DepthMap               depth_;
    ID3D11RasterizerState *no_cull_ = nullptr;
    ID3D11SamplerState    *point_ = nullptr;
    ID3D11Buffer          *constants_ = nullptr;
};

void build_knight(MeshBuilder &b)
{
    const float3 steel = {0.85f, 0.85f, 0.88f}, dark = {0.45f, 0.45f, 0.48f}, gold = {1.0f, 0.55f, 0.15f};
    Mesh cube = cube_mesh(1.0f);
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        b.take(capsule_mesh({0.25f * s, 1.55f, 0}, {0.3f * s, 0.85f, 0}, 0.2f, 0.16f, 4, 16, white), float4x4::identity(), steel);
        b.take(capsule_mesh({0.3f * s, 0.85f, 0}, {0.32f * s, 0.2f, 0}, 0.16f, 0.15f, 4, 16, white), float4x4::identity(), steel);
        b.add(cube, scale({0.3f, 0.2f, 0.5f}) * translate({0.32f * s, 0.1f, -0.08f}), dark);
        b.take(sphere_mesh(0.27f, 20, 10), scale({1.2f, 0.8f, 1.1f}) * translate({0.6f * s, 2.9f, 0}), steel);
        b.take(capsule_mesh({0.62f * s, 2.8f, 0}, {1.15f * s, 2.55f, -0.1f}, 0.14f, 0.12f, 4, 14, white), float4x4::identity(), steel);
        b.take(capsule_mesh({1.15f * s, 2.55f, -0.1f}, {1.65f * s, 2.35f, -0.2f}, 0.12f, 0.1f, 4, 14, white), float4x4::identity(), steel);
        b.take(sphere_mesh(0.12f, 12, 8), translate({1.75f * s, 2.3f, -0.22f}), dark);
    }
    b.add(cube, scale({0.8f, 0.4f, 0.5f}) * translate({0, 1.55f, 0}), dark);
    b.take(capsule_mesh({0, 1.8f, 0}, {0, 2.7f, 0}, 0.38f, 0.46f, 6, 20, white), scale({1.0f, 1.0f, 0.75f}), steel);
    b.take(sphere_mesh(0.1f, 14, 8), translate({0, 2.35f, -0.36f}), gold);
    b.take(sphere_mesh(0.28f, 24, 12), scale({1.0f, 1.2f, 1.05f}) * translate({0, 3.3f, 0}), steel);
    b.add(cube, scale({0.05f, 0.25f, 0.55f}) * translate({0, 3.6f, 0}), gold);
    const float4x4 sword = rotate_z(0.9f) * translate({-1.75f, 2.3f, -0.22f});
    b.add(cube, scale({0.08f, 1.6f, 0.03f}) * translate({0, 0.95f, 0}) * sword, {1.0f, 1.0f, 1.0f});
    b.add(cube, scale({0.45f, 0.07f, 0.08f}) * translate({0, 0.15f, 0}) * sword, gold);
    b.take(cylinder_mesh(0.7f, 0.1f, 6), scale({1.0f, 1.0f, 1.3f}) * rotate_x(0.5f * pi) * rotate_y(-0.2f) *
                                              translate({1.95f, 2.2f, -0.4f}), dark);
    b.take(sphere_mesh(0.14f, 12, 8), scale({1, 1, 0.5f}) * translate({1.92f, 2.2f, -0.49f}), gold);
    cube.release();
}

const char *const deferred_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 view_projection;
    float4 view_position;
    float4 light_positions[5];   // w: radius
    float4 light_colors[5];
};
Texture2D albedo_map : register(t0);
Texture2D gbuffer_position : register(t1);
Texture2D gbuffer_normal : register(t2);
Texture2D gbuffer_albedo : register(t3);
SamplerState wrap_sampler : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 world : WORLD; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
Out vs(In v) {
    Out o;
    o.position = mul(float4(v.position, 1), view_projection);
    o.world = v.position;
    o.normal = v.normal;
    o.uv = v.uv;
    o.color = v.color;
    return o;
}
struct Targets { float4 position : SV_Target0; float4 normal : SV_Target1; float4 albedo : SV_Target2; };
Targets gbuffer_ps(Out p) {
    Targets t;
    float4 a = albedo_map.Sample(wrap_sampler, p.uv * float2(2, 4));
    t.position = float4(p.world, 1);
    t.normal = float4(normalize(p.normal), 1);
    t.albedo = float4(a.rgb * p.color, a.a);
    return t;
}
float4 quad_vs(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float3 shade(float3 position, float3 normal, float4 albedo) {
    float3 color = albedo.rgb * 0.05;
    float3 view = normalize(view_position.xyz - position);
    [unroll] for (int i = 0; i < 5; i++) {
        float3 to_light = light_positions[i].xyz - position;
        float dist = length(to_light);
        if (dist < light_positions[i].w) {
            float3 l = to_light / dist;
            float3 diffuse = max(dot(normal, l), 0) * albedo.rgb * light_colors[i].rgb;
            float3 h = normalize(l + view);
            float3 specular = light_colors[i].rgb * pow(max(dot(normal, h), 0), 16) * albedo.a * 0.6;
            color += (diffuse + specular) / (1 + 0.3 * dist + 0.1 * dist * dist);
        }
    }
    return color;
}
// The four quadrants show the G-buffer's position, normal and albedo, and the lit composition.
float4 compose_ps(float4 pixel : SV_Position) : SV_Target {
    uint2 size;
    gbuffer_position.GetDimensions(size.x, size.y);
    uint2 half_size = size / 2;
    uint2 at = uint2(pixel.xy);
    uint2 quadrant = uint2(at.x >= half_size.x, at.y >= half_size.y);
    int3 texel = int3((at - quadrant * half_size) * 2, 0);
    float4 position = gbuffer_position.Load(texel);
    float4 normal = gbuffer_normal.Load(texel);
    float4 albedo = gbuffer_albedo.Load(texel);
    if (normal.w == 0) return float4(0, 0, 0, 1);
    if (quadrant.y == 0)
        return quadrant.x == 0 ? float4(saturate(position.xyz * float3(0.45, 0.28, -0.8) + float3(0.5, 0, 0.5)), 1)
                               : float4(saturate(normal.xyz * float3(1, 1, -1)), 1);
    if (quadrant.x == 0) return float4(albedo.rgb, 1);
    return float4(shade(position.xyz, normalize(normal.xyz), albedo), 1);
}
)";

struct DeferredFrame {
    float4x4 view_projection;
    float4   view_position;
    float4   light_positions[5];
    float4   light_colors[5];
};

class Deferred final : public Sample {
public:
    const char *name() const override { return "deferred"; }
    bool init(Gpu &gpu) override
    {
        vs_ = gpu.vertex_shader("deferred_vs", deferred_source, "vs", &layout_);
        gbuffer_ps_ = gpu.pixel_shader("deferred_gbuffer_ps", deferred_source, "gbuffer_ps");
        quad_vs_ = gpu.vertex_shader("deferred_quad_vs", deferred_source, "quad_vs");
        compose_ps_ = gpu.pixel_shader("deferred_compose_ps", deferred_source, "compose_ps");
        MeshBuilder knight(40000, 200000);
        build_knight(knight);
        mesh_ = gpu.upload(knight.mesh());
        knight.mesh().release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float fx = static_cast<float>(x) / static_cast<float>(size), fy = static_cast<float>(y) / static_cast<float>(size);
                const float grain = fbm(fx * 16.0f, fy * 16.0f, 0.0f, 11u, 16, 5);
                const float scratch = smoothstep(0.7f, 0.9f, fbm(fx * 64.0f, fy * 4.0f, 1.0f, 12u, 64, 2));
                const float v = 0.55f + 0.45f * grain - 0.15f * scratch;
                texels[y * size + x] = rgba(v, v, v * 0.97f, 0.4f + 0.6f * grain);
            }
        albedo_ = gpu.texture(texels, size, size);
        release_memory(texels);
        const DXGI_FORMAT formats[3] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                                        DXGI_FORMAT_R8G8B8A8_UNORM};
        for (int i = 0; i < 3; i++) gbuffer_[i].create(gpu, gpu.width, gpu.height, formats[i]);
        sampler_ = gpu.sampler();
        constants_ = gpu.constant_buffer(sizeof(DeferredFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *c = gpu.context;
        DeferredFrame frame;
        const float3 eye = {0.0f, 2.0f, -4.9f};
        frame.view_projection = look_at(eye, {0, 1.9f, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 64);
        frame.view_position = {eye.x, eye.y, eye.z, 1};
        const float3 colors[5] = {{0.9f, 0.9f, 0.9f}, {1.8f, 0.2f, 0.1f}, {0.2f, 0.4f, 2.0f}, {0.2f, 1.6f, 0.3f}, {1.8f, 1.2f, 0.1f}};
        for (int i = 0; i < 5; i++) {
            const float a = time * (0.6f + 0.15f * static_cast<float>(i)) + static_cast<float>(i) * 1.3f;
            const float radius = i == 0 ? 1.2f : 2.4f;
            frame.light_positions[i] = {sin(a) * radius, 1.2f + static_cast<float>(i) * 0.5f, -1.2f + cos(a) * radius, 8.0f};
            frame.light_colors[i] = {colors[i].x, colors[i].y, colors[i].z, 1};
        }
        gpu.update(constants_, &frame, sizeof frame);

        unbind_pixel_resources(c, 4);
        ID3D11RenderTargetView *targets[3] = {gbuffer_[0].view, gbuffer_[1].view, gbuffer_[2].view};
        const float clear[4] = {0, 0, 0, 0};
        for (auto *t : targets) c->ClearRenderTargetView(t, clear);
        c->ClearDepthStencilView(gpu.depth, D3D11_CLEAR_DEPTH, 1.0f, 0);
        c->OMSetRenderTargets(3, targets, gpu.depth);
        gpu.viewport(0, 0, static_cast<float>(gpu.width), static_cast<float>(gpu.height));
        c->IASetInputLayout(layout_);
        c->VSSetShader(vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShader(gbuffer_ps_, nullptr, 0);
        c->PSSetShaderResources(0, 1, &albedo_);
        c->PSSetSamplers(0, 1, &sampler_);
        mesh_.draw(c);

        const float black[4] = {0, 0, 0, 1};
        c->OMSetRenderTargets(1, &gpu.target, nullptr);
        c->ClearRenderTargetView(gpu.target, black);
        ID3D11ShaderResourceView *views[3] = {gbuffer_[0].shader_view, gbuffer_[1].shader_view, gbuffer_[2].shader_view};
        c->PSSetShaderResources(1, 3, views);
        c->PSSetConstantBuffers(0, 1, &constants_);
        c->VSSetShader(quad_vs_, nullptr, 0);
        c->PSSetShader(compose_ps_, nullptr, 0);
        draw_fullscreen(c);
        unbind_pixel_resources(c, 4);
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(gbuffer_ps_);
        safe_release(quad_vs_);
        safe_release(compose_ps_);
        safe_release(layout_);
        mesh_.release();
        safe_release(albedo_);
        for (ColorMap &target : gbuffer_) target.release();
        safe_release(sampler_);
        safe_release(constants_);
    }

private:
    ID3D11VertexShader       *vs_ = nullptr;
    ID3D11PixelShader        *gbuffer_ps_ = nullptr;
    ID3D11VertexShader       *quad_vs_ = nullptr;
    ID3D11PixelShader        *compose_ps_ = nullptr;
    ID3D11InputLayout        *layout_ = nullptr;
    DrawMesh                  mesh_;
    ID3D11ShaderResourceView *albedo_ = nullptr;
    ColorMap                  gbuffer_[3];
    ID3D11SamplerState       *sampler_ = nullptr;
    ID3D11Buffer             *constants_ = nullptr;
};

const char *const parallaxmapping_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 view_projection;
    float4 eye;
    float4 light;    // w: radius
    float4 params;   // x: offset scale, y: offset bias, z: occlusion depth
};
Texture2D color_map : register(t0);
Texture2D normal_height_map : register(t1);
SamplerState wrap_sampler : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 world : WORLD; float2 uv : TEXCOORD; };
Out vs(In v) {
    Out o;
    o.position = mul(float4(v.position, 1), view_projection);
    o.world = v.position;
    o.uv = v.uv;
    return o;
}
// The floor's tangent frame: u runs along world x, v along world z, the normal is world y.
float3 to_tangent(float3 w) { return float3(w.x, w.z, w.y); }
float3 shade(float2 uv, float3 world, float2 dx, float2 dy) {
    float3 n = normalize(normal_height_map.SampleGrad(wrap_sampler, uv, dx, dy).rgb * 2 - 1);
    float3 rgb = color_map.SampleGrad(wrap_sampler, uv, dx, dy).rgb;
    float3 to_light = to_tangent(light.xyz - world);
    float dist = length(to_light);
    float3 l = to_light / dist;
    float3 v = normalize(to_tangent(eye.xyz - world));
    float diffuse = saturate(dot(n, l));
    float specular = pow(saturate(dot(reflect(-l, n), v)), 12) * 0.25;
    float attenuation = saturate(1 - dist / light.w);
    return (rgb * 0.15 + rgb * diffuse * 1.5 + specular) * attenuation;
}
float4 offset_ps(Out p) : SV_Target {
    float3 v = normalize(to_tangent(eye.xyz - p.world));
    float2 dx = ddx(p.uv), dy = ddy(p.uv);
    float h = normal_height_map.Sample(wrap_sampler, p.uv).a * params.x + params.y;
    return float4(shade(p.uv + h * v.xy, p.world, dx, dy), 1);
}
float4 occlusion_ps(Out p) : SV_Target {
    float3 v = normalize(to_tangent(eye.xyz - p.world));
    float2 dx = ddx(p.uv), dy = ddy(p.uv);
    const int steps = 48;
    float layer = 1.0 / steps;
    float2 delta = v.xy / max(v.z, 0.15) * params.z / steps;
    float2 uv = p.uv;
    float depth = 0;
    float surface = 1 - normal_height_map.SampleGrad(wrap_sampler, uv, dx, dy).a;
    [loop] for (int i = 0; i < steps && depth < surface; i++) {
        uv -= delta;
        depth += layer;
        surface = 1 - normal_height_map.SampleGrad(wrap_sampler, uv, dx, dy).a;
    }
    float2 previous = uv + delta;
    float after = surface - depth;
    float before = (1 - normal_height_map.SampleGrad(wrap_sampler, previous, dx, dy).a) - (depth - layer);
    float weight = after / (after - before + 1e-5);
    uv = lerp(uv, previous, saturate(weight));
    return float4(shade(uv, p.world, dx, dy), 1);
}
)";

struct ParallaxFrame {
    float4x4 view_projection;
    float4   eye;
    float4   light;
    float4   params;
};

class ParallaxMapping final : public Sample {
public:
    const char *name() const override { return "parallaxmapping"; }
    bool init(Gpu &gpu) override
    {
        vs_ = gpu.vertex_shader("parallaxmapping_vs", parallaxmapping_source, "vs", &layout_);
        offset_ps_ = gpu.pixel_shader("parallaxmapping_offset_ps", parallaxmapping_source, "offset_ps");
        occlusion_ps_ = gpu.pixel_shader("parallaxmapping_occlusion_ps", parallaxmapping_source, "occlusion_ps");
        Mesh plane = plane_mesh(10.0f, 1, 3.0f);
        plane_ = gpu.upload(plane);
        plane.release();
        StoneField stones;
        stones.make(512, 6, 21u);
        uint32_t *normals = stones.normal_height(24.0f);
        uint32_t *colors = stones.colors({0.62f, 0.6f, 0.58f}, 22u);
        normal_height_ = gpu.texture(normals, 512, 512);
        color_ = gpu.texture(colors, 512, 512);
        release_memory(normals);
        release_memory(colors);
        stones.release();
        sampler_ = gpu.sampler();
        constants_ = gpu.constant_buffer(sizeof(ParallaxFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *c = gpu.context;
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        const float half = static_cast<float>(gpu.width) * 0.5f, h = static_cast<float>(gpu.height);
        const float3 eye = {-0.6f, 1.9f, -1.5f};
        ParallaxFrame frame;
        frame.view_projection = rotate_y(time * 0.1f) * look_at(eye, {0.2f, 0, 0.5f}, {0, 1, 0}) *
                                perspective(radians(60), half / h, 0.05f, 50);
        const float4x4 turn = rotate_y(-time * 0.1f);
        const float3 world_eye = transform_point(eye, turn);
        frame.eye = {world_eye.x, world_eye.y, world_eye.z, 1};
        const float a = time * 0.9f;
        frame.light = {0.4f + sin(a) * 1.4f, 0.9f, 0.2f + cos(a) * 1.1f, 4.0f};
        frame.params = {0.05f, -0.03f, 0.08f, 0};
        gpu.update(constants_, &frame, sizeof frame);
        c->IASetInputLayout(layout_);
        c->VSSetShader(vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetConstantBuffers(0, 1, &constants_);
        ID3D11ShaderResourceView *views[2] = {color_, normal_height_};
        c->PSSetShaderResources(0, 2, views);
        c->PSSetSamplers(0, 1, &sampler_);
        gpu.viewport(0, 0, half, h);
        c->PSSetShader(offset_ps_, nullptr, 0);
        plane_.draw(c);
        gpu.viewport(half, 0, half, h);
        c->PSSetShader(occlusion_ps_, nullptr, 0);
        plane_.draw(c);
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(offset_ps_);
        safe_release(occlusion_ps_);
        safe_release(layout_);
        plane_.release();
        safe_release(normal_height_);
        safe_release(color_);
        safe_release(sampler_);
        safe_release(constants_);
    }

private:
    ID3D11VertexShader       *vs_ = nullptr;
    ID3D11PixelShader        *offset_ps_ = nullptr;
    ID3D11PixelShader        *occlusion_ps_ = nullptr;
    ID3D11InputLayout        *layout_ = nullptr;
    DrawMesh                  plane_;
    ID3D11ShaderResourceView *normal_height_ = nullptr;
    ID3D11ShaderResourceView *color_ = nullptr;
    ID3D11SamplerState       *sampler_ = nullptr;
    ID3D11Buffer             *constants_ = nullptr;
};

const char *const sphericalenvmapping_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 world_view;
    row_major float4x4 projection;
    float4 layer;
};
Texture2DArray matcaps : register(t0);
SamplerState clamp_sampler : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 eye : EYE; float3 normal : NORMAL; float3 color : COLOR; };
Out vs(In v) {
    Out o;
    float4 p = mul(float4(v.position, 1), world_view);
    o.eye = p.xyz;
    o.normal = mul(float4(v.normal, 0), world_view).xyz;
    o.color = v.color;
    o.position = mul(p, projection);
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 r = reflect(normalize(p.eye), normalize(p.normal));
    r.z = -r.z;   // the matcap formula is written for a view down -z
    float m = 2 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1) * (r.z + 1));
    float2 uv = float2(r.x / m + 0.5, 0.5 - r.y / m);
    return float4(matcaps.Sample(clamp_sampler, float3(uv, layer.x)).rgb * saturate(p.color.r * 2), 1);
}
)";

struct EnvFrame {
    float4x4 world_view;
    float4x4 projection;
    float4   layer;
};

uint32_t matcap_texel(int kind, float nx, float ny)
{
    const float r2 = nx * nx + ny * ny;
    if (r2 > 0.998f) {
        const float scale_to = sqrt(0.998f / r2);
        nx *= scale_to;
        ny *= scale_to;
    }
    const float3 n = {nx, ny, sqrt(max(0.0f, 1.0f - nx * nx - ny * ny))};
    const float3 key = normalize({-0.45f, 0.6f, 0.65f}), fill = normalize({0.7f, -0.2f, 0.5f});
    const float3 half_vector = normalize(key + float3{0, 0, 1});
    const float diffuse = max(dot(n, key), 0.0f), fill_light = max(dot(n, fill), 0.0f);
    const float specular = power(max(dot(n, half_vector), 0.0f), 48);
    const float rim = power(1.0f - n.z, 3);
    float3 c;
    if (kind == 0) {
        c = float3{0.55f, 0.46f, 0.46f} * (0.35f + 0.8f * diffuse) + float3{0.35f, 0.15f, 0.2f} * fill_light +
            float3{0.9f, 0.45f, 0.6f} * rim + white * (specular * 0.9f);
    } else if (kind == 1) {
        c = float3{0.85f, 0.55f, 0.12f} * (0.2f + 0.9f * diffuse) + float3{0.4f, 0.2f, 0.05f} * rim +
            float3{1.0f, 0.9f, 0.6f} * specular;
    } else if (kind == 2) {
        c = float3{0.15f, 0.55f, 0.3f} * (0.3f + 0.8f * diffuse) + float3{0.5f, 0.9f, 0.7f} * (rim * 0.6f) +
            white * (specular * 0.7f);
    } else {
        const float sky = smoothstep(-0.1f, 0.25f, n.y);
        c = mix3({0.25f, 0.18f, 0.12f}, {0.75f, 0.85f, 1.0f}, sky) * (0.6f + 0.4f * n.z) + white * (specular * 1.2f);
    }
    return rgba(c.x, c.y, c.z);
}

class SphericalEnvMapping final : public Sample {
public:
    const char *name() const override { return "sphericalenvmapping"; }
    bool init(Gpu &gpu) override
    {
        vs_ = gpu.vertex_shader("sphericalenvmapping_vs", sphericalenvmapping_source, "vs", &layout_);
        ps_ = gpu.pixel_shader("sphericalenvmapping_ps", sphericalenvmapping_source);
        Mesh knot = knot_mesh(2.2f, 0.5f, 3, 4, 400, 28);
        mesh_ = gpu.upload(knot);
        knot.release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4 * layers));
        for (uint32_t k = 0; k < layers; k++)
            for (uint32_t y = 0; y < size; y++)
                for (uint32_t x = 0; x < size; x++) {
                    const float nx = (static_cast<float>(x) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
                    const float ny = 1.0f - (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f;
                    texels[(k * size + y) * size + x] = matcap_texel(static_cast<int>(k), nx, ny);
                }
        matcaps_ = gpu.texture(texels, size, size, layers);
        release_memory(texels);
        sampler_ = gpu.sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
        constants_ = gpu.constant_buffer(sizeof(EnvFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *c = gpu.context;
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        EnvFrame frame;
        frame.world_view = rotate_y(time * 0.5f) * rotate_x(0.35f) * translate({0, 0, 6.5f});
        frame.projection = perspective(radians(60), gpu.aspect(), 0.1f, 100);
        frame.layer = {static_cast<float>(static_cast<uint32_t>(time / 4.0f) % layers), 0, 0, 0};
        gpu.update(constants_, &frame, sizeof frame);
        c->IASetInputLayout(layout_);
        c->VSSetShader(vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShader(ps_, nullptr, 0);
        c->PSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShaderResources(0, 1, &matcaps_);
        c->PSSetSamplers(0, 1, &sampler_);
        mesh_.draw(c);
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(ps_);
        safe_release(layout_);
        mesh_.release();
        safe_release(matcaps_);
        safe_release(sampler_);
        safe_release(constants_);
    }

private:
    static constexpr uint32_t layers = 4;
    ID3D11VertexShader       *vs_ = nullptr;
    ID3D11PixelShader        *ps_ = nullptr;
    ID3D11InputLayout        *layout_ = nullptr;
    DrawMesh                  mesh_;
    ID3D11ShaderResourceView *matcaps_ = nullptr;
    ID3D11SamplerState       *sampler_ = nullptr;
    ID3D11Buffer             *constants_ = nullptr;
};

const char *const skeletalanimation_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 view_projection;
    float4 light_position;
    float4 eye;
    row_major float4x4 bones[16];
};
Texture2D skin_map : register(t0);
Texture2D floor_color : register(t1);
Texture2D floor_normal : register(t2);
SamplerState wrap_sampler : register(s0);
struct SkinIn {
    float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR;
    uint4 bone : BONES; float4 weight : WEIGHTS;
};
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 world : WORLD; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
Out skin_vs(SkinIn v) {
    float4x4 m = bones[v.bone.x] * v.weight.x + bones[v.bone.y] * v.weight.y +
                 bones[v.bone.z] * v.weight.z + bones[v.bone.w] * v.weight.w;
    Out o;
    float4 world = mul(float4(v.position, 1), m);
    o.position = mul(world, view_projection);
    o.world = world.xyz;
    o.normal = mul(float4(v.normal, 0), m).xyz;
    o.uv = v.uv;
    o.color = v.color;
    return o;
}
Out floor_vs(In v) {
    Out o;
    o.position = mul(float4(v.position, 1), view_projection);
    o.world = v.position;
    o.normal = v.normal;
    o.uv = v.uv;
    o.color = v.color;
    return o;
}
// A spot light from above and in front of the goblin.
float3 light(float3 world, float3 n, float3 albedo, float shininess, float strength) {
    float3 to_light = light_position.xyz - world;
    float d = length(to_light);
    float3 l = to_light / d;
    float3 v = normalize(eye.xyz - world);
    float cone = smoothstep(0.78, 0.95, dot(-l, normalize(float3(0, 1.2, 0) - light_position.xyz)));
    float diffuse = saturate(dot(n, l));
    float specular = pow(saturate(dot(reflect(-l, n), v)), shininess) * strength;
    return (albedo * (diffuse * 1.5) + specular) * cone + albedo * 0.015;
}
float4 skin_ps(Out p) : SV_Target {
    float3 albedo = skin_map.Sample(wrap_sampler, p.uv * float2(3, 6)).rgb * p.color;
    return float4(light(p.world, normalize(p.normal), albedo, 24, 0.6), 1);
}
float4 floor_ps(Out p) : SV_Target {
    float3 t = normalize(floor_normal.Sample(wrap_sampler, p.uv).rgb * 2 - 1);
    float3 n = float3(t.x, t.z, t.y);
    float3 albedo = floor_color.Sample(wrap_sampler, p.uv).rgb;
    return float4(light(p.world, n, albedo, 16, 0.15), 1);
}
)";

struct SkinVertex {
    float3  position;
    float3  normal;
    float2  uv;
    float3  color;
    uint8_t bone[4];
    float   weight[4];
};

struct SkinFrame {
    float4x4 view_projection;
    float4   light_position;
    float4   eye;
    float4x4 bones[16];
};

constexpr int bone_count = 16;

class SkeletalAnimation final : public Sample {
public:
    const char *name() const override { return "skeletalanimation"; }
    bool init(Gpu &gpu) override
    {
        const D3D11_INPUT_ELEMENT_DESC layout[6] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BONES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0}};
        skin_vs_ = gpu.vertex_shader("skeletalanimation_skin_vs", skeletalanimation_source, "skin_vs", &skin_layout_,
                                     layout, 6);
        skin_ps_ = gpu.pixel_shader("skeletalanimation_skin_ps", skeletalanimation_source, "skin_ps");
        floor_vs_ = gpu.vertex_shader("skeletalanimation_floor_vs", skeletalanimation_source, "floor_vs", &floor_layout_);
        floor_ps_ = gpu.pixel_shader("skeletalanimation_floor_ps", skeletalanimation_source, "floor_ps");
        build_goblin(gpu);
        Mesh plane = plane_mesh(16.0f, 1, 5.0f);
        floor_ = gpu.upload(plane);
        plane.release();
        StoneField stones;
        stones.make(512, 5, 41u);
        uint32_t *normals = stones.normal_height(16.0f);
        uint32_t *colors = stones.colors({0.36f, 0.36f, 0.38f}, 42u);
        floor_normal_ = gpu.texture(normals, 512, 512);
        floor_color_ = gpu.texture(colors, 512, 512);
        release_memory(normals);
        release_memory(colors);
        stones.release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float fx = static_cast<float>(x) / static_cast<float>(size), fy = static_cast<float>(y) / static_cast<float>(size);
                const float mottle = fbm(fx * 8.0f, fy * 8.0f, 0.0f, 51u, 8, 4);
                const float scales = fbm(fx * 48.0f, fy * 48.0f, 2.0f, 52u, 48, 2);
                const float v = 0.55f + 0.5f * mottle - 0.25f * smoothstep(0.45f, 0.7f, scales);
                texels[y * size + x] = rgba(v, v, v);
            }
        skin_ = gpu.texture(texels, size, size);
        release_memory(texels);
        sampler_ = gpu.sampler();
        constants_ = gpu.constant_buffer(sizeof(SkinFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *c = gpu.context;
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        SkinFrame frame;
        const float3 eye = {0.0f, 3.0f, -5.0f};
        frame.view_projection = look_at(eye, {0, 1.45f, 0}, {0, 1, 0}) * perspective(radians(55), gpu.aspect(), 0.1f, 100);
        frame.light_position = {0.0f, 7.0f, -3.0f, 1};
        frame.eye = {eye.x, eye.y, eye.z, 1};
        pose(time, frame.bones);
        gpu.update(constants_, &frame, sizeof frame);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetConstantBuffers(0, 1, &constants_);
        ID3D11ShaderResourceView *views[3] = {skin_, floor_color_, floor_normal_};
        c->PSSetShaderResources(0, 3, views);
        c->PSSetSamplers(0, 1, &sampler_);
        c->IASetInputLayout(floor_layout_);
        c->VSSetShader(floor_vs_, nullptr, 0);
        c->PSSetShader(floor_ps_, nullptr, 0);
        floor_.draw(c);
        c->IASetInputLayout(skin_layout_);
        c->VSSetShader(skin_vs_, nullptr, 0);
        c->PSSetShader(skin_ps_, nullptr, 0);
        const UINT stride = sizeof(SkinVertex), offset = 0;
        c->IASetVertexBuffers(0, 1, &goblin_vertices_, &stride, &offset);
        c->IASetIndexBuffer(goblin_indices_, DXGI_FORMAT_R32_UINT, 0);
        c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->DrawIndexed(goblin_index_count_, 0, 0);
    }
    void release() override
    {
        safe_release(skin_vs_);
        safe_release(skin_ps_);
        safe_release(floor_vs_);
        safe_release(floor_ps_);
        safe_release(skin_layout_);
        safe_release(floor_layout_);
        safe_release(goblin_vertices_);
        safe_release(goblin_indices_);
        floor_.release();
        safe_release(floor_normal_);
        safe_release(floor_color_);
        safe_release(skin_);
        safe_release(sampler_);
        safe_release(constants_);
    }

private:
    static constexpr float joints[bone_count][3] = {
        {0, 1.55f, 0},     {0, 2.1f, 0},       {0, 2.65f, 0},     {0, 3.05f, 0},
        {0.5f, 2.85f, 0},  {0.72f, 2.1f, 0},   {0.82f, 1.4f, 0},
        {-0.5f, 2.85f, 0}, {-0.72f, 2.1f, 0},  {-0.82f, 1.4f, 0},
        {0.28f, 1.55f, 0}, {0.34f, 0.85f, 0},  {0.34f, 0.2f, 0},
        {-0.28f, 1.55f, 0}, {-0.34f, 0.85f, 0}, {-0.34f, 0.2f, 0}};
    static constexpr int parents[bone_count] = {-1, 0, 1, 2, 2, 4, 5, 2, 7, 8, 0, 10, 11, 0, 13, 14};

    float3 joint(int i) const { return {joints[i][0], joints[i][1], joints[i][2]}; }

    void add_part(SkinVertex *vertices, uint32_t *indices, uint32_t &vertex_count, uint32_t &index_count, int bone,
                  int parent, int child, float3 a, float3 b, float r0, float r1, float3 color, float boot_below)
    {
        Mesh part = capsule_mesh(a, b, r0, r1, 8, 18, color);
        const float3 axis = b - a;
        const float length_squared = dot(axis, axis);
        const uint32_t base = vertex_count;
        for (uint32_t i = 0; i < part.vertex_count; i++) {
            const Vertex &v = part.vertices[i];
            SkinVertex &s = vertices[vertex_count++];
            s.position = v.position;
            s.normal = v.normal;
            s.uv = v.uv;
            s.color = v.position.y < boot_below ? float3{0.4f, 0.22f, 0.1f} : v.color;
            const float t = clamp(dot(v.position - a, axis) / length_squared, 0.0f, 1.0f);
            s.bone[0] = static_cast<uint8_t>(bone);
            s.weight[0] = 1.0f;
            if (parent >= 0 && t < 0.3f) {
                s.bone[1] = static_cast<uint8_t>(parent);
                s.weight[1] = 0.5f - 0.5f * t / 0.3f;
            } else if (child >= 0 && t > 0.7f) {
                s.bone[1] = static_cast<uint8_t>(child);
                s.weight[1] = 0.5f * (t - 0.7f) / 0.3f;
            }
            s.weight[0] = 1.0f - s.weight[1];
        }
        for (uint32_t i = 0; i < part.index_count; i++) indices[index_count++] = base + part.indices[i];
        part.release();
    }

    void build_goblin(Gpu &gpu)
    {
        const uint32_t max_vertices = 16000, max_indices = 80000;
        SkinVertex *vertices = static_cast<SkinVertex *>(allocate(sizeof(SkinVertex) * max_vertices));
        uint32_t *indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * max_indices));
        uint32_t vc = 0, ic = 0;
        const float3 green = {0.3f, 0.55f, 0.28f};
        const float none = -1.0f;
        add_part(vertices, indices, vc, ic, 0, -1, 1, {0, 1.5f, 0}, joint(1), 0.4f, 0.38f, green, none);
        add_part(vertices, indices, vc, ic, 1, 0, 2, joint(1), joint(2), 0.38f, 0.46f, green, none);
        add_part(vertices, indices, vc, ic, 2, 1, 3, joint(2), {0, 2.95f, 0}, 0.46f, 0.3f, green, none);
        add_part(vertices, indices, vc, ic, 3, 2, -1, {0, 3.05f, 0}, {0, 3.3f, -0.02f}, 0.15f, 0.2f, green, none);
        add_part(vertices, indices, vc, ic, 3, -1, -1, {0, 3.45f, -0.02f}, {0, 3.72f, 0.02f}, 0.28f, 0.26f, green, none);
        add_part(vertices, indices, vc, ic, 3, -1, -1, {0, 3.52f, -0.24f}, {0, 3.38f, -0.4f}, 0.07f, 0.06f, green, none);
        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            const int arm = side > 0 ? 4 : 7, leg = side > 0 ? 10 : 13;
            add_part(vertices, indices, vc, ic, 3, -1, -1, {0.24f * s, 3.55f, 0}, {0.46f * s, 3.72f, 0.06f}, 0.07f, 0.03f, green, none);
            add_part(vertices, indices, vc, ic, arm, 2, arm + 1, joint(arm), joint(arm + 1), 0.18f, 0.14f, green, none);
            add_part(vertices, indices, vc, ic, arm + 1, arm, arm + 2, joint(arm + 1), joint(arm + 2), 0.14f, 0.1f, green, none);
            add_part(vertices, indices, vc, ic, arm + 2, arm + 1, -1, joint(arm + 2), {0.88f * s, 1.0f, -0.05f}, 0.13f, 0.09f, green, none);
            add_part(vertices, indices, vc, ic, leg, 0, leg + 1, joint(leg), joint(leg + 1), 0.21f, 0.15f, green, none);
            add_part(vertices, indices, vc, ic, leg + 1, leg, leg + 2, joint(leg + 1), joint(leg + 2), 0.15f, 0.12f, green, 0.55f);
            add_part(vertices, indices, vc, ic, leg + 2, leg + 1, -1, joint(leg + 2), {0.34f * s, 0.12f, -0.35f}, 0.13f, 0.12f, green, 0.55f);
        }
        goblin_vertices_ = gpu.buffer(vertices, vc * static_cast<uint32_t>(sizeof(SkinVertex)), D3D11_BIND_VERTEX_BUFFER);
        goblin_indices_ = gpu.buffer(indices, ic * 4u, D3D11_BIND_INDEX_BUFFER);
        goblin_index_count_ = ic;
        release_memory(vertices);
        release_memory(indices);
    }

    void pose(float time, float4x4 *bones) const
    {
        const float w = 2.0f * time;
        float4x4 local[bone_count];
        for (int i = 0; i < bone_count; i++) local[i] = float4x4::identity();
        local[0] = rotate_y(0.45f + 0.3f * sin(0.7f * time));
        local[1] = rotate_x(-0.35f - 0.04f * sin(w));
        local[2] = rotate_x(-0.15f) * rotate_z(0.06f * sin(0.7f * time));
        local[3] = rotate_x(0.45f) * rotate_y(0.5f * sin(0.9f * time));
        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            const int arm = side > 0 ? 4 : 7, leg = side > 0 ? 10 : 13;
            const float swing = sin(1.4f * time + (side > 0 ? 0.0f : pi));
            local[arm] = rotate_z(0.12f * s) * rotate_x(0.35f + 0.3f * swing);
            local[arm + 1] = rotate_x(0.5f + 0.2f * swing);
            local[arm + 2] = rotate_x(0.2f);
            local[leg] = rotate_z(-0.12f * s) * rotate_x(0.9f + 0.08f * sin(w));
            local[leg + 1] = rotate_x(-1.5f - 0.16f * sin(w));
            local[leg + 2] = rotate_x(0.6f + 0.08f * sin(w));
        }
        for (int i = 0; i < bone_count; i++) {
            const float3 p = joint(i);
            float4x4 m = translate(-p) * local[i] * translate(p);
            if (parents[i] >= 0) m = m * bones[parents[i]];
            else m = m * translate({0, -0.42f + 0.05f * sin(w), 0});
            bones[i] = m;
        }
    }

    ID3D11VertexShader       *skin_vs_ = nullptr;
    ID3D11PixelShader        *skin_ps_ = nullptr;
    ID3D11VertexShader       *floor_vs_ = nullptr;
    ID3D11PixelShader        *floor_ps_ = nullptr;
    ID3D11InputLayout        *skin_layout_ = nullptr;
    ID3D11InputLayout        *floor_layout_ = nullptr;
    ID3D11Buffer             *goblin_vertices_ = nullptr;
    ID3D11Buffer             *goblin_indices_ = nullptr;
    uint32_t                  goblin_index_count_ = 0;
    DrawMesh                  floor_;
    ID3D11ShaderResourceView *floor_normal_ = nullptr;
    ID3D11ShaderResourceView *floor_color_ = nullptr;
    ID3D11ShaderResourceView *skin_ = nullptr;
    ID3D11SamplerState       *sampler_ = nullptr;
    ID3D11Buffer             *constants_ = nullptr;
};

const char *const vulkanscene_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 view_projection;
    row_major float4x4 sky_view_projection;
    float4 eye;
    float4 light;
};
TextureCube sky : register(t0);
SamplerState linear_sampler : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct SkyOut { float4 position : SV_Position; float3 direction : DIRECTION; };
SkyOut sky_vs(In v) {
    SkyOut o;
    o.position = mul(float4(v.position, 1), sky_view_projection).xyww;
    o.direction = v.position;
    return o;
}
float4 sky_ps(SkyOut p) : SV_Target { return sky.Sample(linear_sampler, p.direction); }
struct Out { float4 position : SV_Position; float3 world : WORLD; float3 normal : NORMAL; float3 color : COLOR; };
Out vs(In v) {
    Out o;
    o.position = mul(float4(v.position, 1), view_projection);
    o.world = v.position;
    o.normal = v.normal;
    o.color = v.color;
    return o;
}
float4 mesh_ps(Out p) : SV_Target {
    float3 n = normalize(p.normal);
    float3 l = normalize(light.xyz - p.world);
    float3 e = normalize(eye.xyz - p.world);
    float3 h = normalize(l + e);
    float diffuse = saturate(dot(l, n));
    float specular = diffuse > 0 ? pow(saturate(dot(h, n)), 64) : 0;
    float intensity = 0.1 + diffuse + specular;
    float3 r = reflect(-l, n);
    float3 c = (0.2 + 0.5 * diffuse) * p.color + 0.5 * pow(saturate(dot(r, e)), 2) * 0.75;
    if (intensity > 0.95) c *= 2.25;
    if (intensity < 0.15) c = 0.1;
    return float4(c, 1);
}
float4 logo_ps(Out p) : SV_Target {
    float3 n = normalize(p.normal);
    float3 l = normalize(light.xyz - p.world);
    float3 e = normalize(eye.xyz - p.world);
    float3 r = reflect(-l, n);
    float3 c = p.color * (0.25 + 0.85 * max(dot(n, l), 0)) + pow(saturate(dot(r, e)), 24) * 0.4;
    return float4(c, 1);
}
)";

struct SceneFrame {
    float4x4 view_projection;
    float4x4 sky_view_projection;
    float4   eye;
    float4   light;
};

float3 cube_direction(uint32_t face, float s, float t)
{
    switch (face) {
    case 0: return {1, -t, -s};
    case 1: return {-1, -t, s};
    case 2: return {s, 1, t};
    case 3: return {s, -1, -t};
    case 4: return {s, -t, 1};
    default: return {-s, -t, -1};
    }
}

float3 night_sky(float3 d)
{
    d = normalize(d);
    const float up = d.y;
    float3 c = mix3({0.12f, 0.01f, 0.01f}, {0.03f, 0.0f, 0.01f}, saturate(up * 1.5f));
    if (up < 0.0f) c = mix3({0.12f, 0.01f, 0.01f}, {0.05f, 0.0f, 0.0f}, saturate(-up * 3.0f));
    const float band = 1.0f / (1.0f + 900.0f * up * up);
    c = c + float3{0.7f, 0.05f, 0.04f} * band;
    const float clouds = fbm(d.x * 3.0f + 10.0f, d.y * 6.0f + 10.0f, d.z * 3.0f + 10.0f, 77u, 0, 5);
    const float cover = smoothstep(0.45f, 0.75f, clouds) * smoothstep(-0.05f, 0.1f, up) * (1.0f - smoothstep(0.5f, 0.9f, up));
    c = mix3(c, {0.35f, 0.22f, 0.24f}, cover * 0.7f);
    if (up > -0.02f) {
        const float grid = 90.0f;
        const float gx = d.x * grid, gy = d.y * grid, gz = d.z * grid;
        const int cx = static_cast<int>(floor(gx)), cy = static_cast<int>(floor(gy)), cz = static_cast<int>(floor(gz));
        if (lattice(cx, cy, cz, 91u) > 0.93f) {
            const float3 center = {static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f, static_cast<float>(cz) + 0.5f};
            const float3 offset = float3{gx, gy, gz} - center;
            const float star = saturate(1.0f - length(offset) * 3.5f) * (1.0f - cover);
            c = c + float3{1.0f, 0.95f, 0.9f} * (star * (0.5f + 0.5f * lattice(cx, cy, cz, 92u)));
        }
    }
    return c;
}

class VulkanScene final : public Sample {
public:
    const char *name() const override { return "vulkanscene"; }
    bool init(Gpu &gpu) override
    {
        sky_vs_ = gpu.vertex_shader("vulkanscene_sky_vs", vulkanscene_source, "sky_vs", &layout_);
        sky_ps_ = gpu.pixel_shader("vulkanscene_sky_ps", vulkanscene_source, "sky_ps");
        vs_ = gpu.vertex_shader("vulkanscene_vs", vulkanscene_source, "vs");
        mesh_ps_ = gpu.pixel_shader("vulkanscene_mesh_ps", vulkanscene_source, "mesh_ps");
        logo_ps_ = gpu.pixel_shader("vulkanscene_logo_ps", vulkanscene_source, "logo_ps");
        Mesh box = cube_mesh(2.0f);
        sky_box_ = gpu.upload(box);
        box.release();
        const float3 red = {0.75f, 0.04f, 0.04f}, pale = {0.9f, 0.9f, 0.9f};
        MeshBuilder models(60000, 300000);
        models.take(sphere_mesh(2.6f, 64, 32), scale({1, 1, 0.08f}) * translate({0, 0.6f, 1.6f}), pale);
        models.take(knot_mesh(1.3f, 0.32f, 2, 5, 320, 20), rotate_y(0.3f) * translate({0, 0.5f, 0}), red);
        models.take(sphere_mesh(0.75f, 40, 20), scale({1.0f, 1.25f, 0.9f}) * translate({-2.1f, -0.7f, -0.3f}), red);
        models.take(capsule_mesh({-2.3f, 0.2f, -0.3f}, {-2.5f, 1.2f, -0.2f}, 0.12f, 0.05f, 4, 12, white), float4x4::identity(), red);
        models.take(capsule_mesh({-1.9f, 0.2f, -0.3f}, {-1.8f, 1.1f, -0.1f}, 0.12f, 0.05f, 4, 12, white), float4x4::identity(), red);
        models.take(sphere_mesh(0.7f, 40, 20), scale({1.2f, 0.8f, 1.0f}) * translate({0.0f, -1.3f, -0.9f}), red);
        models.take(torus_mesh(0.6f, 0.22f, 48, 16), rotate_x(1.1f) * rotate_z(0.4f) * translate({2.1f, -0.6f, -0.3f}), red);
        models.take(capsule_mesh({1.9f, -0.2f, -0.2f}, {2.5f, 0.9f, -0.1f}, 0.2f, 0.12f, 6, 14, white), float4x4::identity(), red);
        models_ = gpu.upload(models.mesh());
        models.mesh().release();
        MeshBuilder logos(20000, 100000);
        logos.take(torus_mesh(1.5f, 0.2f, 96, 16), rotate_x(0.5f * pi) * scale({1.75f, 0.8f, 1.0f}) * translate({0, 3.0f, 0.3f}), red);
        Mesh cube = cube_mesh(1.0f);
        logos.add(cube, scale({4.2f, 0.6f, 0.35f}) * translate({0, 2.75f, 0.1f}), red);
        logos.add(cube, scale({3.8f, 0.36f, 0.2f}) * translate({0, 2.75f, -0.05f}), pale);
        logos.add(cube, scale({6.8f, 0.95f, 0.4f}) * translate({0, -2.45f, -0.8f}), red);
        Random random(5u);
        float x = -3.1f;
        while (x < 3.0f) {
            const float wide = random.range(0.18f, 0.45f);
            logos.add(cube, scale({wide, 0.62f, 0.2f}) * translate({x + wide * 0.5f, -2.45f, -1.02f}), pale);
            x += wide + random.range(0.06f, 0.14f);
        }
        cube.release();
        logos_ = gpu.upload(logos.mesh());
        logos.mesh().release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 6 * 4));
        for (uint32_t face = 0; face < 6; face++)
            for (uint32_t y = 0; y < size; y++)
                for (uint32_t xi = 0; xi < size; xi++) {
                    const float s = (static_cast<float>(xi) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
                    const float t = (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
                    const float3 c = night_sky(cube_direction(face, s, t));
                    texels[(face * size + y) * size + xi] = rgba(c.x, c.y, c.z);
                }
        sky_ = gpu.texture(texels, size, size, 6, true);
        release_memory(texels);
        sampler_ = gpu.sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
        no_depth_ = make_depth_state(gpu, false, false);
        no_cull_ = make_rasterizer(gpu, D3D11_CULL_NONE);
        constants_ = gpu.constant_buffer(sizeof(SceneFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *c = gpu.context;
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        const float angle = 0.35f * sin(time * 0.4f);
        const float3 eye = {sin(angle) * 10.0f, 0.6f, -cos(angle) * 10.0f};
        const float3 target = {0, 0.3f, 0};
        const float4x4 view = look_at(eye, target, {0, 1, 0});
        const float4x4 projection = perspective(radians(60), gpu.aspect(), 0.1f, 100);
        float4x4 rotation_only = view;
        rotation_only.m[3][0] = rotation_only.m[3][1] = rotation_only.m[3][2] = 0.0f;
        SceneFrame frame;
        frame.view_projection = view * projection;
        frame.sky_view_projection = rotation_only * projection;
        frame.eye = {eye.x, eye.y, eye.z, 1};
        frame.light = {3.0f, 5.0f, -8.0f, 1};
        gpu.update(constants_, &frame, sizeof frame);
        c->IASetInputLayout(layout_);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetConstantBuffers(0, 1, &constants_);
        c->OMSetDepthStencilState(no_depth_, 0);
        c->RSSetState(no_cull_);
        c->VSSetShader(sky_vs_, nullptr, 0);
        c->PSSetShader(sky_ps_, nullptr, 0);
        c->PSSetShaderResources(0, 1, &sky_);
        c->PSSetSamplers(0, 1, &sampler_);
        sky_box_.draw(c);
        c->OMSetDepthStencilState(nullptr, 0);
        c->RSSetState(nullptr);
        c->VSSetShader(vs_, nullptr, 0);
        c->PSSetShader(mesh_ps_, nullptr, 0);
        models_.draw(c);
        c->PSSetShader(logo_ps_, nullptr, 0);
        logos_.draw(c);
    }
    void release() override
    {
        safe_release(sky_vs_);
        safe_release(sky_ps_);
        safe_release(vs_);
        safe_release(mesh_ps_);
        safe_release(logo_ps_);
        safe_release(layout_);
        sky_box_.release();
        models_.release();
        logos_.release();
        safe_release(sky_);
        safe_release(sampler_);
        safe_release(no_depth_);
        safe_release(no_cull_);
        safe_release(constants_);
    }

private:
    ID3D11VertexShader       *sky_vs_ = nullptr;
    ID3D11PixelShader        *sky_ps_ = nullptr;
    ID3D11VertexShader       *vs_ = nullptr;
    ID3D11PixelShader        *mesh_ps_ = nullptr;
    ID3D11PixelShader        *logo_ps_ = nullptr;
    ID3D11InputLayout        *layout_ = nullptr;
    DrawMesh                  sky_box_;
    DrawMesh                  models_;
    DrawMesh                  logos_;
    ID3D11ShaderResourceView *sky_ = nullptr;
    ID3D11SamplerState       *sampler_ = nullptr;
    ID3D11DepthStencilState  *no_depth_ = nullptr;
    ID3D11RasterizerState    *no_cull_ = nullptr;
    ID3D11Buffer             *constants_ = nullptr;
};

}

void register_lighting(Sample **all, uint32_t &made)
{
    all[made++] = new ShadowMapping();
    all[made++] = new ShadowMappingOmni();
    all[made++] = new Deferred();
    all[made++] = new ParallaxMapping();
    all[made++] = new SphericalEnvMapping();
    all[made++] = new SkeletalAnimation();
    all[made++] = new VulkanScene();
}

}

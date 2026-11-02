#include "framework.h"

namespace samples {

namespace {

const char *const lit_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 world;
    row_major float4x4 view_projection;
    float4 light;      // xyz: direction to the light
    float4 tint;       // multiplies the vertex colour
};
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float2 uv : TEXCOORD; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = v.color * tint.rgb;
    o.uv = v.uv;
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 n = normalize(p.normal);
    float diffuse = saturate(dot(n, normalize(light.xyz)));
    float specular = pow(saturate(dot(reflect(-normalize(light.xyz), n), float3(0, 0, -1))), 24.0) * 0.5;
    return float4(p.color * (0.2 + 0.8 * diffuse) + specular, 1);
}
)";

struct LitFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   light;
    float4   tint;
};

class Triangle final : public Sample {
public:
    const char *name() const override { return "triangle"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = R"(
cbuffer Frame : register(b0) { float angle; float aspect; float2 unused; };
struct Out { float4 position : SV_Position; float3 color : COLOR; };
Out vs(float2 position : POSITION, float3 color : COLOR) {
    float s = sin(angle), c = cos(angle);
    Out o;
    o.position = float4((position.x * c - position.y * s) / aspect, position.x * s + position.y * c, 0.5, 1);
    o.color = color;
    return o;
}
float4 ps(Out p) : SV_Target { return float4(p.color, 1); }
)";
        static const D3D11_INPUT_ELEMENT_DESC layout[2] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};
        const float vertices[15] = {0.0f, 0.8f, 1, 0, 0, 0.7f, -0.5f, 0, 1, 0, -0.7f, -0.5f, 0, 0, 1};
        vs_ = gpu.vertex_shader("triangle_vs", source, "vs", &layout_, layout, 2);
        ps_ = gpu.pixel_shader("triangle_ps", source);
        vertices_ = gpu.buffer(vertices, sizeof vertices, D3D11_BIND_VERTEX_BUFFER);
        constants_ = gpu.constant_buffer(16);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float frame[4] = {time, gpu.aspect(), 0, 0};
        gpu.update(constants_, frame, sizeof frame);
        gpu.begin_frame(0.05f, 0.05f, 0.1f);
        const UINT stride = 20, offset = 0;
        auto *c = gpu.context;
        c->IASetVertexBuffers(0, 1, &vertices_, &stride, &offset);
        c->IASetInputLayout(layout_);
        c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->VSSetShader(vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShader(ps_, nullptr, 0);
        c->Draw(3, 0);
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(ps_);
        safe_release(layout_);
        safe_release(vertices_);
        safe_release(constants_);
    }

private:
    ID3D11VertexShader *vs_ = nullptr;
    ID3D11PixelShader  *ps_ = nullptr;
    ID3D11InputLayout  *layout_ = nullptr;
    ID3D11Buffer       *vertices_ = nullptr;
    ID3D11Buffer       *constants_ = nullptr;
};

class PushConstants final : public Sample {
public:
    const char *name() const override { return "pushconstants"; }
    bool init(Gpu &gpu) override
    {
        vs_ = gpu.vertex_shader("lit_vs", lit_source, "vs", &layout_);
        ps_ = gpu.pixel_shader("lit_ps", lit_source);
        Mesh mesh = sphere_mesh(0.35f, 32, 16);
        sphere_ = gpu.upload(mesh);
        mesh.release();
        constants_ = gpu.constant_buffer(sizeof(LitFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.08f, 0.08f, 0.12f);
        auto *c = gpu.context;
        c->IASetInputLayout(layout_);
        c->VSSetShader(vs_, nullptr, 0);
        c->PSSetShader(ps_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetConstantBuffers(0, 1, &constants_);
        const float4x4 vp = look_at({0, 2.5f, -5}, {0, 0, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        for (int i = 0; i < 16; i++) {
            const float a = static_cast<float>(i) / 16.0f * 2 * pi + time;
            LitFrame frame;
            frame.world = translate({cos(a) * 2.2f, sin(time * 2 + i) * 0.4f, sin(a) * 2.2f});
            frame.view_projection = vp;
            frame.light = {0.5f, 1, -0.5f, 0};
            frame.tint = {0.5f + 0.5f * sin(a), 0.5f + 0.5f * sin(a + 2), 0.5f + 0.5f * sin(a + 4), 1};
            gpu.update(constants_, &frame, sizeof frame);
            sphere_.draw(c);
        }
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(ps_);
        safe_release(layout_);
        sphere_.release();
        safe_release(constants_);
    }

private:
    ID3D11VertexShader *vs_ = nullptr;
    ID3D11PixelShader  *ps_ = nullptr;
    ID3D11InputLayout  *layout_ = nullptr;
    DrawMesh            sphere_;
    ID3D11Buffer       *constants_ = nullptr;
};

class Gears final : public Sample {
public:
    const char *name() const override { return "gears"; }
    bool init(Gpu &gpu) override
    {
        vs_ = gpu.vertex_shader("lit_vs", lit_source, "vs", &layout_);
        ps_ = gpu.pixel_shader("lit_ps", lit_source);
        Mesh a = gear_mesh(1.0f, 4.0f, 1.0f, 20, 0.7f, {1.0f, 0.1f, 0.1f});
        Mesh b = gear_mesh(0.5f, 2.0f, 2.0f, 10, 0.7f, {0.1f, 1.0f, 0.2f});
        Mesh d = gear_mesh(1.3f, 2.0f, 0.5f, 10, 0.7f, {0.2f, 0.2f, 1.0f});
        gears_[0] = gpu.upload(a);
        gears_[1] = gpu.upload(b);
        gears_[2] = gpu.upload(d);
        a.release();
        b.release();
        d.release();
        constants_ = gpu.constant_buffer(sizeof(LitFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        auto *c = gpu.context;
        c->IASetInputLayout(layout_);
        c->VSSetShader(vs_, nullptr, 0);
        c->PSSetShader(ps_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetConstantBuffers(0, 1, &constants_);
        const float4x4 view = rotate_y(time * 0.3f) * rotate_x(0.35f) * translate({0, 0, 16});
        const float4x4 vp = view * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        const float angle = time * 2.0f;
        const float3 positions[3] = {{-3.0f, 0.0f, 0.0f}, {3.1f, 0.0f, 0.0f}, {-3.1f, -6.2f, 0.0f}};
        const float rotations[3] = {angle, -2.0f * angle - radians(9), -2.0f * angle - radians(30)};
        for (int i = 0; i < 3; i++) {
            LitFrame frame;
            frame.world = rotate_z(rotations[i]) * translate(positions[i]);
            frame.view_projection = vp;
            frame.light = {0.3f, 0.6f, -1, 0};
            frame.tint = {1, 1, 1, 1};
            gpu.update(constants_, &frame, sizeof frame);
            gears_[i].draw(c);
        }
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(ps_);
        safe_release(layout_);
        for (DrawMesh &gear : gears_) gear.release();
        safe_release(constants_);
    }

private:
    ID3D11VertexShader *vs_ = nullptr;
    ID3D11PixelShader  *ps_ = nullptr;
    ID3D11InputLayout  *layout_ = nullptr;
    DrawMesh            gears_[3];
    ID3D11Buffer       *constants_ = nullptr;
};

class Texture final : public Sample {
public:
    const char *name() const override { return "texture"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = R"(
cbuffer Frame : register(b0) { row_major float4x4 world; row_major float4x4 view_projection; float4 light; float4 tint; };
Texture2D image : register(t0);
SamplerState filter : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float2 uv : TEXCOORD; };
Out vs(In v) { Out o; o.position = mul(mul(float4(v.position, 1), world), view_projection); o.uv = v.uv; return o; }
float4 ps(Out p) : SV_Target { return image.Sample(filter, p.uv); }
)";
        vs_ = gpu.vertex_shader("texture_vs", source, "vs", &layout_);
        ps_ = gpu.pixel_shader("texture_ps", source);
        Mesh mesh = plane_mesh(8.0f, 1, 4.0f);
        plane_ = gpu.upload(mesh);
        mesh.release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const bool check = ((x / 32) + (y / 32)) % 2 == 0;
                const uint32_t r = check ? 230u : 40u, g = check ? 200u : 90u + (x % 32) * 4u, b = check ? 120u : 200u;
                texels[y * size + x] = 0xFF000000u | (b << 16u) | (g << 8u) | r;
            }
        texture_ = gpu.texture(texels, size, size);
        release_memory(texels);
        sampler_ = gpu.sampler();
        constants_ = gpu.constant_buffer(sizeof(LitFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.1f, 0.1f, 0.1f);
        LitFrame frame = {};
        frame.world = rotate_y(time * 0.4f);
        frame.view_projection = look_at({0, 1.5f, -4}, {0, 0, 1}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        gpu.update(constants_, &frame, sizeof frame);
        auto *c = gpu.context;
        c->IASetInputLayout(layout_);
        c->VSSetShader(vs_, nullptr, 0);
        c->VSSetConstantBuffers(0, 1, &constants_);
        c->PSSetShader(ps_, nullptr, 0);
        c->PSSetShaderResources(0, 1, &texture_);
        c->PSSetSamplers(0, 1, &sampler_);
        plane_.draw(c);
    }
    void release() override
    {
        safe_release(vs_);
        safe_release(ps_);
        safe_release(layout_);
        plane_.release();
        safe_release(texture_);
        safe_release(sampler_);
        safe_release(constants_);
    }

private:
    ID3D11VertexShader       *vs_ = nullptr;
    ID3D11PixelShader        *ps_ = nullptr;
    ID3D11InputLayout        *layout_ = nullptr;
    DrawMesh                  plane_;
    ID3D11ShaderResourceView *texture_ = nullptr;
    ID3D11SamplerState       *sampler_ = nullptr;
    ID3D11Buffer             *constants_ = nullptr;
};

}

Sample *make_triangle() { return new Triangle(); }
Sample *make_pushconstants() { return new PushConstants(); }
Sample *make_gears() { return new Gears(); }
Sample *make_texture() { return new Texture(); }

}

// triangle, pushconstants, gears, texture
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
        static const D3D12_INPUT_ELEMENT_DESC layout[2] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        const float vertices[15] = {0.0f, 0.8f, 1, 0, 0, 0.7f, -0.5f, 0, 1, 0, -0.7f, -0.5f, 0, 0, 1};
        root_ = gpu.root_signature(RootLayout().cbv(0, D3D12_SHADER_VISIBILITY_VERTEX));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, gpu.vs("triangle_vs", source), gpu.ps("triangle_ps", source));
        desc.InputLayout = {layout, 2};
        pipeline_ = gpu.pipeline(desc);
        vertices_ = gpu.buffer(vertices, sizeof vertices, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        if (vertices_ != nullptr) view_ = {vertices_->GetGPUVirtualAddress(), sizeof vertices, 20};
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float frame[4] = {time, gpu.aspect(), 0, 0};
        gpu.begin_frame(0.05f, 0.05f, 0.1f);
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(frame, sizeof frame));
        l->IASetVertexBuffers(0, 1, &view_);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        l->DrawInstanced(3, 1, 0, 0);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(pipeline_);
        safe_release(vertices_);
    }

private:
    ID3D12RootSignature     *root_ = nullptr;
    ID3D12PipelineState     *pipeline_ = nullptr;
    ID3D12Resource          *vertices_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view_ = {};
};

// Vulkan's push constants are D3D12's root constants: each sphere's world
// matrix and tint written straight into the root arguments, the frame's
// camera in a constant buffer beside them.
class PushConstants final : public Sample {
public:
    const char *name() const override { return "pushconstants"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = R"(
cbuffer Object : register(b0) { row_major float4x4 world; float4 tint; };
cbuffer Frame : register(b1) { row_major float4x4 view_projection; float4 light; };
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = v.color * tint.rgb;
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 n = normalize(p.normal);
    float diffuse = saturate(dot(n, normalize(light.xyz)));
    float specular = pow(saturate(dot(reflect(-normalize(light.xyz), n), float3(0, 0, -1))), 24.0) * 0.5;
    return float4(p.color * (0.2 + 0.8 * diffuse) + specular, 1);
}
)";
        root_ = gpu.root_signature(RootLayout().constants(0, 20).cbv(1));
        pipeline_ = gpu.pipeline(gpu.graphics(root_, gpu.vs("pushconstants_vs", source),
                                              gpu.ps("pushconstants_ps", source)));
        Mesh mesh = sphere_mesh(0.35f, 32, 16);
        sphere_ = gpu.mesh(mesh);
        mesh.release();
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.08f, 0.08f, 0.12f);
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(pipeline_);
        struct {
            float4x4 view_projection;
            float4   light;
        } frame = {look_at({0, 2.5f, -5}, {0, 0, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100),
                   {0.5f, 1, -0.5f, 0}};
        l->SetGraphicsRootConstantBufferView(1, gpu.constants(&frame, sizeof frame));
        for (int i = 0; i < 16; i++) {
            const float a = static_cast<float>(i) / 16.0f * 2 * pi + time;
            struct {
                float4x4 world;
                float4   tint;
            } object = {translate({cos(a) * 2.2f, sin(time * 2 + i) * 0.4f, sin(a) * 2.2f}),
                        {0.5f + 0.5f * sin(a), 0.5f + 0.5f * sin(a + 2), 0.5f + 0.5f * sin(a + 4), 1}};
            l->SetGraphicsRoot32BitConstants(0, 20, &object, 0);
            sphere_.draw(l);
        }
    }
    void release() override
    {
        safe_release(root_);
        safe_release(pipeline_);
        sphere_.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *pipeline_ = nullptr;
    DrawMesh             sphere_;
};

class Gears final : public Sample {
public:
    const char *name() const override { return "gears"; }
    bool init(Gpu &gpu) override
    {
        root_ = gpu.root_signature(RootLayout().cbv(0));
        pipeline_ = gpu.pipeline(gpu.graphics(root_, gpu.vs("lit_vs", lit_source), gpu.ps("lit_ps", lit_source)));
        Mesh a = gear_mesh(1.0f, 4.0f, 1.0f, 20, 0.7f, {1.0f, 0.1f, 0.1f});
        Mesh b = gear_mesh(0.5f, 2.0f, 2.0f, 10, 0.7f, {0.1f, 1.0f, 0.2f});
        Mesh d = gear_mesh(1.3f, 2.0f, 0.5f, 10, 0.7f, {0.2f, 0.2f, 1.0f});
        gears_[0] = gpu.mesh(a);
        gears_[1] = gpu.mesh(b);
        gears_[2] = gpu.mesh(d);
        a.release();
        b.release();
        d.release();
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(pipeline_);
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
            l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
            gears_[i].draw(l);
        }
    }
    void release() override
    {
        safe_release(root_);
        safe_release(pipeline_);
        for (DrawMesh &gear : gears_) gear.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *pipeline_ = nullptr;
    DrawMesh             gears_[3];
};

// A mipmapped, anisotropically filtered texture on a receding quad.
class TextureSample final : public Sample {
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
        root_ = gpu.root_signature(RootLayout()
                                       .cbv(0, D3D12_SHADER_VISIBILITY_VERTEX)
                                       .srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                       .sampler(0, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                                D3D12_SHADER_VISIBILITY_PIXEL));
        pipeline_ = gpu.pipeline(gpu.graphics(root_, gpu.vs("texture_vs", source), gpu.ps("texture_ps", source)));
        Mesh mesh = plane_mesh(8.0f, 1, 4.0f);
        plane_ = gpu.mesh(mesh);
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
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.1f, 0.1f, 0.1f);
        LitFrame frame = {};
        frame.world = rotate_y(time * 0.4f);
        frame.view_projection = look_at({0, 1.5f, -4}, {0, 0, 1}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetGraphicsRootDescriptorTable(1, texture_.srv.gpu);
        plane_.draw(l);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(pipeline_);
        plane_.release();
        texture_.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *pipeline_ = nullptr;
    DrawMesh             plane_;
    Texture              texture_;
};

}  // namespace

Sample *make_triangle() { return new Triangle(); }
Sample *make_pushconstants() { return new PushConstants(); }
Sample *make_gears() { return new Gears(); }
Sample *make_texture() { return new TextureSample(); }

}  // namespace samples

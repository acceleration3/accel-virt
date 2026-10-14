// computeparticles, computeshader, raytracing (the D3D11 suite's compute samples,
// ported from Sascha Willems' Vulkan samples) and nbody.
#include "framework.h"

namespace samples {

namespace {

// Additive splats land in fixed point, one integer per channel, so the sum is
// the same whatever order the device adds them in; this is one.
const char *const splat_source = R"(
#define ONE 1024.0f
void splat(__global uint *accum, int width, int height, int x, int y, float3 c) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    __global uint *at = accum + (y * width + x) * 3;
    atomic_add(at + 0, (uint)(c.x * ONE));
    atomic_add(at + 1, (uint)(c.y * ONE));
    atomic_add(at + 2, (uint)(c.z * ONE));
}
__kernel void resolve(__global const uint *accum, __global uchar4 *rgba) {
    const size_t i = get_global_id(0);
    const float3 c = min((float3)(accum[i * 3], accum[i * 3 + 1], accum[i * 3 + 2]) / ONE, 1.0f);
    rgba[i] = (uchar4)(convert_uchar_sat_rte(c.x * 255.0f), convert_uchar_sat_rte(c.y * 255.0f),
                       convert_uchar_sat_rte(c.z * 255.0f), 255);
}
)";

// An accumulation target and the picture resolved from it.
struct Splats {
    cl_program program = nullptr;
    cl_kernel  resolve = nullptr;
    cl_mem     accum = nullptr;
    cl_mem     rgba = nullptr;
    uint32_t   width = 0, height = 0;

    void create(Gpu &gpu, cl_program with, uint32_t w, uint32_t h)
    {
        width = w;
        height = h;
        resolve = gpu.kernel(with, "resolve");
        accum = gpu.buffer(static_cast<size_t>(w) * h * 12u);
        rgba = gpu.buffer(static_cast<size_t>(w) * h * 4u);
        gpu.arg(resolve, 0, accum);
        gpu.arg(resolve, 1, rgba);
    }
    void clear(Gpu &gpu)
    {
        const uint32_t zero = 0;
        gpu.check(cl.EnqueueFillBuffer(gpu.queue, accum, &zero, 4, 0, static_cast<size_t>(width) * height * 12u, 0,
                                       nullptr, nullptr),
                  "clEnqueueFillBuffer");
    }
    void finish(Gpu &gpu) { gpu.run(resolve, static_cast<size_t>(width) * height, 256); }
    void capture(Gpu &gpu, Capture &capture)
    {
        uint8_t *pixels = static_cast<uint8_t *>(allocate(static_cast<size_t>(width) * height * 4u));
        gpu.read(rgba, pixels, static_cast<size_t>(width) * height * 4u);
        if (!gpu.failed) capture.image_rgba(pixels, width, height);
        release_memory(pixels);
    }
    void release()
    {
        samples::release(resolve);
        samples::release(accum);
        samples::release(rgba);
    }
};

// ---------------------------------------------------------------- computeparticles

// A quarter of a million particles pulled by an attractor that swings across
// the screen, each drawn as a glowing sprite coloured by its own gradient.
class ComputeParticles final : public Sample {
public:
    const char *name() const override { return "computeparticles"; }
    Work        work() const override { return {count / 1e6, "Mpart/s"}; }
    bool        init(Gpu &gpu) override
    {
        static const char *const source = R"(
float2 attraction(float2 pos, float2 target) {
    const float2 delta = target - pos;
    const float inv = rsqrt(dot(delta, delta) + 0.5f);
    return delta * inv * inv * inv * 0.0035f;
}
float2 repulsion(float2 pos, float2 target) {
    const float2 delta = target - pos;
    const float d = sqrt(dot(delta, delta));
    return delta * (1.0f / (d * d * d)) * -0.000035f;
}
// A particle is (position, velocity) then (gradient, -, -, -).
__kernel void simulate(__global float4 *particles, float dt, float dest_x, float dest_y, float force, uint count) {
    const uint i = get_global_id(0);
    if (i >= count) return;
    float4 motion = particles[i * 2];
    float4 gradient = particles[i * 2 + 1];
    float2 pos = motion.xy, vel = motion.zw;
    const float2 target = (float2)(dest_x, dest_y);
    vel += repulsion(pos, target) * 0.05f * force;
    const float2 moved = pos + vel * dt;
    if (moved.x < -1.0f || moved.x > 1.0f || moved.y < -1.0f || moved.y > 1.0f)
        vel = -vel * 0.1f + attraction(moved, target) * 12.0f * force;
    else
        pos = moved;
    gradient.x += 0.02f * dt;
    if (gradient.x > 1.0f) gradient.x -= 1.0f;
    particles[i * 2] = (float4)(pos, vel);
    particles[i * 2 + 1] = gradient;
}
__constant float3 ramp_keys[5] = {(float3)(1.0f, 0.85f, 0.15f), (float3)(1.0f, 0.2f, 0.75f), (float3)(0.15f, 0.85f, 1.0f),
                                  (float3)(0.45f, 0.35f, 1.0f), (float3)(1.0f, 0.85f, 0.15f)};
__kernel void draw(__global const float4 *particles, __global uint *accum, int width, int height, uint count) {
    const uint i = get_global_id(0);
    if (i >= count) return;
    const float2 pos = particles[i * 2].xy;
    const float t = fmin(fmax(particles[i * 2 + 1].x, 0.0f), 0.999f) * 4.0f;
    const int k = (int)t;
    const float3 color = mix(ramp_keys[k], ramp_keys[k + 1], t - (float)k) * 0.2f;
    const float px = (pos.x * 0.5f + 0.5f) * width, py = (0.5f - pos.y * 0.5f) * height;
    const int x0 = (int)floor(px) - 4, y0 = (int)floor(py) - 4;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            const float2 uv = ((float2)(x0 + x, y0 + y) + 0.5f - (float2)(px, py)) / 4.0f;
            float glow = clamp(1.0f - length(uv), 0.0f, 1.0f);
            glow *= glow;
            if (glow > 0.0f) splat(accum, width, height, x0 + x, y0 + y, color * glow);
        }
}
)";
        char combined[8192] = {};
        copy_text(combined, sizeof combined, splat_source);
        append_text(combined, sizeof combined, source);
        program_ = gpu.program(combined);
        simulate_ = gpu.kernel(program_, "simulate");
        draw_ = gpu.kernel(program_, "draw");
        splats_.create(gpu, program_, width, height);
        float  *start = static_cast<float *>(allocate(count * 32u));
        Random random(0x5EEDu);
        for (uint32_t i = 0; i < count; i++) {
            start[i * 8 + 0] = random.range(-1, 1);
            start[i * 8 + 1] = random.range(-1, 1);
            start[i * 8 + 4] = start[i * 8 + 0] * 0.5f;
        }
        initial_ = gpu.buffer(count * 32u, start, CL_MEM_READ_ONLY);
        release_memory(start);
        particles_ = gpu.buffer(count * 32u);
        gpu.arg(simulate_, 0, particles_);
        gpu.arg(simulate_, 4, force);
        gpu.arg(simulate_, 5, count);
        gpu.arg(draw_, 0, particles_);
        gpu.arg(draw_, 1, splats_.accum);
        gpu.arg(draw_, 2, static_cast<int32_t>(width));
        gpu.arg(draw_, 3, static_cast<int32_t>(height));
        gpu.arg(draw_, 4, count);
        reset(gpu);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        advance(gpu, steps_ + 2);
        draw(gpu);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        reset(gpu);
        advance(gpu, static_cast<uint32_t>((capture_time + preroll) * steps_per_second + 0.001f));
        draw(gpu);
        splats_.capture(gpu, capture);
        return "ok";
    }
    void release() override
    {
        samples::release(simulate_);
        samples::release(draw_);
        samples::release(program_);
        samples::release(initial_);
        samples::release(particles_);
        splats_.release();
    }

private:
    static constexpr uint32_t count = 256u * 1024u;
    static constexpr uint32_t width = 1280, height = 720;
    static constexpr float    steps_per_second = 120.0f;
    static constexpr float    force = 8.0f;
    static constexpr float    preroll = 8.0f;
    static constexpr float    capture_time = 1.5f;

    // Time zero shows the swarm already settled into its streams.
    void reset(Gpu &gpu)
    {
        gpu.check(cl.EnqueueCopyBuffer(gpu.queue, initial_, particles_, 0, 0, count * 32u, 0, nullptr, nullptr),
                  "clEnqueueCopyBuffer");
        steps_ = 0;
        advance(gpu, static_cast<uint32_t>(preroll * steps_per_second));
    }
    void advance(Gpu &gpu, uint32_t target)
    {
        for (; steps_ < target && !gpu.failed; steps_++) {
            const float t = static_cast<float>(steps_) / steps_per_second - preroll;
            gpu.arg(simulate_, 1, 2.5f / 60.0f);
            gpu.arg(simulate_, 2, sin(t * 0.04f * 2 * pi) * 0.75f);
            gpu.arg(simulate_, 3, 0.0f);
            gpu.run(simulate_, count, 256);
        }
    }
    void draw(Gpu &gpu)
    {
        splats_.clear(gpu);
        gpu.run(draw_, count, 256);
        splats_.finish(gpu);
    }

    cl_program program_ = nullptr;
    cl_kernel  simulate_ = nullptr, draw_ = nullptr;
    cl_mem     initial_ = nullptr, particles_ = nullptr;
    Splats     splats_;
    uint32_t   steps_ = 0;
};

// ---------------------------------------------------------------- computeshader

// A painting generated on the CPU, filtered three ways by kernels reading it as
// an image: sharpen, edge detect and emboss.
class ComputeShader final : public Sample {
public:
    const char *name() const override { return "computeshader"; }
    Work        work() const override { return {3.0 * size * size / 1e6, "Mpix/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__constant sampler_t clamped = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
float3 texel(read_only image2d_t source, int2 p) { return read_imagef(source, clamped, p).xyz; }
float grey(read_only image2d_t source, int2 p) { const float3 c = texel(source, p); return (c.x + c.y + c.z) / 3.0f; }
__kernel void sharpen(read_only image2d_t source, write_only image2d_t filtered) {
    const int2 p = (int2)(get_global_id(0), get_global_id(1));
    float3 sum = texel(source, p) * 9.0f;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (x != 0 || y != 0) sum -= texel(source, p + (int2)(x, y));
    write_imagef(filtered, p, (float4)(clamp(sum, 0.0f, 1.0f), 1.0f));
}
__kernel void edgedetect(read_only image2d_t source, write_only image2d_t filtered) {
    const int2 p = (int2)(get_global_id(0), get_global_id(1));
    float sum = grey(source, p);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (x != 0 || y != 0) sum -= grey(source, p + (int2)(x, y)) / 8.0f;
    const float v = clamp(sum / 0.1f, 0.0f, 1.0f);
    write_imagef(filtered, p, (float4)(v, v, v, 1.0f));
}
__kernel void emboss(read_only image2d_t source, write_only image2d_t filtered) {
    const int2 p = (int2)(get_global_id(0), get_global_id(1));
    const float sum = -grey(source, p + (int2)(-1, -1)) - grey(source, p) + 2.0f * grey(source, p + (int2)(1, 1));
    const float v = clamp(sum + 0.5f, 0.0f, 1.0f);
    write_imagef(filtered, p, (float4)(v, v, v, 1.0f));
})");
        static const char *const names[3] = {"sharpen", "edgedetect", "emboss"};
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4u));
        paint(texels);
        image_ = gpu.image2d(size, size, CL_RGBA, CL_UNORM_INT8, texels, CL_MEM_READ_ONLY);
        release_memory(texels);
        for (int i = 0; i < 3; i++) {
            filters_[i] = gpu.kernel(program_, names[i]);
            filtered_[i] = gpu.image2d(size, size, CL_RGBA, CL_UNORM_INT8, nullptr, CL_MEM_WRITE_ONLY);
            gpu.arg(filters_[i], 0, image_);
            gpu.arg(filters_[i], 1, filtered_[i]);
        }
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        for (cl_kernel filter : filters_) gpu.run(filter, size, size, 16, 16);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        // The original and its three filtered copies side by side.
        const uint32_t strip = size * 4;
        uint8_t       *pixels = static_cast<uint8_t *>(allocate(static_cast<size_t>(strip) * size * 4u));
        uint8_t       *one = static_cast<uint8_t *>(allocate(size * size * 4u));
        for (int panel = 0; panel < 4; panel++) {
            gpu.read_image(panel == 0 ? image_ : filtered_[panel - 1], one, size, size);
            for (uint32_t y = 0; y < size; y++)
                copy_bytes(pixels + (static_cast<size_t>(y) * strip + panel * size) * 4u, one + y * size * 4u,
                           size * 4u);
        }
        if (!gpu.failed) capture.image_rgba(pixels, strip, size);
        release_memory(one);
        release_memory(pixels);
        return "ok";
    }
    void release() override
    {
        for (int i = 0; i < 3; i++) {
            samples::release(filters_[i]);
            samples::release(filtered_[i]);
        }
        samples::release(program_);
        samples::release(image_);
    }

private:
    static constexpr uint32_t size = 512;

    // Sky, clouds and smoke over a brown sea, and a tall ship.
    static void paint(uint32_t *texels)
    {
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float u = (static_cast<float>(x) + 0.5f) / size, v = (static_cast<float>(y) + 0.5f) / size;
                const float3 c = ship(u, v, sky(u, v));
                texels[y * size + x] = pack_rgba(c.x, c.y, c.z, 1);
            }
    }
    static float3 sky(float u, float v)
    {
        const float horizon = 0.82f;
        if (v > horizon) {
            const float  streak = fbm(u * 3.0f, v * 60.0f, 5u, 4);
            const float3 sea = {0.46f, 0.38f, 0.29f};
            return sea * (0.75f + 0.45f * streak) * (1.0f - (v - horizon) * 1.2f);
        }
        const float3 top = {0.60f, 0.70f, 0.82f}, low = {0.82f, 0.82f, 0.78f};
        float3       c = top * (1 - v / horizon) + low * (v / horizon);
        const float  n = fbm(u * 5.0f, v * 7.0f, 11u, 5);
        const float  cloud = smooth(0.42f, 0.7f, n + v * 0.25f - 0.1f);
        const float  shade = 0.45f + 0.5f * fbm(u * 9.0f + 3.0f, v * 9.0f, 23u, 4);
        c = c * (1 - cloud) + float3{shade, shade, shade * 1.02f} * cloud;
        const float dx = (u - 0.45f) / 0.18f, dy = (v - 0.72f) / 0.1f;
        const float puff = smooth(1.0f, 0.3f, dx * dx + dy * dy) * smooth(0.3f, 0.6f, fbm(u * 12, v * 12, 41u, 4));
        const float white = 0.85f + 0.12f * fbm(u * 20, v * 20, 43u, 3);
        return c * (1 - puff) + float3{white, white, white * 1.03f} * puff;
    }
    static float3 ship(float u, float v, float3 c)
    {
        const float grain = fbm(u * 40.0f, v * 40.0f, 77u, 3);
        const float masts[3] = {0.57f, 0.68f, 0.78f};
        for (int m = 0; m < 3; m++) {
            if (abs(u - masts[m]) < 0.004f && v > 0.18f + m * 0.03f && v < 0.82f) c = {0.25f, 0.18f, 0.1f};
            for (int s = 0; s < 3; s++) {
                const float v0 = 0.24f + s * 0.16f + m * 0.03f, v1 = v0 + 0.13f;
                if (v < v0 || v > v1) continue;
                const float f = (v - v0) / (v1 - v0);
                const float half = 0.035f + 0.025f * s + 0.012f * f + 0.01f * sin(f * pi);
                if (abs(u - masts[m] + 0.01f) < half) {
                    const float light = 0.72f + 0.2f * sin(f * pi) + 0.1f * grain - 0.12f * (u - masts[m]) / half;
                    c = float3{0.9f, 0.84f, 0.74f} * light;
                }
            }
        }
        const float hx = (u - 0.68f) / 0.2f, hy = (v - 0.8f) / 0.075f;
        if (hy > -0.35f && hx * hx + hy * hy < 1.0f) c = float3{0.34f, 0.2f, 0.1f} * (0.6f + 0.5f * grain);
        if (hy > -0.45f && hy < -0.35f && abs(hx) < 0.9f) c = {0.55f, 0.32f, 0.14f};
        return c;
    }

    cl_program program_ = nullptr;
    cl_kernel  filters_[3] = {};
    cl_mem     image_ = nullptr;
    cl_mem     filtered_[3] = {};
};

// ---------------------------------------------------------------- raytracing

// Three spheres over a plane, one bounce of reflection and hard shadows, the
// light circling once every eight seconds.
class Raytracing final : public Sample {
public:
    const char *name() const override { return "raytracing"; }
    Work        work() const override { return {static_cast<double>(width) * height / 1e6, "Mpix/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__constant float3 centers[3] = {(float3)(-2.25f, 1.0f, 0.0f), (float3)(0.0f, 2.5f, 0.0f), (float3)(2.25f, 1.0f, 0.0f)};
__constant float3 colors[3] = {(float3)(1, 0, 0), (float3)(0, 0, 1), (float3)(0, 1, 0)};
float sphere_hit(float3 o, float3 d, float3 center) {
    const float3 oc = o - center;
    const float b = dot(oc, d);
    const float h = b * b - (dot(oc, oc) - 1.0f);
    return h < 0.0f ? -1.0f : -b - sqrt(h);
}
int intersect(float3 o, float3 d, float *t) {
    int id = -1;
    *t = 1000.0f;
    for (int i = 0; i < 3; i++) {
        const float s = sphere_hit(o, d, centers[i]);
        if (s > 0.0001f && s < *t) { *t = s; id = i; }
    }
    const float p = -o.y / d.y;
    if (p > 0.0001f && p < *t) { *t = p; id = 3; }
    return id;
}
float shadow(float3 o, float3 d) {
    for (int i = 0; i < 3; i++)
        if (sphere_hit(o, d, centers[i]) > 0.0001f) return 0.5f;
    return 1.0f;
}
float3 shade(float3 *o, float3 *d, float3 light, float3 camera) {
    float t;
    const int id = intersect(*o, *d, &t);
    if (id < 0) return (float3)(0.0f);
    const float3 p = *o + t * *d;
    const float3 l = normalize(light - p);
    float3 n = (float3)(0, 1, 0);
    float3 color;
    if (id == 3) {
        color = (float3)(clamp(dot(n, l), 0.0f, 1.0f));
    } else {
        n = p - centers[id];
        const float diffuse = clamp(dot(n, l), 0.0f, 1.0f);
        const float specular = pow(clamp(dot(n, normalize(l + normalize(camera))), 0.0f, 1.0f), 32.0f);
        color = diffuse * colors[id] + specular * 2.0f;
    }
    color *= shadow(p, l);
    color = mix(color, (float3)(0.0f), clamp(t / 20.0f, 0.0f, 1.0f));
    *d = *d - 2.0f * dot(n, *d) * n;
    *o = p;
    return color;
}
__kernel void trace(write_only image2d_t result, float lx, float ly, float lz, float aspect) {
    const int2 id = (int2)(get_global_id(0), get_global_id(1));
    const int2 size = get_image_dim(result);
    if (id.x >= size.x || id.y >= size.y) return;
    const float2 uv = ((float2)(id.x, id.y) + 0.5f) / (float2)(size.x, size.y);
    const float3 camera = (float3)(0.0f, 1.5f, 4.0f), light = (float3)(lx, ly, lz);
    float3 o = camera;
    float3 d = normalize((float3)((uv.x * 2 - 1) * aspect, 1 - uv.y * 2, -1));
    const float3 color = shade(&o, &d, light, camera);
    const float3 reflected = shade(&o, &d, light, camera);
    write_imagef(result, id, (float4)(0.6f * color + 0.4f * mix(reflected, color, 0.6f), 1.0f));
})");
        kernel_ = gpu.kernel(program_, "trace");
        image_ = gpu.image2d(width, height, CL_RGBA, CL_UNORM_INT8, nullptr, CL_MEM_WRITE_ONLY);
        gpu.arg(kernel_, 0, image_);
        gpu.arg(kernel_, 4, static_cast<float>(width) / height);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        render(gpu, static_cast<float>(frame_++) / 60.0f);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        render(gpu, 1.5f);
        uint8_t *pixels = static_cast<uint8_t *>(allocate(width * height * 4u));
        gpu.read_image(image_, pixels, width, height);
        if (!gpu.failed) capture.image_rgba(pixels, width, height);
        release_memory(pixels);
        return "ok";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(image_);
        frame_ = 0;
    }

private:
    static constexpr uint32_t width = 1280, height = 720;
    void                      render(Gpu &gpu, float time)
    {
        const float a = time / 8.0f * 2 * pi;
        gpu.arg(kernel_, 1, sin(a) * 2.0f);
        gpu.arg(kernel_, 2, 5.0f);
        gpu.arg(kernel_, 3, cos(a) * 2.0f);
        gpu.run(kernel_, width, height, 16, 16);
    }
    cl_program program_ = nullptr;
    cl_kernel  kernel_ = nullptr;
    cl_mem     image_ = nullptr;
    uint32_t   frame_ = 0;
};

// ---------------------------------------------------------------- nbody

// Two discs of 8192 bodies each falling into one another, every body pulled by
// every other through tiles of local memory; drawn as a density map.
class NBody final : public Sample {
public:
    const char *name() const override { return "nbody"; }
    Work        work() const override { return {20.0 * count * count / 1e9, "GFLOP/s"}; }
    bool        init(Gpu &gpu) override
    {
        static const char *const source = R"(
#define TILE 256
__kernel void integrate(__global const float4 *pos_in, __global const float4 *vel_in, __global float4 *pos_out,
                        __global float4 *vel_out, uint count, float dt) {
    __local float4 tile[TILE];
    const uint i = get_global_id(0), lid = get_local_id(0);
    const float4 p = pos_in[i];
    float3 acceleration = (float3)(0.0f);
    for (uint start = 0; start < count; start += TILE) {
        tile[lid] = pos_in[start + lid];
        barrier(CLK_LOCAL_MEM_FENCE);
        for (uint j = 0; j < TILE; j++) {
            const float4 q = tile[j];
            const float3 r = q.xyz - p.xyz;
            const float d2 = dot(r, r) + 0.0025f;
            const float inv = rsqrt(d2);
            acceleration += r * (q.w * inv * inv * inv);
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    float4 v = vel_in[i];
    v.xyz += acceleration * dt;
    v.xyz *= 0.9995f;
    vel_out[i] = v;
    pos_out[i] = (float4)(p.xyz + v.xyz * dt, p.w);
}
__kernel void draw(__global const float4 *pos, __global const float4 *vel, __global uint *accum, int width, int height) {
    const uint i = get_global_id(0);
    const float4 p = pos[i];
    const float speed = length(vel[i].xyz);
    const float3 slow = (float3)(0.25f, 0.45f, 1.0f), fast = (float3)(1.0f, 0.55f, 0.2f);
    const float3 c = mix(slow, fast, clamp(speed * 0.6f, 0.0f, 1.0f)) * 0.3f;
    // A gentle tilt so the discs read as discs.
    const float x = p.x, y = p.y * 0.8f + p.z * 0.6f;
    const int px = (int)((x / 3.2f * 0.5f + 0.5f) * width), py = (int)((0.5f - y / 3.2f * 0.5f) * height);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) splat(accum, width, height, px + dx, py + dy, c * (dx == 0 && dy == 0 ? 1.0f : 0.35f));
}
)";
        char combined[8192] = {};
        copy_text(combined, sizeof combined, splat_source);
        append_text(combined, sizeof combined, source);
        program_ = gpu.program(combined);
        integrate_ = gpu.kernel(program_, "integrate");
        draw_ = gpu.kernel(program_, "draw");
        splats_.create(gpu, program_, image_size, image_size);
        float *pos = static_cast<float *>(allocate(count * 16u)), *vel = static_cast<float *>(allocate(count * 16u));
        Random random(0x0B0D1E5u);
        for (uint32_t i = 0; i < count; i++) {
            const bool   second = i >= count / 2;
            const float3 centre = second ? float3{1.6f, 0.3f, 0.0f} : float3{-1.6f, -0.3f, 0.0f};
            const float3 drift = second ? float3{-0.18f, 0.05f, 0.0f} : float3{0.18f, -0.05f, 0.0f};
            const float  angle = random.range(0.0f, 2.0f * pi), radius = 0.15f + 1.1f * sqrt(random.unit());
            const float  height = random.range(-0.04f, 0.04f);
            const float  orbit = sqrt(count / 2 * body_mass / (radius + 0.2f)) * 0.9f;
            pos[i * 4 + 0] = centre.x + cos(angle) * radius;
            pos[i * 4 + 1] = centre.y + sin(angle) * radius;
            pos[i * 4 + 2] = centre.z + height;
            pos[i * 4 + 3] = body_mass;
            vel[i * 4 + 0] = drift.x - sin(angle) * orbit;
            vel[i * 4 + 1] = drift.y + cos(angle) * orbit;
            vel[i * 4 + 2] = 0.0f;
        }
        initial_pos_ = gpu.buffer(count * 16u, pos, CL_MEM_READ_ONLY);
        initial_vel_ = gpu.buffer(count * 16u, vel, CL_MEM_READ_ONLY);
        release_memory(pos);
        release_memory(vel);
        for (int i = 0; i < 2; i++) {
            pos_[i] = gpu.buffer(count * 16u);
            vel_[i] = gpu.buffer(count * 16u);
        }
        gpu.arg(integrate_, 4, count);
        gpu.arg(integrate_, 5, 0.005f);
        gpu.arg(draw_, 2, splats_.accum);
        gpu.arg(draw_, 3, static_cast<int32_t>(image_size));
        gpu.arg(draw_, 4, static_cast<int32_t>(image_size));
        reset(gpu);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { advance(gpu); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        reset(gpu);
        for (uint32_t i = 0; i < capture_steps; i++) {
            advance(gpu);
            gpu.flush();
        }
        splats_.clear(gpu);
        gpu.arg(draw_, 0, pos_[current_]);
        gpu.arg(draw_, 1, vel_[current_]);
        gpu.run(draw_, count, 256);
        splats_.finish(gpu);
        splats_.capture(gpu, capture);
        float *pos = static_cast<float *>(allocate(count * 16u));
        gpu.read(pos_[current_], pos, count * 16u);
        if (!gpu.failed) capture.buffer(Capture::f32, pos, count * 4);
        release_memory(pos);
        return "ok";
    }
    void release() override
    {
        samples::release(integrate_);
        samples::release(draw_);
        samples::release(program_);
        samples::release(initial_pos_);
        samples::release(initial_vel_);
        for (int i = 0; i < 2; i++) {
            samples::release(pos_[i]);
            samples::release(vel_[i]);
        }
        splats_.release();
    }

private:
    static constexpr uint32_t count = 16384;
    static constexpr uint32_t image_size = 1024;
    static constexpr uint32_t capture_steps = 400;
    static constexpr float    body_mass = 0.00005f;

    void reset(Gpu &gpu)
    {
        gpu.check(cl.EnqueueCopyBuffer(gpu.queue, initial_pos_, pos_[0], 0, 0, count * 16u, 0, nullptr, nullptr),
                  "clEnqueueCopyBuffer");
        gpu.check(cl.EnqueueCopyBuffer(gpu.queue, initial_vel_, vel_[0], 0, 0, count * 16u, 0, nullptr, nullptr),
                  "clEnqueueCopyBuffer");
        current_ = 0;
    }
    void advance(Gpu &gpu)
    {
        gpu.arg(integrate_, 0, pos_[current_]);
        gpu.arg(integrate_, 1, vel_[current_]);
        gpu.arg(integrate_, 2, pos_[1 - current_]);
        gpu.arg(integrate_, 3, vel_[1 - current_]);
        gpu.run(integrate_, count, 256);
        current_ = 1 - current_;
    }

    cl_program program_ = nullptr;
    cl_kernel  integrate_ = nullptr, draw_ = nullptr;
    cl_mem     initial_pos_ = nullptr, initial_vel_ = nullptr;
    cl_mem     pos_[2] = {}, vel_[2] = {};
    Splats     splats_;
    uint32_t   current_ = 0;
};

}  // namespace

void register_compute(Sample **all, uint32_t &made)
{
    all[made++] = new ComputeParticles();
    all[made++] = new ComputeShader();
    all[made++] = new Raytracing();
    all[made++] = new NBody();
}

}  // namespace samples

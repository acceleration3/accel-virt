#include "framework.h"

namespace samples {

namespace {
void capture_rgba_buffer(Gpu &gpu, Capture &capture, cl_mem rgba, uint32_t width, uint32_t height)
{
    const size_t bytes = static_cast<size_t>(width) * height * 4u;
    uint8_t     *pixels = static_cast<uint8_t *>(allocate(bytes));
    gpu.read(rgba, pixels, bytes);
    if (!gpu.failed) capture.image_rgba(pixels, width, height);
    release_memory(pixels);
}

void capture_image(Gpu &gpu, Capture &capture, cl_mem image, uint32_t width, uint32_t height)
{
    uint8_t *pixels = static_cast<uint8_t *>(allocate(static_cast<size_t>(width) * height * 4u));
    gpu.read_image(image, pixels, width, height);
    if (!gpu.failed) capture.image_rgba(pixels, width, height);
    release_memory(pixels);
}

uint32_t *test_card(uint32_t width, uint32_t height)
{
    uint32_t *texels = static_cast<uint32_t *>(allocate(static_cast<size_t>(width) * height * 4u));
    for (uint32_t y = 0; y < height; y++)
        for (uint32_t x = 0; x < width; x++) {
            const float u = (x + 0.5f) / width, v = (y + 0.5f) / height;
            float3      c;
            if (v < 0.3f) {
                const int bar = static_cast<int>(u * 8.0f);
                c = {static_cast<float>(bar & 1), static_cast<float>((bar >> 1) & 1),
                     static_cast<float>((bar >> 2) & 1)};
            } else {
                const float dx = u - 0.5f, dy = v - 0.65f;
                const float zone = 0.5f + 0.5f * cos(900.0f * (dx * dx + dy * dy));
                c = {zone, zone * 0.8f + 0.2f * u, zone * 0.6f + 0.4f * v};
            }
            if (x % 64 == 0 || y % 64 == 0) c = {1.0f, 1.0f, 1.0f};
            texels[y * width + x] = pack_rgba(c.x, c.y, c.z, 1.0f);
        }
    return texels;
}

class Mandelbrot final : public Sample {
public:
    const char *name() const override { return "mandelbrot"; }
    Work        work() const override { return {static_cast<double>(width) * height / 1e6, "Mpix/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void mandelbrot(__global uchar4 *rgba, float cx, float cy, float scale, uint limit) {
    const int x = get_global_id(0), y = get_global_id(1);
    const int width = get_global_size(0), height = get_global_size(1);
    const float2 c = (float2)(cx + (x - width * 0.5f) * scale, cy + (y - height * 0.5f) * scale);
    float2 z = (float2)(0.0f);
    uint i = 0;
    for (; i < limit && dot(z, z) < 256.0f; i++) z = (float2)(z.x * z.x - z.y * z.y, 2.0f * z.x * z.y) + c;
    float3 color = (float3)(0.0f);
    if (i < limit) {
        const float smooth = (float)i + 1.0f - log2(log2(dot(z, z)) * 0.5f);
        const float t = smooth * 0.035f;
        color = 0.5f + 0.5f * cos(6.2831853f * (t + (float3)(0.0f, 0.15f, 0.3f)));
    }
    rgba[y * width + x] = (uchar4)(convert_uchar_sat_rte(color.x * 255.0f), convert_uchar_sat_rte(color.y * 255.0f),
                                   convert_uchar_sat_rte(color.z * 255.0f), 255);
})");
        kernel_ = gpu.kernel(program_, "mandelbrot");
        rgba_ = gpu.buffer(width * height * 4u);
        gpu.arg(kernel_, 0, rgba_);
        gpu.arg(kernel_, 1, -0.743643887f);
        gpu.arg(kernel_, 2, 0.131825904f);
        gpu.arg(kernel_, 4, 1024u);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { render(gpu, static_cast<float>(frame_++ % 240u) / 60.0f); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        render(gpu, 1.5f);
        capture_rgba_buffer(gpu, capture, rgba_, width, height);
        return "ok";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(rgba_);
        frame_ = 0;
    }

private:
    static constexpr uint32_t width = 1280, height = 720;
    void                      render(Gpu &gpu, float time)
    {
        float scale = 3.0f / height;
        for (float t = 0.0f; t < time; t += 1.0f / 60.0f) scale *= 0.96f;
        gpu.arg(kernel_, 3, scale);
        gpu.run(kernel_, width, height, 16, 16);
    }
    cl_program program_ = nullptr;
    cl_kernel  kernel_ = nullptr;
    cl_mem     rgba_ = nullptr;
    uint32_t   frame_ = 0;
};

class GaussianBlur final : public Sample {
public:
    const char *name() const override { return "gaussianblur"; }
    Work        work() const override { return {2.0 * size * size / 1e6, "Mpix/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__constant sampler_t clamped = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
__kernel void blur(read_only image2d_t source, write_only image2d_t target, __constant float *weights, int radius,
                   int dx, int dy) {
    const int2 p = (int2)(get_global_id(0), get_global_id(1));
    float4 sum = read_imagef(source, clamped, p) * weights[0];
    for (int i = 1; i <= radius; i++) {
        sum += read_imagef(source, clamped, p + (int2)(dx, dy) * i) * weights[i];
        sum += read_imagef(source, clamped, p - (int2)(dx, dy) * i) * weights[i];
    }
    write_imagef(target, p, sum);
})");
        kernel_ = gpu.kernel(program_, "blur");
        uint32_t *card = test_card(size, size);
        source_ = gpu.image2d(size, size, CL_RGBA, CL_UNORM_INT8, card, CL_MEM_READ_ONLY);
        release_memory(card);
        across_ = gpu.image2d(size, size, CL_RGBA, CL_FLOAT);
        blurred_ = gpu.image2d(size, size, CL_RGBA, CL_UNORM_INT8, nullptr, CL_MEM_WRITE_ONLY);
        float  weights[radius + 1] = {};
        double total = 0.0;
        for (int i = 0; i <= radius; i++) {
            weights[i] = static_cast<float>(exp_d(-0.5 * i * i / (sigma * sigma)));
            total += (i == 0 ? 1.0 : 2.0) * weights[i];
        }
        for (float &weight : weights) weight = static_cast<float>(weight / total);
        weights_ = gpu.buffer(sizeof weights, weights, CL_MEM_READ_ONLY);
        gpu.arg(kernel_, 2, weights_);
        gpu.arg(kernel_, 3, static_cast<int32_t>(radius));
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        pass(gpu, source_, across_, 1, 0);
        pass(gpu, across_, blurred_, 0, 1);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        capture_image(gpu, capture, blurred_, size, size);
        return "ok";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(source_);
        samples::release(across_);
        samples::release(blurred_);
        samples::release(weights_);
    }

private:
    static constexpr uint32_t size = 1024;
    static constexpr int      radius = 12;
    static constexpr double   sigma = 5.0;
    void                      pass(Gpu &gpu, cl_mem from, cl_mem to, int32_t dx, int32_t dy)
    {
        gpu.arg(kernel_, 0, from);
        gpu.arg(kernel_, 1, to);
        gpu.arg(kernel_, 4, dx);
        gpu.arg(kernel_, 5, dy);
        gpu.run(kernel_, size, size, 16, 16);
    }
    cl_program program_ = nullptr;
    cl_kernel  kernel_ = nullptr;
    cl_mem     source_ = nullptr, across_ = nullptr, blurred_ = nullptr, weights_ = nullptr;
};

class Convolution final : public Sample {
public:
    const char *name() const override { return "convolution"; }
    Work        work() const override { return {static_cast<double>(size) * size / 1e6, "Mpix/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
#define TILE 16
#define R 2
#define SPAN (TILE + 2 * R)
__kernel void convolve(__global const float *in, __global float *out, __constant float *filter, int n) {
    __local float tile[SPAN][SPAN];
    const int lx = get_local_id(0), ly = get_local_id(1);
    const int gx = get_group_id(0) * TILE, gy = get_group_id(1) * TILE;
    for (int y = ly; y < SPAN; y += TILE)
        for (int x = lx; x < SPAN; x += TILE) {
            const int sx = clamp(gx + x - R, 0, n - 1), sy = clamp(gy + y - R, 0, n - 1);
            tile[y][x] = in[sy * n + sx];
        }
    barrier(CLK_LOCAL_MEM_FENCE);
    float sum = 0.0f;
    for (int y = 0; y <= 2 * R; y++)
        for (int x = 0; x <= 2 * R; x++) sum += tile[ly + y][lx + x] * filter[y * (2 * R + 1) + x];
    out[(gy + ly) * n + gx + lx] = sum;
}
__kernel void show(__global const float *field, __global uchar4 *rgba) {
    const size_t i = get_global_id(0);
    const float v = field[i] * 3.0f;
    const float3 c = v >= 0.0f ? (float3)(1.0f, 0.85f, 0.5f) * v : (float3)(0.4f, 0.7f, 1.0f) * -v;
    rgba[i] = (uchar4)(convert_uchar_sat_rte(c.x * 255.0f), convert_uchar_sat_rte(c.y * 255.0f),
                       convert_uchar_sat_rte(c.z * 255.0f), 255);
})");
        convolve_ = gpu.kernel(program_, "convolve");
        show_ = gpu.kernel(program_, "show");
        float *field = static_cast<float *>(allocate(size * size * 4u));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float u = static_cast<float>(x) / size, v = static_cast<float>(y) / size;
                const float dx = u - 0.5f, dy = v - 0.5f;
                const float disc = dx * dx + dy * dy < 0.12f ? 1.0f : 0.0f;
                field[y * size + x] = disc * (fbm(u * 24.0f, v * 24.0f, 3u, 4) > 0.5f ? 1.0f : 0.3f) +
                                      ((x / 64 + y / 64) % 2 == 0 ? 0.25f : 0.0f);
            }
        in_ = gpu.buffer(size * size * 4u, field, CL_MEM_READ_ONLY);
        release_memory(field);
        const float filter[25] = {0, 0, -1, 0, 0, 0, -1, -2, -1, 0, -1, -2, 16, -2, -1,
                                  0, -1, -2, -1, 0, 0, 0, -1, 0, 0};
        filter_ = gpu.buffer(sizeof filter, filter, CL_MEM_READ_ONLY);
        out_ = gpu.buffer(size * size * 4u);
        rgba_ = gpu.buffer(size * size * 4u);
        gpu.arg(convolve_, 0, in_);
        gpu.arg(convolve_, 1, out_);
        gpu.arg(convolve_, 2, filter_);
        gpu.arg(convolve_, 3, static_cast<int32_t>(size));
        gpu.arg(show_, 0, out_);
        gpu.arg(show_, 1, rgba_);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { gpu.run(convolve_, size, size, 16, 16); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        gpu.run(show_, size * size, 256);
        capture_rgba_buffer(gpu, capture, rgba_, size, size);
        return "ok";
    }
    void release() override
    {
        samples::release(convolve_);
        samples::release(show_);
        samples::release(program_);
        samples::release(in_);
        samples::release(out_);
        samples::release(filter_);
        samples::release(rgba_);
    }

private:
    static constexpr uint32_t size = 2048;
    cl_program                program_ = nullptr;
    cl_kernel                 convolve_ = nullptr, show_ = nullptr;
    cl_mem                    in_ = nullptr, out_ = nullptr, filter_ = nullptr, rgba_ = nullptr;
};

class GameOfLife final : public Sample {
public:
    const char *name() const override { return "gameoflife"; }
    Work        work() const override { return {static_cast<double>(size) * size / 1e6, "Mcell/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void generation(__global const uchar *before, __global uchar *after, int n) {
    const int x = get_global_id(0), y = get_global_id(1);
    int neighbours = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx != 0 || dy != 0) neighbours += before[((y + dy + n) % n) * n + (x + dx + n) % n] != 0;
    const uchar age = before[y * n + x];
    const bool alive = neighbours == 3 || (age != 0 && neighbours == 2);
    after[y * n + x] = alive ? (uchar)min((int)age + 1, 255) : 0;
}
__kernel void show(__global const uchar *cells, __global uchar4 *rgba) {
    const size_t i = get_global_id(0);
    const uchar age = cells[i];
    if (age == 0) { rgba[i] = (uchar4)(8, 10, 24, 255); return; }
    const float t = min(age / 64.0f, 1.0f);
    rgba[i] = (uchar4)(convert_uchar_sat_rte(255.0f * (1.0f - 0.6f * t)), convert_uchar_sat_rte(255.0f * (0.4f + 0.6f * t)),
                       convert_uchar_sat_rte(120.0f + 135.0f * t), 255);
})");
        generation_ = gpu.kernel(program_, "generation");
        show_ = gpu.kernel(program_, "show");
        uint8_t *cells = static_cast<uint8_t *>(allocate(size * size));
        Random   random(0x11FEu);
        for (uint32_t i = 0; i < size * size; i++) cells[i] = (random.next() & 3u) == 0 ? 1 : 0;
        initial_ = gpu.buffer(size * size, cells, CL_MEM_READ_ONLY);
        release_memory(cells);
        cells_[0] = gpu.buffer(size * size);
        cells_[1] = gpu.buffer(size * size);
        rgba_ = gpu.buffer(size * size * 4u);
        gpu.arg(generation_, 2, static_cast<int32_t>(size));
        gpu.arg(show_, 1, rgba_);
        reset(gpu);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { advance(gpu); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        reset(gpu);
        for (uint32_t i = 0; i < capture_generations; i++) {
            advance(gpu);
            gpu.flush();
        }
        gpu.arg(show_, 0, cells_[current_]);
        gpu.run(show_, size * size, 256);
        capture_rgba_buffer(gpu, capture, rgba_, size, size);
        return "ok";
    }
    void release() override
    {
        samples::release(generation_);
        samples::release(show_);
        samples::release(program_);
        samples::release(initial_);
        samples::release(cells_[0]);
        samples::release(cells_[1]);
        samples::release(rgba_);
    }

private:
    static constexpr uint32_t size = 1024;
    static constexpr uint32_t capture_generations = 200;
    void                      reset(Gpu &gpu)
    {
        gpu.check(cl.EnqueueCopyBuffer(gpu.queue, initial_, cells_[0], 0, 0, size * size, 0, nullptr, nullptr),
                  "clEnqueueCopyBuffer");
        current_ = 0;
    }
    void advance(Gpu &gpu)
    {
        gpu.arg(generation_, 0, cells_[current_]);
        gpu.arg(generation_, 1, cells_[1 - current_]);
        gpu.run(generation_, size, size, 16, 16);
        current_ = 1 - current_;
    }
    cl_program program_ = nullptr;
    cl_kernel  generation_ = nullptr, show_ = nullptr;
    cl_mem     initial_ = nullptr, cells_[2] = {}, rgba_ = nullptr;
    uint32_t   current_ = 0;
};

class Heat2d final : public Sample {
public:
    const char *name() const override { return "heat2d"; }
    Work        work() const override { return {static_cast<double>(size) * size / 1e6, "Mcell/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
float source(int x, int y, int n) {
    const float2 p = (float2)(x, y) / n;
    const float2 spots[4] = {(float2)(0.25f, 0.3f), (float2)(0.7f, 0.25f), (float2)(0.4f, 0.75f), (float2)(0.8f, 0.7f)};
    const float heat[4] = {1.0f, 0.7f, 0.85f, -0.6f};
    for (int i = 0; i < 4; i++)
        if (distance(p, spots[i]) < 0.03f) return heat[i];
    return 2.0f;
}
__kernel void relax(__global const float *before, __global float *after, int n) {
    const int x = get_global_id(0), y = get_global_id(1);
    const float held = source(x, y, n);
    if (held <= 1.0f) { after[y * n + x] = held; return; }
    if (x == 0 || y == 0 || x == n - 1 || y == n - 1) { after[y * n + x] = 0.0f; return; }
    after[y * n + x] = 0.25f * (before[y * n + x - 1] + before[y * n + x + 1] + before[(y - 1) * n + x] +
                                before[(y + 1) * n + x]);
}
__kernel void show(__global const float *field, __global uchar4 *rgba) {
    const size_t i = get_global_id(0);
    const float v = clamp(field[i], -1.0f, 1.0f);
    const float3 hot = (float3)(1.0f, 0.35f + 0.6f * v * v, 0.1f), cold = (float3)(0.1f, 0.5f, 1.0f);
    const float3 c = v >= 0.0f ? mix((float3)(0.05f), hot, sqrt(v)) : mix((float3)(0.05f), cold, sqrt(-v));
    rgba[i] = (uchar4)(convert_uchar_sat_rte(c.x * 255.0f), convert_uchar_sat_rte(c.y * 255.0f),
                       convert_uchar_sat_rte(c.z * 255.0f), 255);
})");
        relax_ = gpu.kernel(program_, "relax");
        show_ = gpu.kernel(program_, "show");
        field_[0] = gpu.buffer(size * size * 4u);
        field_[1] = gpu.buffer(size * size * 4u);
        rgba_ = gpu.buffer(size * size * 4u);
        gpu.arg(relax_, 2, static_cast<int32_t>(size));
        gpu.arg(show_, 1, rgba_);
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
        gpu.arg(show_, 0, field_[current_]);
        gpu.run(show_, size * size, 256);
        capture_rgba_buffer(gpu, capture, rgba_, size, size);
        float *field = static_cast<float *>(allocate(size * size * 4u));
        gpu.read(field_[current_], field, size * size * 4u);
        const uint32_t kept = size * size / 16;
        float         *picked = static_cast<float *>(allocate(kept * 4u));
        for (uint32_t i = 0; i < kept; i++) picked[i] = field[i * 16];
        if (!gpu.failed) capture.buffer(Capture::f32, picked, kept);
        release_memory(picked);
        release_memory(field);
        return "ok";
    }
    void release() override
    {
        samples::release(relax_);
        samples::release(show_);
        samples::release(program_);
        samples::release(field_[0]);
        samples::release(field_[1]);
        samples::release(rgba_);
    }

private:
    static constexpr uint32_t size = 1024;
    static constexpr uint32_t capture_steps = 2000;
    void                      reset(Gpu &gpu)
    {
        const float zero = 0.0f;
        for (cl_mem field : field_)
            gpu.check(cl.EnqueueFillBuffer(gpu.queue, field, &zero, 4, 0, size * size * 4u, 0, nullptr, nullptr),
                      "clEnqueueFillBuffer");
        current_ = 0;
    }
    void advance(Gpu &gpu)
    {
        gpu.arg(relax_, 0, field_[current_]);
        gpu.arg(relax_, 1, field_[1 - current_]);
        gpu.run(relax_, size, size, 16, 16);
        current_ = 1 - current_;
    }
    cl_program program_ = nullptr;
    cl_kernel  relax_ = nullptr, show_ = nullptr;
    cl_mem     field_[2] = {}, rgba_ = nullptr;
    uint32_t   current_ = 0;
};

class ImageResample final : public Sample {
public:
    const char *name() const override { return "imageresample"; }
    Work        work() const override { return {static_cast<double>(out_size) * out_size / 1e6, "Mpix/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void resample(read_only image2d_t source, write_only image2d_t target, sampler_t filter, float angle, float zoom) {
    const int2 p = (int2)(get_global_id(0), get_global_id(1));
    const int2 size = get_image_dim(target);
    const float2 centred = ((float2)(p.x, p.y) + 0.5f) / (float2)(size.x, size.y) - 0.5f;
    const float c = cos(angle), s = sin(angle);
    const float2 uv = (float2)(c * centred.x - s * centred.y, s * centred.x + c * centred.y) * zoom + 0.5f;
    write_imagef(target, p, read_imagef(source, filter, uv));
})");
        kernel_ = gpu.kernel(program_, "resample");
        uint32_t *card = test_card(in_size, in_size);
        source_ = gpu.image2d(in_size, in_size, CL_RGBA, CL_UNORM_INT8, card, CL_MEM_READ_ONLY);
        release_memory(card);
        target_ = gpu.image2d(out_size, out_size, CL_RGBA, CL_UNORM_INT8, nullptr, CL_MEM_WRITE_ONLY);
        sampler_ = gpu.sampler(true, CL_ADDRESS_REPEAT, CL_FILTER_LINEAR);
        gpu.arg(kernel_, 0, source_);
        gpu.arg(kernel_, 1, target_);
        gpu.arg(kernel_, 2, sampler_);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { render(gpu, static_cast<float>(frame_++) / 60.0f); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        render(gpu, 1.5f);
        capture_image(gpu, capture, target_, out_size, out_size);
        return "ok";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(source_);
        samples::release(target_);
        samples::release(sampler_);
        frame_ = 0;
    }

private:
    static constexpr uint32_t in_size = 512, out_size = 1536;
    void                      render(Gpu &gpu, float time)
    {
        gpu.arg(kernel_, 3, time * 0.4f);
        gpu.arg(kernel_, 4, 0.9f + 0.5f * sin(time * 0.7f));
        gpu.run(kernel_, out_size, out_size, 16, 16);
    }
    cl_program program_ = nullptr;
    cl_kernel  kernel_ = nullptr;
    cl_mem     source_ = nullptr, target_ = nullptr;
    cl_sampler sampler_ = nullptr;
    uint32_t   frame_ = 0;
};

}

void register_image(Sample **all, uint32_t &made)
{
    all[made++] = new Mandelbrot();
    all[made++] = new GaussianBlur();
    all[made++] = new Convolution();
    all[made++] = new GameOfLife();
    all[made++] = new Heat2d();
    all[made++] = new ImageResample();
}

}

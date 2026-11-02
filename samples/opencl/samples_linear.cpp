#include "framework.h"

namespace samples {

namespace {
template <typename T> T *strided(const T *from, uint32_t count, uint32_t stride, uint32_t &kept)
{
    kept = count / stride;
    T *picked = static_cast<T *>(allocate(sizeof(T) * kept));
    for (uint32_t i = 0; i < kept; i++) picked[i] = from[static_cast<size_t>(i) * stride];
    return picked;
}

bool close_to(double value, double expected, double relative, double absolute)
{
    const double difference = value > expected ? value - expected : expected - value;
    const double magnitude = expected < 0 ? -expected : expected;
    return difference <= absolute || difference <= relative * magnitude;
}

class VectorAdd final : public Sample {
public:
    const char *name() const override { return "vectoradd"; }
    Work        work() const override { return {3.0 * count * 4.0 / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void axpb(__global const float *a, __global const float *b, __global float *c, float alpha) {
    const size_t i = get_global_id(0);
    c[i] = alpha * a[i] + b[i];
})");
        kernel_ = gpu.kernel(program_, "axpb");
        float *a = static_cast<float *>(allocate(count * 4u)), *b = static_cast<float *>(allocate(count * 4u));
        for (uint32_t i = 0; i < count; i++) {
            a[i] = static_cast<float>(mix_bits(i) & 0xFFFFu) / 65536.0f - 0.5f;
            b[i] = static_cast<float>(i % 1000u) * 0.001f;
        }
        a_ = gpu.buffer(count * 4u, a, CL_MEM_READ_ONLY);
        b_ = gpu.buffer(count * 4u, b, CL_MEM_READ_ONLY);
        c_ = gpu.buffer(count * 4u);
        release_memory(a);
        release_memory(b);
        gpu.arg(kernel_, 0, a_);
        gpu.arg(kernel_, 1, b_);
        gpu.arg(kernel_, 2, c_);
        gpu.arg(kernel_, 3, alpha);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { gpu.run(kernel_, count, 256); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        gpu.run(kernel_, count, 256);
        float *c = static_cast<float *>(allocate(count * 4u));
        gpu.read(c_, c, count * 4u);
        bool right = !gpu.failed;
        for (uint32_t i = 0; i < count && right; i++) {
            const float a = static_cast<float>(mix_bits(i) & 0xFFFFu) / 65536.0f - 0.5f;
            const float b = static_cast<float>(i % 1000u) * 0.001f;
            right = close_to(c[i], static_cast<double>(alpha) * a + b, 1e-6, 1e-6);
        }
        uint32_t kept = 0;
        float   *picked = strided(c, count, 4, kept);
        capture.buffer(Capture::f32, picked, kept);
        release_memory(picked);
        release_memory(c);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(a_);
        samples::release(b_);
        samples::release(c_);
    }

private:
    static constexpr uint32_t count = 4u << 20;
    static constexpr float    alpha = 2.5f;
    cl_program                program_ = nullptr;
    cl_kernel                 kernel_ = nullptr;
    cl_mem                    a_ = nullptr, b_ = nullptr, c_ = nullptr;
};

class Reduction final : public Sample {
public:
    const char *name() const override { return "reduction"; }
    Work        work() const override { return {count * 4.0 / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
// Each work-item sums a grid-strided run, then the group sums its items in local memory.
__kernel void reduce(__global const uint *in, __global uint *out, uint n, __local uint *scratch) {
    const uint lid = get_local_id(0), size = get_local_size(0);
    uint sum = 0;
    for (uint i = get_global_id(0); i < n; i += get_global_size(0)) sum += in[i];
    scratch[lid] = sum;
    for (uint stride = size / 2; stride > 0; stride /= 2) {
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lid < stride) scratch[lid] += scratch[lid + stride];
    }
    if (lid == 0) out[get_group_id(0)] = scratch[0];
})");
        first_ = gpu.kernel(program_, "reduce");
        second_ = gpu.kernel(program_, "reduce");
        uint32_t *values = static_cast<uint32_t *>(allocate(count * 4u));
        for (uint32_t i = 0; i < count; i++) values[i] = mix_bits(i * 7u + 3u) & 0x7Fu;
        input_ = gpu.buffer(count * 4u, values, CL_MEM_READ_ONLY);
        release_memory(values);
        partials_ = gpu.buffer(groups * 4u);
        total_ = gpu.buffer(4u);
        gpu.arg(first_, 0, input_);
        gpu.arg(first_, 1, partials_);
        gpu.arg(first_, 2, count);
        gpu.local(first_, 3, group * 4u);
        gpu.arg(second_, 0, partials_);
        gpu.arg(second_, 1, total_);
        gpu.arg(second_, 2, groups);
        gpu.local(second_, 3, group * 4u);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        gpu.run(first_, groups * group, group);
        gpu.run(second_, group, group);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        uint32_t found[groups + 1] = {};
        gpu.read(partials_, found, groups * 4u);
        gpu.read(total_, found + groups, 4u);
        uint32_t expected = 0;
        for (uint32_t i = 0; i < count; i++) expected += mix_bits(i * 7u + 3u) & 0x7Fu;
        capture.buffer(Capture::u32, found, groups + 1);
        return !gpu.failed && found[groups] == expected ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(first_);
        samples::release(second_);
        samples::release(program_);
        samples::release(input_);
        samples::release(partials_);
        samples::release(total_);
    }

private:
    static constexpr uint32_t count = 16u << 20;
    static constexpr uint32_t group = 256;
    static constexpr uint32_t groups = 1024;
    cl_program                program_ = nullptr;
    cl_kernel                 first_ = nullptr, second_ = nullptr;
    cl_mem                    input_ = nullptr, partials_ = nullptr, total_ = nullptr;
};

class PrefixSum final : public Sample {
public:
    const char *name() const override { return "prefixsum"; }
    Work        work() const override { return {count / 1e6, "Melem/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
#define BLOCK 512
__kernel void scan_block(__global const uint *in, __global uint *out, __global uint *sums, uint n,
                         __local uint *tmp) {
    const uint lid = get_local_id(0), group = get_group_id(0);
    const uint a = group * BLOCK + lid, b = a + BLOCK / 2;
    tmp[lid] = a < n ? in[a] : 0;
    tmp[lid + BLOCK / 2] = b < n ? in[b] : 0;
    uint offset = 1;
    for (uint d = BLOCK / 2; d > 0; d >>= 1) {
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lid < d) {
            const uint ai = offset * (2 * lid + 1) - 1, bi = offset * (2 * lid + 2) - 1;
            tmp[bi] += tmp[ai];
        }
        offset <<= 1;
    }
    if (lid == 0) {
        sums[group] = tmp[BLOCK - 1];
        tmp[BLOCK - 1] = 0;
    }
    for (uint d = 1; d < BLOCK; d <<= 1) {
        offset >>= 1;
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lid < d) {
            const uint ai = offset * (2 * lid + 1) - 1, bi = offset * (2 * lid + 2) - 1;
            const uint t = tmp[ai];
            tmp[ai] = tmp[bi];
            tmp[bi] += t;
        }
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    if (a < n) out[a] = tmp[lid];
    if (b < n) out[b] = tmp[lid + BLOCK / 2];
}
__kernel void add_block(__global uint *out, __global const uint *scanned, uint n) {
    const uint group = get_group_id(0), a = group * BLOCK + get_local_id(0), b = a + BLOCK / 2;
    const uint add = scanned[group];
    if (a < n) out[a] += add;
    if (b < n) out[b] += add;
})");
        scan_ = gpu.kernel(program_, "scan_block");
        add_ = gpu.kernel(program_, "add_block");
        uint32_t *values = static_cast<uint32_t *>(allocate(count * 4u));
        for (uint32_t i = 0; i < count; i++) values[i] = mix_bits(i) & 0xFu;
        input_ = gpu.buffer(count * 4u, values, CL_MEM_READ_ONLY);
        release_memory(values);
        uint32_t n = count;
        for (uint32_t level = 0; level < levels; level++) {
            sizes_[level] = n;
            out_[level] = gpu.buffer(static_cast<size_t>(n) * 4u);
            n = (n + block - 1) / block;
            sums_[level] = gpu.buffer(static_cast<size_t>(n) * 4u);
        }
        gpu.local(scan_, 4, block * 4u);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        for (uint32_t level = 0; level < levels; level++) {
            gpu.arg(scan_, 0, level == 0 ? input_ : sums_[level - 1]);
            gpu.arg(scan_, 1, out_[level]);
            gpu.arg(scan_, 2, sums_[level]);
            gpu.arg(scan_, 3, sizes_[level]);
            gpu.run(scan_, blocks(sizes_[level]) * (block / 2), block / 2);
        }
        for (uint32_t level = levels - 1; level > 0; level--) {
            gpu.arg(add_, 0, out_[level - 1]);
            gpu.arg(add_, 1, out_[level]);
            gpu.arg(add_, 2, sizes_[level - 1]);
            gpu.run(add_, blocks(sizes_[level - 1]) * (block / 2), block / 2);
        }
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        uint32_t *found = static_cast<uint32_t *>(allocate(count * 4u));
        gpu.read(out_[0], found, count * 4u);
        bool     right = !gpu.failed;
        uint32_t running = 0;
        for (uint32_t i = 0; i < count && right; i++) {
            right = found[i] == running;
            running += mix_bits(i) & 0xFu;
        }
        uint32_t  kept = 0;
        uint32_t *picked = strided(found, count, 4, kept);
        capture.buffer(Capture::u32, picked, kept);
        release_memory(picked);
        release_memory(found);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(scan_);
        samples::release(add_);
        samples::release(program_);
        samples::release(input_);
        for (uint32_t level = 0; level < levels; level++) {
            samples::release(out_[level]);
            samples::release(sums_[level]);
        }
    }

private:
    static constexpr uint32_t count = 4u << 20;
    static constexpr uint32_t block = 512;
    static constexpr uint32_t levels = 3;
    static size_t             blocks(uint32_t n) { return (n + block - 1) / block; }
    cl_program                program_ = nullptr;
    cl_kernel                 scan_ = nullptr, add_ = nullptr;
    cl_mem                    input_ = nullptr;
    cl_mem                    out_[levels] = {}, sums_[levels] = {};
    uint32_t                  sizes_[levels] = {};
};

const char *const matmul_source = R"(
#define TILE 16
__kernel void naive(__global const float *a, __global const float *b, __global float *c, uint n) {
    const uint col = get_global_id(0), row = get_global_id(1);
    float sum = 0.0f;
    for (uint k = 0; k < n; k++) sum += a[row * n + k] * b[k * n + col];
    c[row * n + col] = sum;
}
__kernel void tiled(__global const float *a, __global const float *b, __global float *c, uint n) {
    __local float ta[TILE][TILE], tb[TILE][TILE];
    const uint lx = get_local_id(0), ly = get_local_id(1);
    const uint col = get_global_id(0), row = get_global_id(1);
    float sum = 0.0f;
    for (uint t = 0; t < n; t += TILE) {
        ta[ly][lx] = a[row * n + t + lx];
        tb[ly][lx] = b[(t + ly) * n + col];
        barrier(CLK_LOCAL_MEM_FENCE);
        for (uint k = 0; k < TILE; k++) sum += ta[ly][k] * tb[k][lx];
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    c[row * n + col] = sum;
})";

class MatMul final : public Sample {
public:
    MatMul(const char *name, const char *kernel) : name_(name), entry_(kernel) {}
    const char *name() const override { return name_; }
    Work        work() const override { return {2.0 * n * n * n / 1e9, "GFLOP/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(matmul_source);
        kernel_ = gpu.kernel(program_, entry_);
        a_host_ = static_cast<float *>(allocate(n * n * 4u));
        b_host_ = static_cast<float *>(allocate(n * n * 4u));
        for (uint32_t i = 0; i < n * n; i++) {
            a_host_[i] = static_cast<float>(mix_bits(i) & 0xFFFu) / 2048.0f - 1.0f;
            b_host_[i] = static_cast<float>(mix_bits(i + 0x9E3779B9u) & 0xFFFu) / 2048.0f - 1.0f;
        }
        a_ = gpu.buffer(n * n * 4u, a_host_, CL_MEM_READ_ONLY);
        b_ = gpu.buffer(n * n * 4u, b_host_, CL_MEM_READ_ONLY);
        c_ = gpu.buffer(n * n * 4u);
        gpu.arg(kernel_, 0, a_);
        gpu.arg(kernel_, 1, b_);
        gpu.arg(kernel_, 2, c_);
        gpu.arg(kernel_, 3, n);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { gpu.run(kernel_, n, n, 16, 16); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        float *c = static_cast<float *>(allocate(n * n * 4u));
        gpu.read(c_, c, n * n * 4u);
        bool right = !gpu.failed;
        for (uint32_t probe = 0; probe < 256 && right; probe++) {
            const uint32_t row = mix_bits(probe) % n, col = mix_bits(probe + 1000u) % n;
            double         expected = 0.0;
            for (uint32_t k = 0; k < n; k++)
                expected += static_cast<double>(a_host_[row * n + k]) * b_host_[k * n + col];
            right = close_to(c[row * n + col], expected, 1e-4, 1e-3);
        }
        capture.buffer(Capture::f32, c, n * n);
        release_memory(c);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(a_);
        samples::release(b_);
        samples::release(c_);
        release_memory(a_host_);
        release_memory(b_host_);
        a_host_ = b_host_ = nullptr;
    }

private:
    static constexpr uint32_t n = 1024;
    const char               *name_;
    const char               *entry_;
    cl_program                program_ = nullptr;
    cl_kernel                 kernel_ = nullptr;
    cl_mem                    a_ = nullptr, b_ = nullptr, c_ = nullptr;
    float                    *a_host_ = nullptr, *b_host_ = nullptr;
};

class Transpose final : public Sample {
public:
    const char *name() const override { return "transpose"; }
    Work        work() const override { return {2.0 * n * n * 4.0 / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
#define TILE 16
__kernel void transpose(__global const float *in, __global float *out, uint n) {
    __local float tile[TILE][TILE + 1];
    const uint lx = get_local_id(0), ly = get_local_id(1);
    const uint gx = get_group_id(0) * TILE, gy = get_group_id(1) * TILE;
    tile[ly][lx] = in[(gy + ly) * n + gx + lx];
    barrier(CLK_LOCAL_MEM_FENCE);
    out[(gx + ly) * n + gy + lx] = tile[lx][ly];
})");
        kernel_ = gpu.kernel(program_, "transpose");
        float *values = static_cast<float *>(allocate(n * n * 4u));
        for (uint32_t i = 0; i < n * n; i++) values[i] = static_cast<float>(i);
        in_ = gpu.buffer(n * n * 4u, values, CL_MEM_READ_ONLY);
        release_memory(values);
        out_ = gpu.buffer(n * n * 4u);
        gpu.arg(kernel_, 0, in_);
        gpu.arg(kernel_, 1, out_);
        gpu.arg(kernel_, 2, n);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { gpu.run(kernel_, n, n, 16, 16); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        float *out = static_cast<float *>(allocate(n * n * 4u));
        gpu.read(out_, out, n * n * 4u);
        bool right = !gpu.failed;
        for (uint32_t y = 0; y < n && right; y++)
            for (uint32_t x = 0; x < n && right; x++) right = out[y * n + x] == static_cast<float>(x * n + y);
        uint32_t kept = 0;
        float   *picked = strided(out, n * n, 4, kept);
        capture.buffer(Capture::f32, picked, kept);
        release_memory(picked);
        release_memory(out);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(in_);
        samples::release(out_);
    }

private:
    static constexpr uint32_t n = 2048;
    cl_program                program_ = nullptr;
    cl_kernel                 kernel_ = nullptr;
    cl_mem                    in_ = nullptr, out_ = nullptr;
};

class Histogram final : public Sample {
public:
    const char *name() const override { return "histogram"; }
    Work        work() const override { return {bytes / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void histogram(__global const uint *in, __global uint *bins, uint words) {
    __local uint local_bins[256];
    const uint lid = get_local_id(0);
    local_bins[lid] = 0;
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint i = get_global_id(0); i < words; i += get_global_size(0)) {
        const uint w = in[i];
        atomic_inc(&local_bins[w & 0xFF]);
        atomic_inc(&local_bins[(w >> 8) & 0xFF]);
        atomic_inc(&local_bins[(w >> 16) & 0xFF]);
        atomic_inc(&local_bins[w >> 24]);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    atomic_add(&bins[lid], local_bins[lid]);
})");
        kernel_ = gpu.kernel(program_, "histogram");
        uint8_t *values = static_cast<uint8_t *>(allocate(bytes));
        Random   random(0xC0FFEEu);
        for (uint32_t i = 0; i < bytes; i++) {
            const uint32_t r = random.next();
            values[i] = static_cast<uint8_t>(((r & 0xFF) + ((r >> 8) & 0xFF) + ((r >> 16) & 0xFF) + (r >> 24)) / 4);
        }
        input_ = gpu.buffer(bytes, values, CL_MEM_READ_ONLY);
        for (uint32_t i = 0; i < bytes; i++) expected_[values[i]]++;
        release_memory(values);
        bins_ = gpu.buffer(256 * 4u);
        const uint32_t words = bytes / 4u;
        gpu.arg(kernel_, 0, input_);
        gpu.arg(kernel_, 1, bins_);
        gpu.arg(kernel_, 2, words);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        const uint32_t zero = 0;
        gpu.check(cl.EnqueueFillBuffer(gpu.queue, bins_, &zero, 4, 0, 256 * 4u, 0, nullptr, nullptr),
                  "clEnqueueFillBuffer");
        gpu.run(kernel_, 256 * 256, 256);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        uint32_t bins[256] = {};
        gpu.read(bins_, bins, sizeof bins);
        bool right = !gpu.failed;
        for (uint32_t i = 0; i < 256; i++) right = right && bins[i] == expected_[i];
        capture.buffer(Capture::u32, bins, 256);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(input_);
        samples::release(bins_);
        for (uint32_t &bin : expected_) bin = 0;
    }

private:
    static constexpr uint32_t bytes = 16u << 20;
    cl_program                program_ = nullptr;
    cl_kernel                 kernel_ = nullptr;
    cl_mem                    input_ = nullptr, bins_ = nullptr;
    uint32_t                  expected_[256] = {};
};

class BitonicSort final : public Sample {
public:
    const char *name() const override { return "bitonicsort"; }
    Work        work() const override { return {count / 1e6, "Mkey/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void bitonic_step(__global uint *keys, uint j, uint k) {
    const uint i = get_global_id(0), partner = i ^ j;
    if (partner <= i) return;
    const uint a = keys[i], b = keys[partner];
    const bool ascending = (i & k) == 0;
    if ((a > b) == ascending) {
        keys[i] = b;
        keys[partner] = a;
    }
})");
        kernel_ = gpu.kernel(program_, "bitonic_step");
        uint32_t *keys = static_cast<uint32_t *>(allocate(count * 4u));
        for (uint32_t i = 0; i < count; i++) {
            keys[i] = mix_bits(i ^ 0xA5A5A5A5u);
            sum_ += keys[i];
            xor_ ^= keys[i];
        }
        original_ = gpu.buffer(count * 4u, keys, CL_MEM_READ_ONLY);
        release_memory(keys);
        keys_ = gpu.buffer(count * 4u);
        gpu.arg(kernel_, 0, keys_);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        gpu.check(cl.EnqueueCopyBuffer(gpu.queue, original_, keys_, 0, 0, count * 4u, 0, nullptr, nullptr),
                  "clEnqueueCopyBuffer");
        for (uint32_t k = 2; k <= count; k <<= 1)
            for (uint32_t j = k >> 1; j > 0; j >>= 1) {
                gpu.arg(kernel_, 1, j);
                gpu.arg(kernel_, 2, k);
                gpu.run(kernel_, count, 256);
            }
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        uint32_t *keys = static_cast<uint32_t *>(allocate(count * 4u));
        gpu.read(keys_, keys, count * 4u);
        bool     right = !gpu.failed;
        uint32_t sum = 0, bits = 0;
        for (uint32_t i = 0; i < count; i++) {
            if (i > 0 && keys[i - 1] > keys[i]) right = false;
            sum += keys[i];
            bits ^= keys[i];
        }
        right = right && sum == sum_ && bits == xor_;
        uint32_t  kept = 0;
        uint32_t *picked = strided(keys, count, 4, kept);
        capture.buffer(Capture::u32, picked, kept);
        release_memory(picked);
        release_memory(keys);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(original_);
        samples::release(keys_);
        sum_ = xor_ = 0;
    }

private:
    static constexpr uint32_t count = 1u << 20;
    cl_program                program_ = nullptr;
    cl_kernel                 kernel_ = nullptr;
    cl_mem                    original_ = nullptr, keys_ = nullptr;
    uint32_t                  sum_ = 0, xor_ = 0;
};

class BlackScholes final : public Sample {
public:
    const char *name() const override { return "blackscholes"; }
    Work        work() const override { return {count / 1e6, "Mopt/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
// The cumulative normal distribution, by the Abramowitz and Stegun polynomial.
float cnd(float d) {
    const float k = 1.0f / (1.0f + 0.2316419f * fabs(d));
    const float poly = k * (0.31938153f + k * (-0.356563782f + k * (1.781477937f + k * (-1.821255978f + k * 1.330274429f))));
    const float c = 1.0f - 0.39894228040143267794f * exp(-0.5f * d * d) * poly;
    return d < 0.0f ? 1.0f - c : c;
}
__kernel void price(__global const float *spot, __global const float *strike, __global const float *years,
                    __global float *call, __global float *put, float rate, float volatility) {
    const size_t i = get_global_id(0);
    const float s = spot[i], x = strike[i], t = years[i];
    const float root = sqrt(t);
    const float d1 = (log(s / x) + (rate + 0.5f * volatility * volatility) * t) / (volatility * root);
    const float d2 = d1 - volatility * root;
    const float discounted = x * exp(-rate * t);
    call[i] = s * cnd(d1) - discounted * cnd(d2);
    put[i] = discounted * cnd(-d2) - s * cnd(-d1);
})");
        kernel_ = gpu.kernel(program_, "price");
        float *spot = static_cast<float *>(allocate(count * 4u));
        float *strike = static_cast<float *>(allocate(count * 4u));
        float *years = static_cast<float *>(allocate(count * 4u));
        Random random(0xB5u);
        for (uint32_t i = 0; i < count; i++) {
            spot[i] = random.range(5.0f, 30.0f);
            strike[i] = random.range(1.0f, 100.0f);
            years[i] = random.range(0.25f, 10.0f);
        }
        spot_ = gpu.buffer(count * 4u, spot, CL_MEM_READ_ONLY);
        strike_ = gpu.buffer(count * 4u, strike, CL_MEM_READ_ONLY);
        years_ = gpu.buffer(count * 4u, years, CL_MEM_READ_ONLY);
        release_memory(spot);
        release_memory(strike);
        release_memory(years);
        call_ = gpu.buffer(count * 4u);
        put_ = gpu.buffer(count * 4u);
        gpu.arg(kernel_, 0, spot_);
        gpu.arg(kernel_, 1, strike_);
        gpu.arg(kernel_, 2, years_);
        gpu.arg(kernel_, 3, call_);
        gpu.arg(kernel_, 4, put_);
        gpu.arg(kernel_, 5, rate);
        gpu.arg(kernel_, 6, volatility);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { gpu.run(kernel_, count, 256); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        float *call = static_cast<float *>(allocate(count * 4u)), *put = static_cast<float *>(allocate(count * 4u));
        gpu.read(call_, call, count * 4u);
        gpu.read(put_, put, count * 4u);
        Random random(0xB5u);
        bool   right = !gpu.failed;
        for (uint32_t i = 0; i < count && right; i++) {
            const double s = random.range(5.0f, 30.0f), x = random.range(1.0f, 100.0f), t = random.range(0.25f, 10.0f);
            right = close_to(static_cast<double>(call[i]) - put[i], s - x * exp_d(-rate * t), 1e-4, 2e-3);
        }
        const uint32_t kept = count / 4;
        float         *pairs = static_cast<float *>(allocate(kept * 8u));
        for (uint32_t i = 0; i < kept; i++) {
            pairs[i * 2] = call[i * 4];
            pairs[i * 2 + 1] = put[i * 4];
        }
        capture.buffer(Capture::f32, pairs, kept * 2);
        release_memory(pairs);
        release_memory(call);
        release_memory(put);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(spot_);
        samples::release(strike_);
        samples::release(years_);
        samples::release(call_);
        samples::release(put_);
    }

private:
    static constexpr uint32_t count = 4u << 20;
    static constexpr float    rate = 0.02f;
    static constexpr float    volatility = 0.30f;
    cl_program                program_ = nullptr;
    cl_kernel                 kernel_ = nullptr;
    cl_mem spot_ = nullptr, strike_ = nullptr, years_ = nullptr, call_ = nullptr, put_ = nullptr;
};

}

void register_linear(Sample **all, uint32_t &made)
{
    all[made++] = new VectorAdd();
    all[made++] = new Reduction();
    all[made++] = new PrefixSum();
    all[made++] = new MatMul("matmul", "naive");
    all[made++] = new MatMul("matmultiled", "tiled");
    all[made++] = new Transpose();
    all[made++] = new Histogram();
    all[made++] = new BitonicSort();
    all[made++] = new BlackScholes();
}

}

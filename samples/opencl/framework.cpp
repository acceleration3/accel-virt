#include "framework.h"

namespace samples {

Cl cl;

// ---------------------------------------------------------------- text

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

void copy_bytes(void *to, const void *from, size_t bytes)
{
    unsigned char       *d = static_cast<unsigned char *>(to);
    const unsigned char *s = static_cast<const unsigned char *>(from);
    for (size_t i = 0; i < bytes; i++) d[i] = s[i];
}

// ---------------------------------------------------------------- entry points

bool Cl::load()
{
    bool all = true;
#define SAMPLES_CL_RESOLVE(name)                                                                                       \
    this->name = reinterpret_cast<decltype(this->name)>(cl_entry("cl" #name));                                      \
    if (this->name == nullptr) {                                                                                       \
        log("no entry point cl" #name "\n");                                                                           \
        all = false;                                                                                                   \
    }
    SAMPLES_CL_ENTRIES(SAMPLES_CL_RESOLVE)
#undef SAMPLES_CL_RESOLVE
    return all;
}

// ---------------------------------------------------------------- the device

bool Gpu::open(uint32_t index)
{
    cl_platform_id platforms[8] = {};
    cl_uint        platform_count = 0;
    if (!check(cl.GetPlatformIDs(8, platforms, &platform_count), "clGetPlatformIDs")) return false;
    // The index counts GPUs across every platform, in the order they are listed.
    uint32_t seen = 0;
    for (cl_uint p = 0; p < platform_count && device == nullptr; p++) {
        cl_device_id devices[16] = {};
        cl_uint      count = 0;
        if (cl.GetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, 16, devices, &count) != CL_SUCCESS) continue;
        if (index < seen + count) {
            platform = platforms[p];
            device = devices[index - seen];
        }
        seen += count;
    }
    if (device == nullptr) {
        log("no OpenCL GPU at that index\n");
        failed = true;
        return false;
    }
    size_t named = 0;
    check(cl.GetDeviceInfo(device, CL_DEVICE_NAME, sizeof device_name - 1, device_name, &named), "CL_DEVICE_NAME");
    const cl_context_properties properties[] = {CL_CONTEXT_PLATFORM,
                                                reinterpret_cast<cl_context_properties>(platform), 0};
    cl_int error = CL_SUCCESS;
    context = cl.CreateContext(properties, 1, &device, nullptr, nullptr, &error);
    if (!check(error, "clCreateContext")) return false;
    queue = cl.CreateCommandQueueWithProperties(context, device, nullptr, &error);
    return check(error, "clCreateCommandQueueWithProperties");
}

void Gpu::close()
{
    if (queue != nullptr) cl.ReleaseCommandQueue(queue);
    if (context != nullptr) cl.ReleaseContext(context);
    queue = nullptr;
    context = nullptr;
}

bool Gpu::check(cl_int status, const char *what)
{
    if (status == CL_SUCCESS) return true;
    if (!failed) {
        char line[256] = {};
        copy_text(line, sizeof line, what);
        append_text(line, sizeof line, " failed: ");
        append_number(line, sizeof line, status, 0);
        append_text(line, sizeof line, "\n");
        log(line);
    }
    failed = true;
    return false;
}

cl_program Gpu::program(const char *source, const char *options)
{
    if (failed) return nullptr;
    cl_int     error = CL_SUCCESS;
    cl_program made = cl.CreateProgramWithSource(context, 1, &source, nullptr, &error);
    if (!check(error, "clCreateProgramWithSource")) return nullptr;
    const cl_int built = cl.BuildProgram(made, 1, &device, options, nullptr, nullptr);
    if (built != CL_SUCCESS) {
        size_t bytes = 0;
        cl.GetProgramBuildInfo(made, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &bytes);
        char *text = static_cast<char *>(allocate(bytes + 1));
        if (cl.GetProgramBuildInfo(made, device, CL_PROGRAM_BUILD_LOG, bytes, text, nullptr) == CL_SUCCESS) {
            log(text);
            log("\n");
        }
        release_memory(text);
        check(built, "clBuildProgram");
        cl.ReleaseProgram(made);
        return nullptr;
    }
    return made;
}

cl_kernel Gpu::kernel(cl_program program, const char *name)
{
    if (failed || program == nullptr) return nullptr;
    cl_int    error = CL_SUCCESS;
    cl_kernel made = cl.CreateKernel(program, name, &error);
    check(error, name);
    return made;
}

cl_mem Gpu::buffer(size_t bytes, const void *initial, cl_mem_flags flags)
{
    if (failed) return nullptr;
    cl_int error = CL_SUCCESS;
    cl_mem made = cl.CreateBuffer(context, flags, bytes, nullptr, &error);
    if (!check(error, "clCreateBuffer")) return nullptr;
    if (initial != nullptr) write(made, initial, bytes);
    return made;
}

cl_mem Gpu::image2d(uint32_t width, uint32_t height, cl_channel_order order, cl_channel_type type,
                    const void *initial, cl_mem_flags flags)
{
    if (failed) return nullptr;
    cl_image_format format = {order, type};
    cl_image_desc   desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;
    cl_int error = CL_SUCCESS;
    cl_mem made = cl.CreateImage(context, flags, &format, &desc, nullptr, &error);
    if (!check(error, "clCreateImage")) return nullptr;
    if (initial != nullptr) write_image(made, initial, width, height);
    return made;
}

cl_sampler Gpu::sampler(bool normalized, cl_addressing_mode address, cl_filter_mode filter)
{
    if (failed) return nullptr;
    const cl_sampler_properties properties[] = {CL_SAMPLER_NORMALIZED_COORDS,
                                                static_cast<cl_sampler_properties>(normalized ? CL_TRUE : CL_FALSE),
                                                CL_SAMPLER_ADDRESSING_MODE, address,
                                                CL_SAMPLER_FILTER_MODE, filter, 0};
    cl_int     error = CL_SUCCESS;
    cl_sampler made = cl.CreateSamplerWithProperties(context, properties, &error);
    check(error, "clCreateSamplerWithProperties");
    return made;
}

void Gpu::write(cl_mem buffer, const void *from, size_t bytes, size_t offset)
{
    if (failed) return;
    check(cl.EnqueueWriteBuffer(queue, buffer, CL_TRUE, offset, bytes, from, 0, nullptr, nullptr),
          "clEnqueueWriteBuffer");
}

void Gpu::read(cl_mem buffer, void *into, size_t bytes, size_t offset)
{
    if (failed) return;
    check(cl.EnqueueReadBuffer(queue, buffer, CL_TRUE, offset, bytes, into, 0, nullptr, nullptr),
          "clEnqueueReadBuffer");
}

void Gpu::write_image(cl_mem image, const void *from, uint32_t width, uint32_t height)
{
    if (failed) return;
    const size_t origin[3] = {0, 0, 0}, region[3] = {width, height, 1};
    check(cl.EnqueueWriteImage(queue, image, CL_TRUE, origin, region, 0, 0, from, 0, nullptr, nullptr),
          "clEnqueueWriteImage");
}

void Gpu::read_image(cl_mem image, void *into, uint32_t width, uint32_t height)
{
    if (failed) return;
    const size_t origin[3] = {0, 0, 0}, region[3] = {width, height, 1};
    check(cl.EnqueueReadImage(queue, image, CL_TRUE, origin, region, 0, 0, into, 0, nullptr, nullptr),
          "clEnqueueReadImage");
}

void Gpu::local(cl_kernel kernel, cl_uint index, size_t bytes)
{
    if (!failed) check(cl.SetKernelArg(kernel, index, bytes, nullptr), "clSetKernelArg (local)");
}

void Gpu::run(cl_kernel kernel, size_t global_x, size_t local_x)
{
    if (failed) return;
    check(cl.EnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_x, local_x != 0 ? &local_x : nullptr, 0,
                                  nullptr, nullptr),
          "clEnqueueNDRangeKernel");
}

void Gpu::run(cl_kernel kernel, size_t global_x, size_t global_y, size_t local_x, size_t local_y)
{
    if (failed) return;
    const size_t global[2] = {global_x, global_y}, local[2] = {local_x, local_y};
    check(cl.EnqueueNDRangeKernel(queue, kernel, 2, nullptr, global, local_x != 0 ? local : nullptr, 0, nullptr,
                                  nullptr),
          "clEnqueueNDRangeKernel");
}

void Gpu::finish()
{
    if (queue != nullptr) check(cl.Finish(queue), "clFinish");
}

// ---------------------------------------------------------------- capture

void Capture::path(char *into, size_t capacity, const char *extension) const
{
    copy_text(into, capacity, directory);
    append_text(into, capacity, "/");
    append_text(into, capacity, sample);
    append_text(into, capacity, extension);
}

void Capture::image_rgba(const uint8_t *rgba, uint32_t width, uint32_t height)
{
    const size_t   texels = static_cast<size_t>(width) * height;
    const size_t   bytes = 12u + texels * 4u;
    uint8_t       *file = static_cast<uint8_t *>(allocate(bytes));
    const uint32_t header[3] = {0x41524742u, width, height};   // "BGRA"
    copy_bytes(file, header, sizeof header);
    for (size_t i = 0; i < texels; i++) {
        file[12 + i * 4 + 0] = rgba[i * 4 + 2];
        file[12 + i * 4 + 1] = rgba[i * 4 + 1];
        file[12 + i * 4 + 2] = rgba[i * 4 + 0];
        file[12 + i * 4 + 3] = rgba[i * 4 + 3];
    }
    char where[512] = {};
    path(where, sizeof where, ".bgra");
    if (!write_file(where, file, bytes)) failed = true;
    release_memory(file);
}

void Capture::buffer(Element type, const void *elements, uint32_t count)
{
    const size_t   bytes = 12u + static_cast<size_t>(count) * 4u;
    uint8_t       *file = static_cast<uint8_t *>(allocate(bytes));
    const uint32_t header[3] = {0x424B5652u, type, count};   // "RVKB"
    copy_bytes(file, header, sizeof header);
    copy_bytes(file + 12, elements, static_cast<size_t>(count) * 4u);
    char where[512] = {};
    path(where, sizeof where, ".buf");
    if (!write_file(where, file, bytes)) failed = true;
    release_memory(file);
}

// ---------------------------------------------------------------- content

float smooth(float a, float b, float x)
{
    const float t = clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

namespace {

float lattice(int x, int y, uint32_t seed)
{
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

float value_noise(float x, float y, uint32_t seed)
{
    const float fx = floor(x), fy = floor(y);
    const int   ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    const float tx = x - fx, ty = y - fy;
    const float sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    const float a = lattice(ix, iy, seed), b = lattice(ix + 1, iy, seed);
    const float c = lattice(ix, iy + 1, seed), d = lattice(ix + 1, iy + 1, seed);
    return mix(mix(a, b, sx), mix(c, d, sx), sy);
}

}  // namespace

float fbm(float x, float y, uint32_t seed, int octaves)
{
    float sum = 0, amplitude = 0.5f, total = 0;
    for (int o = 0; o < octaves; o++) {
        sum += value_noise(x, y, seed + static_cast<uint32_t>(o) * 31u) * amplitude;
        total += amplitude;
        x *= 2.0f;
        y *= 2.0f;
        amplitude *= 0.5f;
    }
    return sum / total;
}

uint32_t pack_rgba(float r, float g, float b, float a)
{
    auto byte = [](float v) { return static_cast<uint32_t>(clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return byte(r) | (byte(g) << 8u) | (byte(b) << 16u) | (byte(a) << 24u);
}

uint32_t ramp_rgba(float t)
{
    const float3 keys[5] = {{0.02f, 0.02f, 0.08f}, {0.35f, 0.05f, 0.55f}, {0.9f, 0.25f, 0.2f},
                            {1.0f, 0.8f, 0.2f}, {1.0f, 1.0f, 0.95f}};
    const float  x = clamp(t, 0.0f, 1.0f) * 4.0f;
    const int    k = x >= 4.0f ? 3 : static_cast<int>(x);
    const float  f = x - static_cast<float>(k);
    return pack_rgba(mix(keys[k].x, keys[k + 1].x, f), mix(keys[k].y, keys[k + 1].y, f),
                     mix(keys[k].z, keys[k + 1].z, f), 1.0f);
}

uint32_t mix_bits(uint32_t x)
{
    x ^= x >> 16u;
    x *= 0x7FEB352Du;
    x ^= x >> 15u;
    x *= 0x846CA68Bu;
    x ^= x >> 16u;
    return x;
}

namespace {
constexpr double ln2 = 0.69314718055994530942;
}

double exp_d(double x)
{
    if (x < -700.0) return 0.0;
    if (x > 700.0) x = 700.0;
    const double scaled = x / ln2;
    const long long k = static_cast<long long>(scaled < 0 ? scaled - 0.5 : scaled + 0.5);
    const double r = x - static_cast<double>(k) * ln2;
    double term = 1.0, sum = 1.0;
    for (int n = 1; n < 18; n++) {
        term *= r / n;
        sum += term;
    }
    const uint64_t bits = static_cast<uint64_t>(k + 1023) << 52u;
    double scale = 0.0;
    copy_bytes(&scale, &bits, sizeof scale);
    return sum * scale;
}

double log_d(double x)
{
    if (x <= 0.0) return -1e300;
    uint64_t bits = 0;
    copy_bytes(&bits, &x, sizeof bits);
    long long exponent = static_cast<long long>((bits >> 52u) & 0x7FFu) - 1023;
    bits = (bits & 0x000FFFFFFFFFFFFFull) | (1023ull << 52u);
    double m = 0.0;
    copy_bytes(&m, &bits, sizeof m);
    if (m > 1.41421356237309504880) {
        m *= 0.5;
        exponent++;
    }
    const double s = (m - 1.0) / (m + 1.0), s2 = s * s;
    double power = s, sum = 0.0;
    for (int n = 0; n < 16; n++) {
        sum += power / (2 * n + 1);
        power *= s2;
    }
    return static_cast<double>(exponent) * ln2 + 2.0 * sum;
}

}  // namespace samples

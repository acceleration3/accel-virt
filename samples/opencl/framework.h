// The OpenCL samples' framework: what a sample is, what it is handed, and the
// few services the two platforms give it. The host build (Linux) runs on the
// host's own OpenCL; the guest build (Windows, no C runtime) runs through the
// Khronos loader and the repository's ICD. Both resolve every entry point at
// run time, so neither links an import library and both run the same kernels.
#pragma once

#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 300
#endif
#include <CL/cl.h>
#include <stddef.h>
#include <stdint.h>

#include "../d3d11/smath.h"

namespace samples {

using sm::pi; using sm::sqrt; using sm::abs; using sm::floor; using sm::fract; using sm::sin; using sm::cos;
using sm::min; using sm::max; using sm::clamp; using sm::mix; using sm::Random;
using sm::float2; using sm::float3; using sm::float4; using sm::dot; using sm::length; using sm::normalize;

// ---- platform (guest_platform.cpp / host_platform.cpp)
void   log(const char *text);
void   log_number(const char *label, double value);
void  *allocate(size_t bytes);   // zeroed
void   release_memory(void *memory);
double seconds();
bool   write_file(const char *path, const void *data, size_t size);
// The address of an OpenCL entry point in the platform's loader, or null.
void  *cl_entry(const char *name);

// ---- text
void copy_text(char *to, size_t capacity, const char *from);
void append_text(char *to, size_t capacity, const char *from);
void append_number(char *to, size_t capacity, double value, int decimals);
bool same_text(const char *a, const char *b);
void copy_bytes(void *to, const void *from, size_t bytes);

// ---- the OpenCL entry points the samples call, resolved once
struct Cl {
#define SAMPLES_CL_ENTRIES(X)                                                                                         \
    X(GetPlatformIDs) X(GetPlatformInfo) X(GetDeviceIDs) X(GetDeviceInfo) X(CreateContext) X(ReleaseContext)          \
    X(CreateCommandQueueWithProperties) X(ReleaseCommandQueue) X(CreateBuffer) X(CreateImage) X(ReleaseMemObject)     \
    X(CreateSamplerWithProperties) X(ReleaseSampler) X(CreateProgramWithSource) X(BuildProgram)                       \
    X(GetProgramBuildInfo) X(ReleaseProgram) X(CreateKernel) X(ReleaseKernel) X(SetKernelArg)                         \
    X(EnqueueNDRangeKernel) X(EnqueueReadBuffer) X(EnqueueWriteBuffer) X(EnqueueCopyBuffer) X(EnqueueFillBuffer)      \
    X(EnqueueReadImage) X(EnqueueWriteImage) X(EnqueueCopyImage) X(EnqueueMapBuffer) X(EnqueueUnmapMemObject)         \
    X(Finish) X(Flush) X(ReleaseEvent) X(EnqueueReadBufferRect) X(EnqueueWriteBufferRect) X(SetEventCallback)
#define SAMPLES_CL_MEMBER(name) decltype(&::cl##name) name;
    SAMPLES_CL_ENTRIES(SAMPLES_CL_MEMBER)
#undef SAMPLES_CL_MEMBER
    bool load();
};
extern Cl cl;

// ---- the device, and what a sample asks of it
class Gpu {
public:
    cl_platform_id   platform = nullptr;
    cl_device_id     device = nullptr;
    cl_context       context = nullptr;
    cl_command_queue queue = nullptr;
    char             device_name[256] = {};
    // Set by the first call that failed; the runner reports it.
    bool             failed = false;

    bool open(uint32_t index);
    void close();

    // Logs `what` and marks the run failed unless `status` is CL_SUCCESS.
    bool check(cl_int status, const char *what);

    // A program built from source, its build log printed if the build fails.
    cl_program program(const char *source, const char *options = nullptr);
    cl_kernel  kernel(cl_program program, const char *name);

    // A buffer, filled from `initial` with a blocking write when one is given.
    cl_mem buffer(size_t bytes, const void *initial = nullptr, cl_mem_flags flags = CL_MEM_READ_WRITE);
    // A 2D image of `order` and `type`, filled from tightly packed `initial` rows.
    cl_mem image2d(uint32_t width, uint32_t height, cl_channel_order order, cl_channel_type type,
                   const void *initial = nullptr, cl_mem_flags flags = CL_MEM_READ_WRITE);
    cl_sampler sampler(bool normalized, cl_addressing_mode address, cl_filter_mode filter);

    void write(cl_mem buffer, const void *from, size_t bytes, size_t offset = 0);
    void read(cl_mem buffer, void *into, size_t bytes, size_t offset = 0);
    void write_image(cl_mem image, const void *from, uint32_t width, uint32_t height);
    void read_image(cl_mem image, void *into, uint32_t width, uint32_t height);

    template <typename T> void arg(cl_kernel kernel, cl_uint index, const T &value)
    {
        if (!failed) check(cl.SetKernelArg(kernel, index, sizeof value, &value), "clSetKernelArg");
    }
    // A __local argument of `bytes`.
    void local(cl_kernel kernel, cl_uint index, size_t bytes);
    void run(cl_kernel kernel, size_t global_x, size_t local_x = 0);
    void run(cl_kernel kernel, size_t global_x, size_t global_y, size_t local_x, size_t local_y);
    void finish();
};

template <typename T> void release_object(T &object, cl_int(CL_API_CALL *release)(T))
{
    if (object != nullptr) release(object);
    object = nullptr;
}
inline void release(cl_mem &object) { release_object(object, cl.ReleaseMemObject); }
inline void release(cl_kernel &object) { release_object(object, cl.ReleaseKernel); }
inline void release(cl_program &object) { release_object(object, cl.ReleaseProgram); }
inline void release(cl_sampler &object) { release_object(object, cl.ReleaseSampler); }

// ---- what a sample leaves behind to be compared
// An image as B8G8R8A8 rows ("BGRA", width, height, texels) - the D3D11 suite's
// frame format - and a buffer as "AVB", element type, count, elements.
class Capture {
public:
    enum Element : uint32_t { u32 = 1, f32 = 2, i32 = 3 };

    const char *directory = ".";
    const char *sample = "";
    bool        failed = false;

    // Rows of R8G8B8A8, as a CL_RGBA/CL_UNORM_INT8 image reads back.
    void image_rgba(const uint8_t *rgba, uint32_t width, uint32_t height);
    void buffer(Element type, const void *elements, uint32_t count);

private:
    void path(char *into, size_t capacity, const char *extension) const;
};

// What one iteration does, for the report's rate column: `amount` of `unit`.
struct Work {
    double      amount = 0.0;
    const char *unit = "";
};

// ---- samples
class Sample {
public:
    virtual const char *name() const = 0;
    virtual bool        init(Gpu &gpu) = 0;
    // One iteration, enqueued; the runner finishes the queue per block.
    virtual void        step(Gpu &gpu) = 0;
    // Starts again from the sample's initial state, runs what the comparison
    // is made on, reads it back into `capture` and checks it where the answer
    // is known: "ok", or "mismatch" when the device computed something else.
    virtual const char *capture(Gpu &gpu, Capture &capture) = 0;
    virtual Work        work() const = 0;
    virtual void        release() = 0;
};

// Every sample, in the report's order.
Sample **registry(uint32_t &count);

// The runner (runner.cpp), given the program's arguments as narrow text.
int run(int argc, const char *const *argv);

// ---- shared CPU-side content
float    smooth(float a, float b, float x);
float    fbm(float x, float y, uint32_t seed, int octaves);
uint32_t pack_rgba(float r, float g, float b, float a);
// A colour for `t` in [0, 1] along a fixed ramp, as packed RGBA.
uint32_t ramp_rgba(float t);
// A well-mixed 32-bit value from `x`: the inputs both machines generate.
uint32_t mix_bits(uint32_t x);
// e^x and ln x in double precision, for the CPU references (the guest has no
// C runtime to take them from).
double exp_d(double x);
double log_d(double x);

}  // namespace samples

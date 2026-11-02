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

void   log(const char *text);
void   log_number(const char *label, double value);
void  *allocate(size_t bytes);
void   release_memory(void *memory);
double seconds();
bool   write_file(const char *path, const void *data, size_t size);
void  *cl_entry(const char *name);

void copy_text(char *to, size_t capacity, const char *from);
void append_text(char *to, size_t capacity, const char *from);
void append_number(char *to, size_t capacity, double value, int decimals);
bool same_text(const char *a, const char *b);
void copy_bytes(void *to, const void *from, size_t bytes);

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

class Gpu {
public:
    cl_platform_id   platform = nullptr;
    cl_device_id     device = nullptr;
    cl_context       context = nullptr;
    cl_command_queue queue = nullptr;
    char             device_name[256] = {};
    bool             failed = false;

    bool open(uint32_t index);
    void close();

    bool check(cl_int status, const char *what);

    cl_program program(const char *source, const char *options = nullptr);
    cl_kernel  kernel(cl_program program, const char *name);

    cl_mem buffer(size_t bytes, const void *initial = nullptr, cl_mem_flags flags = CL_MEM_READ_WRITE);
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
    void local(cl_kernel kernel, cl_uint index, size_t bytes);
    void run(cl_kernel kernel, size_t global_x, size_t local_x = 0);
    void run(cl_kernel kernel, size_t global_x, size_t global_y, size_t local_x, size_t local_y);
    void finish();
    void flush();
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

class Capture {
public:
    enum Element : uint32_t { u32 = 1, f32 = 2, i32 = 3 };

    const char *directory = ".";
    const char *sample = "";
    bool        failed = false;

    void image_rgba(const uint8_t *rgba, uint32_t width, uint32_t height);
    void buffer(Element type, const void *elements, uint32_t count);

private:
    void path(char *into, size_t capacity, const char *extension) const;
};

struct Work {
    double      amount = 0.0;
    const char *unit = "";
};

class Sample {
public:
    virtual const char *name() const = 0;
    virtual bool        init(Gpu &gpu) = 0;
    virtual void        step(Gpu &gpu) = 0;
    virtual const char *capture(Gpu &gpu, Capture &capture) = 0;
    virtual Work        work() const = 0;
    virtual void        release() = 0;
};

Sample **registry(uint32_t &count);

int run(int argc, const char *const *argv);

float    smooth(float a, float b, float x);
float    fbm(float x, float y, uint32_t seed, int octaves);
uint32_t pack_rgba(float r, float g, float b, float a);
uint32_t ramp_rgba(float t);
uint32_t mix_bits(uint32_t x);
double exp_d(double x);
double log_d(double x);

}

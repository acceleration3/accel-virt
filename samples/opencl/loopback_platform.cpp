// The loopback build's platform: the guest's whole OpenCL path in this one Linux
// process - avcl's core, the driver, the device core and the executor library
// on this machine's GPU, the L4 rig the full-stack tests run on
// (tests/harness/fullstack) - so the suite runs through the ICD with no VM. What
// it measures is the rig's speed, which is sanitized and single-threaded, not
// the VM's; what it is for is its outputs, which must match the host's.
#include "framework.h"

#include "harness/executor/executor_library_file.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_d3d_adapter_source.hpp"
#include "harness/icd/scripted_gl_share_source.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"

#include <CL/cl_icd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// The executor's drivers never give everything back; this run judges outputs, not leaks.
extern "C" const char *__asan_default_options(void) { return "detect_leaks=0"; }

namespace samples {

namespace {

av::clicd::ClIcdRoot *icd = nullptr;

const cl_icd_dispatch &table(const void *object) { return **reinterpret_cast<const cl_icd_dispatch *const *>(object); }

// What the Khronos loader does with each call: the ICD's own enumeration for
// platforms, and the table in the first argument's first word for the rest.
cl_int CL_API_CALL get_platform_ids(cl_uint count, cl_platform_id *platforms, cl_uint *found)
{
    return icd->platform_ids(count, platforms, found);
}
cl_int CL_API_CALL get_platform_info(cl_platform_id p, cl_platform_info n, size_t s, void *v, size_t *r)
{
    return table(p).clGetPlatformInfo(p, n, s, v, r);
}
cl_int CL_API_CALL get_device_ids(cl_platform_id p, cl_device_type t, cl_uint n, cl_device_id *d, cl_uint *f)
{
    return table(p).clGetDeviceIDs(p, t, n, d, f);
}
cl_int CL_API_CALL get_device_info(cl_device_id d, cl_device_info n, size_t s, void *v, size_t *r)
{
    return table(d).clGetDeviceInfo(d, n, s, v, r);
}
cl_context CL_API_CALL create_context(const cl_context_properties *p, cl_uint n, const cl_device_id *d,
                                      void(CL_CALLBACK *notify)(const char *, const void *, size_t, void *),
                                      void *user, cl_int *e)
{
    return table(d[0]).clCreateContext(p, n, d, notify, user, e);
}
cl_int CL_API_CALL release_context(cl_context c) { return table(c).clReleaseContext(c); }
cl_command_queue CL_API_CALL create_queue(cl_context c, cl_device_id d, const cl_queue_properties *p, cl_int *e)
{
    return table(c).clCreateCommandQueueWithProperties(c, d, p, e);
}
cl_int CL_API_CALL release_queue(cl_command_queue q) { return table(q).clReleaseCommandQueue(q); }
cl_mem CL_API_CALL create_buffer(cl_context c, cl_mem_flags f, size_t s, void *h, cl_int *e)
{
    return table(c).clCreateBuffer(c, f, s, h, e);
}
cl_mem CL_API_CALL create_image(cl_context c, cl_mem_flags f, const cl_image_format *fo, const cl_image_desc *d,
                                void *h, cl_int *e)
{
    return table(c).clCreateImage(c, f, fo, d, h, e);
}
cl_int CL_API_CALL release_mem(cl_mem m) { return table(m).clReleaseMemObject(m); }
cl_sampler CL_API_CALL create_sampler(cl_context c, const cl_sampler_properties *p, cl_int *e)
{
    return table(c).clCreateSamplerWithProperties(c, p, e);
}
cl_int CL_API_CALL release_sampler(cl_sampler s) { return table(s).clReleaseSampler(s); }
cl_program CL_API_CALL create_program(cl_context c, cl_uint n, const char **s, const size_t *l, cl_int *e)
{
    return table(c).clCreateProgramWithSource(c, n, s, l, e);
}
cl_int CL_API_CALL build_program(cl_program p, cl_uint n, const cl_device_id *d, const char *o,
                                 void(CL_CALLBACK *notify)(cl_program, void *), void *user)
{
    return table(p).clBuildProgram(p, n, d, o, notify, user);
}
cl_int CL_API_CALL build_info(cl_program p, cl_device_id d, cl_program_build_info n, size_t s, void *v, size_t *r)
{
    return table(p).clGetProgramBuildInfo(p, d, n, s, v, r);
}
cl_int CL_API_CALL release_program(cl_program p) { return table(p).clReleaseProgram(p); }
cl_kernel CL_API_CALL create_kernel(cl_program p, const char *n, cl_int *e) { return table(p).clCreateKernel(p, n, e); }
cl_int CL_API_CALL release_kernel(cl_kernel k) { return table(k).clReleaseKernel(k); }
cl_int CL_API_CALL set_arg(cl_kernel k, cl_uint i, size_t s, const void *v) { return table(k).clSetKernelArg(k, i, s, v); }
cl_int CL_API_CALL ndrange(cl_command_queue q, cl_kernel k, cl_uint d, const size_t *o, const size_t *g,
                           const size_t *l, cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueNDRangeKernel(q, k, d, o, g, l, n, w, e);
}
cl_int CL_API_CALL read_buffer(cl_command_queue q, cl_mem m, cl_bool b, size_t o, size_t s, void *p, cl_uint n,
                               const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueReadBuffer(q, m, b, o, s, p, n, w, e);
}
cl_int CL_API_CALL write_buffer(cl_command_queue q, cl_mem m, cl_bool b, size_t o, size_t s, const void *p,
                                cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueWriteBuffer(q, m, b, o, s, p, n, w, e);
}
cl_int CL_API_CALL copy_buffer(cl_command_queue q, cl_mem a, cl_mem b, size_t ao, size_t bo, size_t s, cl_uint n,
                               const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueCopyBuffer(q, a, b, ao, bo, s, n, w, e);
}
cl_int CL_API_CALL fill_buffer(cl_command_queue q, cl_mem m, const void *p, size_t ps, size_t o, size_t s,
                               cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueFillBuffer(q, m, p, ps, o, s, n, w, e);
}
cl_int CL_API_CALL read_rect(cl_command_queue q, cl_mem m, cl_bool b, const size_t *bo, const size_t *ho,
                             const size_t *r, size_t brp, size_t bsp, size_t hrp, size_t hsp, void *p, cl_uint n,
                             const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueReadBufferRect(q, m, b, bo, ho, r, brp, bsp, hrp, hsp, p, n, w, e);
}
cl_int CL_API_CALL write_rect(cl_command_queue q, cl_mem m, cl_bool b, const size_t *bo, const size_t *ho,
                              const size_t *r, size_t brp, size_t bsp, size_t hrp, size_t hsp, const void *p,
                              cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueWriteBufferRect(q, m, b, bo, ho, r, brp, bsp, hrp, hsp, p, n, w, e);
}
cl_int CL_API_CALL set_callback(cl_event e, cl_int status, void(CL_CALLBACK *notify)(cl_event, cl_int, void *),
                                void *data)
{
    return table(e).clSetEventCallback(e, status, notify, data);
}
cl_int CL_API_CALL read_image(cl_command_queue q, cl_mem m, cl_bool b, const size_t *o, const size_t *r, size_t rp,
                              size_t sp, void *p, cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueReadImage(q, m, b, o, r, rp, sp, p, n, w, e);
}
cl_int CL_API_CALL write_image(cl_command_queue q, cl_mem m, cl_bool b, const size_t *o, const size_t *r,
                               size_t rp, size_t sp, const void *p, cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueWriteImage(q, m, b, o, r, rp, sp, p, n, w, e);
}
cl_int CL_API_CALL copy_image(cl_command_queue q, cl_mem a, cl_mem b, const size_t *ao, const size_t *bo,
                              const size_t *r, cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueCopyImage(q, a, b, ao, bo, r, n, w, e);
}
void *CL_API_CALL map_buffer(cl_command_queue q, cl_mem m, cl_bool b, cl_map_flags f, size_t o, size_t s,
                             cl_uint n, const cl_event *w, cl_event *e, cl_int *x)
{
    return table(q).clEnqueueMapBuffer(q, m, b, f, o, s, n, w, e, x);
}
cl_int CL_API_CALL unmap(cl_command_queue q, cl_mem m, void *p, cl_uint n, const cl_event *w, cl_event *e)
{
    return table(q).clEnqueueUnmapMemObject(q, m, p, n, w, e);
}
cl_int CL_API_CALL finish(cl_command_queue q) { return table(q).clFinish(q); }
cl_int CL_API_CALL flush(cl_command_queue q) { return table(q).clFlush(q); }
cl_int CL_API_CALL release_event(cl_event e) { return table(e).clReleaseEvent(e); }

struct Entry {
    const char *name;
    void       *address;
};
#define SAMPLES_ENTRY(name, function) {name, reinterpret_cast<void *>(&function)}
const Entry entries[] = {
    SAMPLES_ENTRY("clGetPlatformIDs", get_platform_ids), SAMPLES_ENTRY("clGetPlatformInfo", get_platform_info),
    SAMPLES_ENTRY("clGetDeviceIDs", get_device_ids), SAMPLES_ENTRY("clGetDeviceInfo", get_device_info),
    SAMPLES_ENTRY("clCreateContext", create_context), SAMPLES_ENTRY("clReleaseContext", release_context),
    SAMPLES_ENTRY("clCreateCommandQueueWithProperties", create_queue),
    SAMPLES_ENTRY("clReleaseCommandQueue", release_queue), SAMPLES_ENTRY("clCreateBuffer", create_buffer),
    SAMPLES_ENTRY("clCreateImage", create_image), SAMPLES_ENTRY("clReleaseMemObject", release_mem),
    SAMPLES_ENTRY("clCreateSamplerWithProperties", create_sampler), SAMPLES_ENTRY("clReleaseSampler", release_sampler),
    SAMPLES_ENTRY("clCreateProgramWithSource", create_program), SAMPLES_ENTRY("clBuildProgram", build_program),
    SAMPLES_ENTRY("clGetProgramBuildInfo", build_info), SAMPLES_ENTRY("clReleaseProgram", release_program),
    SAMPLES_ENTRY("clCreateKernel", create_kernel), SAMPLES_ENTRY("clReleaseKernel", release_kernel),
    SAMPLES_ENTRY("clSetKernelArg", set_arg), SAMPLES_ENTRY("clEnqueueNDRangeKernel", ndrange),
    SAMPLES_ENTRY("clEnqueueReadBuffer", read_buffer), SAMPLES_ENTRY("clEnqueueWriteBuffer", write_buffer),
    SAMPLES_ENTRY("clEnqueueCopyBuffer", copy_buffer), SAMPLES_ENTRY("clEnqueueFillBuffer", fill_buffer),
    SAMPLES_ENTRY("clEnqueueReadImage", read_image), SAMPLES_ENTRY("clEnqueueWriteImage", write_image),
    SAMPLES_ENTRY("clEnqueueCopyImage", copy_image), SAMPLES_ENTRY("clEnqueueMapBuffer", map_buffer),
    SAMPLES_ENTRY("clEnqueueUnmapMemObject", unmap), SAMPLES_ENTRY("clFinish", finish),
    SAMPLES_ENTRY("clFlush", flush), SAMPLES_ENTRY("clReleaseEvent", release_event),
    SAMPLES_ENTRY("clEnqueueReadBufferRect", read_rect), SAMPLES_ENTRY("clEnqueueWriteBufferRect", write_rect),
    SAMPLES_ENTRY("clSetEventCallback", set_callback),
};
#undef SAMPLES_ENTRY

}  // namespace

void log(const char *text)
{
    fputs(text, stdout);
    fflush(stdout);
}

void log_number(const char *label, double value) { printf("%s%.3f\n", label, value); }

void *allocate(size_t bytes) { return calloc(1, bytes != 0 ? bytes : 1); }
void  release_memory(void *memory) { free(memory); }

double seconds()
{
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
}

bool write_file(const char *path, const void *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    if (file == nullptr) return false;
    const bool ok = fwrite(data, 1, size, file) == size;
    fclose(file);
    return ok;
}

void *cl_entry(const char *name)
{
    for (const Entry &entry : entries)
        if (strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

}  // namespace samples

int main(int argc, char **argv)
{
    // Found before the executor library opens its own displays, as the full-stack tests do.
    const av::WidestHostGpu gpu;
    if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) {
        fprintf(stderr, "this host's widest GPU runs no OpenCL\n");
        return 2;
    }
    const av::ExecutorLibraryFile library(AV_EXECUTOR_LIBRARY);
    if (library.ops() == nullptr) {
        fprintf(stderr, "no executor library at %s\n", AV_EXECUTOR_LIBRARY);
        return 2;
    }
    int code = 0;
    {
        av::FullStackRig rig(*library.ops(), gpu.uuid());
        if (rig.started() != 0) {
            fprintf(stderr, "the rig's adapter did not start\n");
            return 2;
        }
        av::MockPlatform          memory;
        av::LockTrace             trace;
        av::RecordingThreadLock   lock(trace, 'H');
        av::RecordingThreadLock   send_lock(trace, 'S');
        av::RecordingThreadLock   objects_lock(trace, 'O');
        av::RecordingLog          icd_log;
        av::ScriptedThread        watcher;
        av::ScriptedGlShareSource no_gl;
        av::ScriptedD3dAdapterSource no_d3d;
        av::clicd::ClIcdRoot      root(memory, rig.kmt(), lock, send_lock, objects_lock, icd_log, watcher, no_gl, no_d3d,
                                       av::clicd::cl_icd_binding);
        samples::icd = &root;
        code = samples::run(argc, argv);
        for (std::uint32_t line = 0; line < icd_log.lines() && line < av::RecordingLog::max_lines; line++)
            fprintf(stderr, "avcl: %s\n", icd_log.line(line));
        samples::icd = nullptr;
    }
    return code;
}

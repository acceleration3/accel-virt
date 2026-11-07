#ifndef AV_OPENCL_BACKEND_HPP
#define AV_OPENCL_BACKEND_HPP

#include "executor/opencl/cl_event_signals.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "executor/opencl/cl_argument_retention.hpp"
#include "executor/opencl/cl_binary_envelope.hpp"
#include "executor/opencl/cl_program_il.hpp"
#include "executor/opencl/spirv_argument_kinds.hpp"
#include "executor/opencl/cl_bounced_rect.hpp"
#include "executor/opencl/cl_read_ahead.hpp"
#include "executor/opencl/copied_gl_sharing.hpp"
#include "executor/opencl/direct_gl_sharing.hpp"
#include "executor/guest_region.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "executor/host_object_registry.hpp"
#include "executor/opencl/host_svm_ranges.hpp"
#include "executor/opencl/kernel_pacer.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/opencl/staging_bytes.hpp"
#include "executor/session_backend.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/shim/gl_object_bytes.hpp"
#include "executor/shim/gl_renderbuffer_texture.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "proto/generated/opencl/cl_real_dispatch.h"

#include <stdint.h>

namespace av {

class DriverLog;
class OwnedFd;
class SharedImageShape;

class OpenGlBackend;

class GlDisplay;
class ImportTable;
class OpenClDevice;
class SourceMemory;
struct OpenClBackendProbe;

class OpenClBackend final : public clproto::RealDispatch, public SessionBackend {
public:
    explicit OpenClBackend(DriverLog &lines);
    OpenClBackend(BlockSource &blocks, DriverLog &lines);

    [[nodiscard]] bool initialize(const OpenClDevice *device, const GlDisplay *display, const OpenGlBackend *share,
                                  const ImportTable &imports);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    void drain() override;
    bool poll() override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    [[nodiscard]] bool adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;
    void unbind_arena(const SourceMemory &memory) override;
    void retire_arena(const SourceMemory &memory) override;

    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     OwnedFd &descriptor) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape, uint64_t into,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    void *handle_for(clproto::HandleKind kind, uint64_t wire) const override;
    void *svm_for(uint64_t address, uint64_t bytes) const override;
    void  made(clproto::HandleKind kind, void *object) override;
    bool  room_for(uint64_t objects) override;
    void  retained(clproto::HandleKind kind, void *object) override;
    void  released(clproto::HandleKind kind, void *object) override;

    cl_int     on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id *platforms,
                                   cl_uint *num_platforms) override;
    cl_int     on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries,
                                 cl_device_id *devices, cl_uint *num_devices) override;
    cl_int     on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size,
                                  void *param_value, size_t *param_value_size_ret) override;
    cl_context on_clCreateContextFromType(const cl_context_properties *properties, cl_device_type device_type,
                                          cl_int *errcode_ret) override;
    cl_context on_clCreateContext(const cl_context_properties *properties, cl_uint num_devices,
                                  const cl_device_id *devices, cl_int *errcode_ret) override;

    void *on_clSVMAlloc(cl_context context, cl_svm_mem_flags flags, size_t size,
                        cl_uint alignment) override;
    void  on_clSVMFree(cl_context context, void *svm_pointer) override;


    cl_mem on_clCreateBufferWithProperties(cl_context context, const cl_mem_properties *properties,
                                           cl_mem_flags flags, size_t size, void *host_ptr,
                                           cl_int *errcode_ret) override;

    cl_int on_clReleaseMemObject(cl_mem memobj) override;
    void  *on_clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map,
                                 cl_map_flags map_flags, size_t offset, size_t size,
                                 cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                 cl_event *event, cl_int *errcode_ret) override;
    cl_int on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void *mapped_ptr,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list, cl_event *event) override;

    cl_int on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                                  size_t offset, size_t size, void *ptr, cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim,
                                     const size_t *global_work_offset, const size_t *global_work_size,
                                     const size_t *local_work_size, cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueTask(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list,
                            const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clFlush(cl_command_queue command_queue) override;
    cl_int on_clFinish(cl_command_queue command_queue) override;
    cl_int on_clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
                                   size_t offset, size_t size, const void *ptr,
                                   cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                   cl_event *event) override;
    cl_int on_clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read,
                                 const size_t *origin, const size_t *region, size_t row_pitch,
                                 size_t slice_pitch, void *ptr, cl_uint num_events_in_wait_list,
                                 const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueReadBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                                      const size_t *buffer_origin, const size_t *host_origin,
                                      const size_t *region, size_t buffer_row_pitch,
                                      size_t buffer_slice_pitch, size_t host_row_pitch,
                                      size_t host_slice_pitch, void *ptr, cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueWriteImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write,
                                  const size_t *origin, const size_t *region, size_t input_row_pitch,
                                  size_t input_slice_pitch, const void *ptr,
                                  cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                  cl_event *event) override;
    cl_int on_clEnqueueWriteBufferRect(cl_command_queue command_queue, cl_mem buffer,
                                       cl_bool blocking_write, const size_t *buffer_origin,
                                       const size_t *host_origin, const size_t *region,
                                       size_t buffer_row_pitch, size_t buffer_slice_pitch,
                                       size_t host_row_pitch, size_t host_slice_pitch, const void *ptr,
                                       cl_uint num_events_in_wait_list,
                                       const cl_event *event_wait_list, cl_event *event) override;

    cl_int     on_clGetProgramInfo(cl_program program, cl_program_info param_name, size_t param_value_size,
                                   void *param_value, size_t *param_value_size_ret) override;
    cl_program on_clCreateProgramWithBinary(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                                            const size_t *lengths, const unsigned char **binaries,
                                            cl_int *binary_status, cl_int *errcode_ret) override;
    cl_int     on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size,
                                      const void *param_value) override;
    bool       declared_argument(cl_kernel kernel, cl_uint index, clproto::KernelArgument &argument) override;

    cl_mem on_clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj,
                                   cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                    cl_GLuint texture, cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                      cl_GLuint texture, cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                      cl_GLuint texture, cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                         cl_int *errcode_ret) override;
    cl_int on_clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type *gl_object_type,
                                cl_GLuint *gl_object_name) override;
    cl_int on_clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info param_name, size_t param_value_size,
                                 void *param_value, size_t *param_value_size_ret) override;
    cl_int on_clEnqueueAcquireGLObjects(cl_command_queue command_queue, cl_uint num_objects,
                                        const cl_mem *mem_objects, cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueReleaseGLObjects(cl_command_queue command_queue, cl_uint num_objects,
                                        const cl_mem *mem_objects, cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clSetEventCallback(cl_event event, cl_int command_exec_callback_type) override;
    cl_int on_clGetEventInfo(cl_event event, cl_event_info param_name, size_t param_value_size, void *param_value,
                             size_t *param_value_size_ret) override;

    void bind_signal(void (*signal)(void *context), void *context) override;

    void     shut_down() override;
    uint64_t ran() const;

private:
    cl_mem image_over(cl_mem buffer, const SharedImageShape &shape);
    friend struct OpenClBackendProbe;

    [[nodiscard]] Status hold(clproto::HandleKind kind, void *object);
    void                 let_go(clproto::HandleKind kind, void *object, uint32_t times);

    cl_uint program_binary_sizes(cl_program program, size_t *sizes);
    cl_int  sealed_binaries(cl_program program, size_t param_value_size, void *param_value,
                            size_t *param_value_size_ret);

    cl_int answers_type(cl_device_type device_type) const;

    [[nodiscard]] bool shares_with_gl(const GlDisplay &display) const;
    [[nodiscard]] bool copy_gl_objects();
    void               close_vehicle();
    void *acquire_queue();
    void  pace(cl_command_queue queue);
    void  look_at_marker();
    void  stop_pacing();

    static constexpr cl_uint max_program_devices = 16u;

    HostHeap            heap_;
    BlockSource        &blocks_;
    const OpenClDevice *bound_;
    ClBinaryEnvelope    envelope_;
    ClArgumentRetention arguments_;
    StagingBytes        binaries_;
    ClProgramIl         il_;
    SpirvArgumentKinds  argument_kinds_;
    GlContext             vehicle_;
    GlHostImport          import_;
    GlSharedBuffer        buffers_;
    GlRenderbufferTexture renderbuffer_textures_;
    GlObjectBytes         object_bytes_;
    DirectGlSharing       direct_;
    CopiedGlSharing       copied_;
    GlSharing            *gl_;
    bool                  reaches_gl_;
    bool                  imports_;
    void              *context_;
    void              *queue_;
    HostObjectRegistry<uint64_t, clproto::HandleKind, void *> handles_;
    HostSvmRanges    svm_;
    HostSvmRanges    mapped_;
    GuestBytes       guest_;
    GuestRegion      arena_;
    const ImportTable *imported_;
    ClEventSignals   events_;
    ClReadAhead      read_ahead_;
    ClBouncedRect    bounced_;
    MonotonicClock   clock_;
    KernelPacer      pacer_;
    void            *paced_queue_;
    void            *marker_;
    uint64_t       ran_;
    bool           lost_;
    DriverLog     &lines_;
};

}

#endif

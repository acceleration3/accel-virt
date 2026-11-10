#ifndef AV_CLICD_CL_ICD_ROOT_HPP
#define AV_CLICD_CL_ICD_ROOT_HPP

#include "core/support/object_pool.hpp"
#include "core/trace/state_reporter.hpp"
#include "icd/common/refused_calls.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opencl/cl_blocking_wait.hpp"
#include "icd/opencl/cl_destructor_callbacks.hpp"
#include "icd/opencl/cl_host_numbers.hpp"
#include "icd/opencl/cl_devices.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_event_owner.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_info_answers.hpp"
#include "icd/opencl/cl_image_judge.hpp"
#include "icd/opencl/cl_kernel_shape.hpp"
#include "icd/opencl/cl_program_source.hpp"
#include "icd/opencl/cl_object_table.hpp"
#include "icd/opencl/cl_pending_reads.hpp"
#include "icd/opencl/cl_refusal_log.hpp"
#include "icd/opencl/cl_platform.hpp"
#include "icd/opencl/cl_program_notify.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "icd/opencl/cl_wait_list.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <CL/cl_icd.h>

namespace av {
class KeyedLog;
}

namespace av {
class BlockSource;
class ExtensionWords;
class ClientTransport;
class Kmt;
class StateWriter;
class ThreadLock;
}

namespace av::icd {
class ServiceThread;
}

namespace av::clicd {

class ClArenaBlocks;
class ClBufferRect;
class MappedRegion;

class D3d11Bridge;
class D3d11Object;
class D3dAdapterSource;
class GlShareSource;
class HeaderSource;
struct ClIcdRootProbe;

class ClIcdRoot final : public StateReporter, public ClEventOwner {
public:
    static constexpr size_t lent_minimum = 64u * 1024u;
    static constexpr size_t  host_extensions_bytes = 4096u;

    ClIcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock,
              ThreadLock &objects_lock, ThreadLock &blocks_lock, KeyedLog &log, icd::ServiceThread &watcher_thread, GlShareSource &gl_share,
              D3dAdapterSource &d3d_adapters, D3d11Bridge &d3d11, HeaderSource &headers,
              ClIcdBinding &binding);
    ~ClIcdRoot();
    ClIcdRoot(const ClIcdRoot &) = delete;
    ClIcdRoot &operator=(const ClIcdRoot &) = delete;

    const cl_icd_dispatch &dispatch() const;

    cl_int platform_ids(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
    void  *extension_function(const char *name);
    cl_int platform_info(cl_platform_id platform, cl_platform_info name, size_t value_bytes,
                         void *value, size_t *size_ret);

    cl_int device_ids(cl_platform_id platform, cl_device_type type, cl_uint num_entries,
                      cl_device_id *devices, cl_uint *num_devices);
    cl_int device_info(cl_device_id device, cl_device_info name, size_t value_bytes, void *value,
                       size_t *size_ret);
    cl_int device_reference(cl_device_id device);
    static constexpr cl_device_info free_memory_amd = 0x4039u;

    cl_context create_context(const cl_context_properties *properties, const cl_device_id *devices,
                              cl_uint num_devices, cl_int *error,
                              const ClContextNotify &notify = ClContextNotify());
    cl_int     retain_context(cl_context context);
    cl_int     release_context(cl_context context);
    cl_context create_context_from_type(const cl_context_properties *properties, cl_device_type type,
                                        cl_int *error, const ClContextNotify &notify = ClContextNotify());
    cl_int     watch_destruction(cl_context context, ClDestructorCallbacks::ContextCallback callback,
                                 void *user_data);
    cl_int     watch_destruction(cl_mem object, ClDestructorCallbacks::MemCallback callback, void *user_data);
    cl_int     create_sub_devices(cl_device_id device, const cl_device_partition_property *properties,
                                  cl_uint num_devices, cl_device_id *out_devices, cl_uint *num_devices_ret);
    void      *extension_function_for(cl_platform_id platform, const char *name);

    cl_command_queue create_queue(cl_context context, cl_device_id device,
                                  const uint64_t *properties, cl_int *error);
    cl_mem           create_buffer(cl_context context, cl_mem_flags flags, size_t bytes,
                                   void *host_pointer, const uint64_t *properties, cl_int *error);
    cl_mem           create_buffer_for_host(cl_context context, cl_mem_flags flags, size_t bytes,
                                            void *host_pointer, const uint64_t *properties, cl_int *error);
    cl_mem           create_over_application(ClObject &context, ClientTransport &wire, cl_mem_flags flags,
                                             size_t bytes, void *host_pointer, cl_int *error);
    cl_program       create_program(cl_context context, cl_uint count, const char **sources,
                                    const size_t *lengths, cl_int *error);
    cl_kernel        create_kernel(cl_program program, const char *name, cl_int *error);
    cl_int           set_kernel_argument(cl_kernel kernel, cl_uint index, size_t bytes,
                                         const void *value);
    cl_int           enqueue_kernel(cl_command_queue queue, cl_kernel kernel, cl_uint dimensions,
                                    const size_t *offset, const size_t *global, const size_t *local,
                                    cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int           enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                  size_t offset, size_t bytes, void *into, cl_event *event,
                                  const ClWaitList &waits = ClWaitList());
    cl_int           enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                   size_t offset, size_t bytes, const void *from, cl_event *event,
                                   const ClWaitList &waits = ClWaitList());
    cl_int           finish(cl_command_queue queue);
    cl_int           flush(cl_command_queue queue);

    cl_int     build_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                             const char *options, const ClProgramNotify &notify = ClProgramNotify());
    cl_program create_program_with_binary(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                                          const size_t *lengths, const unsigned char **binaries,
                                          cl_int *binary_status, cl_int *error);
    cl_program create_program_with_il(cl_context context, const void *il, size_t length, cl_int *error);
    cl_program create_program_with_built_in_kernels(cl_context context, cl_uint num_devices,
                                                    const cl_device_id *device_list, const char *kernel_names,
                                                    cl_int *error);
    cl_int     compile_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                               const char *options, cl_uint num_headers, const cl_program *headers,
                               const char **header_names, const ClProgramNotify &notify);
    cl_program link_program(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                            const char *options, cl_uint num_inputs, const cl_program *inputs,
                            const ClProgramNotify &notify, cl_int *error);
    cl_int     program_info(cl_program program, cl_program_info name, size_t value_bytes, void *value,
                            size_t *size_ret);
    cl_int     set_specialization_constant(cl_program program, cl_uint id, size_t bytes, const void *value);
    cl_int     set_program_release_callback(cl_program program, void(CL_CALLBACK *notify)(cl_program, void *));
    cl_int     unload_platform_compiler(cl_platform_id platform);

    cl_int    create_kernels_in_program(cl_program program, cl_uint num_kernels, cl_kernel *kernels,
                                        cl_uint *num_kernels_ret);
    cl_kernel clone_kernel(cl_kernel kernel, cl_int *error);
    cl_int    kernel_info(cl_kernel kernel, cl_kernel_info name, size_t value_bytes, void *value, size_t *size_ret);
    cl_int    kernel_arg_info(cl_kernel kernel, cl_uint index, cl_kernel_arg_info name, size_t value_bytes,
                              void *value, size_t *size_ret);
    cl_int    kernel_sub_group_info(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info name,
                                    size_t input_bytes, const void *input, size_t value_bytes, void *value,
                                    size_t *size_ret);
    cl_int    set_kernel_exec_info(cl_kernel kernel, cl_kernel_exec_info name, size_t bytes, const void *value);

    cl_int program_build_info(cl_program program, cl_device_id device, cl_program_build_info name,
                              size_t value_bytes, void *value, size_t *size_ret);
    cl_int kernel_work_group_info(cl_kernel kernel, cl_device_id device,
                                  cl_kernel_work_group_info name, size_t value_bytes, void *value,
                                  size_t *size_ret);

    cl_int enqueue_copy_buffer(cl_command_queue queue, cl_mem from, cl_mem to, size_t from_offset,
                               size_t to_offset, size_t bytes, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_fill_buffer(cl_command_queue queue, cl_mem buffer, const void *pattern,
                               size_t pattern_bytes, size_t offset, size_t bytes, cl_event *event,
                               const ClWaitList &waits = ClWaitList());
    cl_int enqueue_copy_image(cl_command_queue queue, cl_mem from, cl_mem to,
                              const size_t *from_origin, const size_t *to_origin,
                              const size_t *region, cl_event *event, const ClWaitList &waits = ClWaitList());

    cl_mem create_image(cl_context context, cl_mem_flags flags, const cl_image_format *format,
                        const cl_image_desc *desc, void *host_pointer, cl_int *error);
    cl_mem create_image_2d(cl_context context, cl_mem_flags flags, const cl_image_format *format, size_t width,
                           size_t height, size_t row_pitch, void *host_pointer, cl_int *error);
    cl_mem create_image_3d(cl_context context, cl_mem_flags flags, const cl_image_format *format, size_t width,
                           size_t height, size_t depth, size_t row_pitch, size_t slice_pitch, void *host_pointer,
                           cl_int *error);
    cl_int enqueue_read_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                              const size_t *region, size_t row_pitch, size_t slice_pitch,
                              void *into, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_write_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                               const size_t *region, size_t row_pitch, size_t slice_pitch,
                               const void *from, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_read_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect, void *into,
                                    cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_write_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect,
                                     const void *from, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_copy_buffer_rect(cl_command_queue queue, cl_mem from, cl_mem to, const size_t *from_origin,
                                    const size_t *to_origin, const size_t *region, size_t from_row_pitch,
                                    size_t from_slice_pitch, size_t to_row_pitch, size_t to_slice_pitch,
                                    cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_sampler create_sampler(cl_context context, const uint64_t *properties, cl_int *error);
    cl_int     sampler_info(cl_sampler sampler, cl_sampler_info name, size_t value_bytes, void *value,
                            size_t *size_ret);
    cl_int supported_image_formats(cl_context context, cl_mem_flags flags, cl_mem_object_type type,
                                   cl_uint num_entries, cl_image_format *formats, cl_uint *num_formats);

    cl_mem create_from_gl_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *error);
    cl_mem create_from_gl_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                                  cl_GLuint texture, cl_int *error);
    cl_mem create_from_gl_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                       cl_int *error);
    cl_int gl_object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name);
    cl_int gl_texture_info(cl_mem object, cl_gl_texture_info name, size_t value_bytes, void *value,
                           size_t *size_ret);
    cl_int enqueue_acquire_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                              const cl_event *events, cl_event *event);
    cl_int enqueue_release_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                              const cl_event *events, cl_event *event);
    cl_int gl_context_info(const cl_context_properties *properties, cl_gl_context_info name, size_t value_bytes,
                           void *value, size_t *size_ret);

    static constexpr cl_uint d3d10_device_source = 0x4010u;
    static constexpr cl_uint d3d10_adapter_source = 0x4011u;
    static constexpr cl_uint d3d10_preferred_devices = 0x4012u;
    static constexpr cl_uint d3d10_all_devices = 0x4013u;
    static constexpr cl_uint d3d10_context_device = 0x4014u;
    static constexpr cl_int  invalid_d3d10_device = -1002;

    cl_int device_ids_from_d3d10(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                 cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices);
    cl_mem create_from_d3d10(cl_context context, cl_int *error);
    cl_int enqueue_d3d10_objects(cl_command_queue queue);

    static constexpr cl_uint d3d11_device_source = 0x4019u;
    static constexpr cl_uint d3d11_adapter_source = 0x401Au;
    static constexpr cl_uint d3d11_preferred_devices = 0x401Bu;
    static constexpr cl_uint d3d11_all_devices = 0x401Cu;
    static constexpr cl_uint d3d11_context_device = 0x401Du;
    static constexpr cl_uint d3d11_resource_info = 0x401Eu;
    static constexpr cl_uint d3d11_subresource_info = 0x401Fu;
    static constexpr cl_uint d3d11_prefer_shared = 0x402Du;
    static constexpr cl_int  invalid_d3d11_device = -1006;
    static constexpr cl_int  invalid_d3d11_resource = -1007;
    static constexpr cl_command_type acquire_d3d11_command = 0x4020u;
    static constexpr cl_command_type release_d3d11_command = 0x4021u;

    cl_int device_ids_from_d3d11(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                 cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices);
    cl_mem create_from_d3d11_buffer(cl_context context, cl_mem_flags flags, void *resource, cl_int *error);
    cl_mem create_from_d3d11_texture(cl_context context, cl_mem_flags flags, void *resource, cl_uint subresource,
                                     D3d11Resource::Kind kind, cl_int *error);
    cl_int enqueue_acquire_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                 const cl_event *events, cl_event *event);
    cl_int enqueue_release_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                 const cl_event *events, cl_event *event);

    cl_int enqueue_marker(cl_command_queue queue, cl_uint count, const cl_event *events,
                          cl_event *event);
    cl_int enqueue_barrier(cl_command_queue queue, cl_uint count, const cl_event *events,
                           cl_event *event);
    cl_int           enqueue_external(cl_command_queue queue, cl_uint count, const cl_mem *objects,
                                      cl_event *event, const ClWaitList &waits = ClWaitList());

    cl_int enqueue_task(cl_command_queue queue, cl_kernel kernel, cl_event *event,
                        const ClWaitList &waits = ClWaitList());
    cl_int enqueue_event_marker(cl_command_queue queue, cl_event *event);
    cl_int enqueue_wait_for_events(cl_command_queue queue, cl_uint count, const cl_event *events);
    cl_int enqueue_copy_image_to_buffer(cl_command_queue queue, cl_mem image, cl_mem buffer, const size_t *origin,
                                        const size_t *region, size_t offset, cl_event *event,
                                        const ClWaitList &waits = ClWaitList());
    cl_int enqueue_copy_buffer_to_image(cl_command_queue queue, cl_mem buffer, cl_mem image, size_t offset,
                                        const size_t *origin, const size_t *region, cl_event *event,
                                        const ClWaitList &waits = ClWaitList());
    cl_int enqueue_fill_image(cl_command_queue queue, cl_mem image, const void *colour, const size_t *origin,
                              const size_t *region, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_migrate(cl_command_queue queue, cl_uint count, const cl_mem *objects,
                           cl_mem_migration_flags flags, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_mem create_sub_buffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type type, const void *info,
                             cl_int *error);
    void  *map_image(cl_command_queue queue, cl_mem image, cl_bool blocking, cl_map_flags flags,
                     const size_t *origin, const size_t *region, size_t *row_pitch, size_t *slice_pitch,
                     cl_event *event, cl_int *error, const ClWaitList &waits = ClWaitList());
    cl_mem create_image_with_properties(cl_context context, const cl_mem_properties *properties,
                                        cl_mem_flags flags, const cl_image_format *format, const cl_image_desc *desc,
                                        void *host_pointer, cl_int *error);
    cl_mem create_pipe(cl_context context, cl_mem_flags flags, cl_uint packet_bytes, cl_uint packets,
                       const cl_pipe_properties *properties, cl_int *error);
    cl_int pipe_info(cl_mem pipe, cl_pipe_info name, size_t value_bytes, void *value, size_t *size_ret);
    cl_int svm_memcpy(cl_command_queue queue, cl_bool blocking, void *to, const void *from, size_t bytes,
                      cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int svm_mem_fill(cl_command_queue queue, void *pointer, const void *pattern, size_t pattern_bytes,
                        size_t bytes, cl_event *event, const ClWaitList &waits = ClWaitList());
    using SvmFreed = void(CL_CALLBACK *)(cl_command_queue queue, cl_uint count, void *pointers[], void *user_data);
    cl_int enqueue_svm_free(cl_command_queue queue, cl_uint count, void *pointers[], SvmFreed freed,
                            void *user_data, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_svm_migrate(cl_command_queue queue, cl_uint count, const void **pointers,
                               cl_event *event, const ClWaitList &waits = ClWaitList());

    void  *map_buffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking, cl_map_flags flags,
                      size_t offset, size_t bytes, cl_event *event, cl_int *error, const ClWaitList &waits = ClWaitList());
    cl_int unmap(cl_command_queue queue, cl_mem buffer, void *pointer, cl_event *event, const ClWaitList &waits = ClWaitList());

    void  *svm_alloc(cl_context context, cl_svm_mem_flags flags, size_t bytes, cl_uint alignment);
    void   svm_free(cl_context context, void *pointer);
    cl_int svm_map(cl_command_queue queue, cl_bool blocking, cl_map_flags flags, void *pointer,
                   size_t bytes, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int svm_unmap(cl_command_queue queue, void *pointer, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int set_kernel_svm_argument(cl_kernel kernel, cl_uint index, const void *pointer);

    cl_event create_user_event(cl_context context, cl_int *error);
    cl_int   set_user_event_status(cl_event event, cl_int status);
    cl_int   wait_for_events(cl_uint count, const cl_event *events);
    cl_int   event_info(cl_event event, cl_event_info name, size_t value_bytes, void *value,
                        size_t *size_ret);
    void     note_refused(const char *entry_point);
    cl_int   noted(const char *entry_point, cl_int result);
    cl_int   noted_query(const char *entry_point, uint32_t query, cl_int result);
    cl_int   context_info(cl_context context, cl_context_info name, size_t value_bytes, void *value,
                          size_t *size_ret);
    cl_int   queue_info(cl_command_queue queue, cl_command_queue_info name, size_t value_bytes, void *value,
                        size_t *size_ret);
    cl_int   mem_object_info(cl_mem object, cl_mem_info name, size_t value_bytes, void *value,
                             size_t *size_ret);
    cl_int   image_info(cl_mem image, cl_image_info name, size_t value_bytes, void *value, size_t *size_ret);
    cl_int   event_profiling(cl_event event, cl_profiling_info name, size_t value_bytes, void *value,
                             size_t *size_ret);
    cl_int   watch_event(cl_event event, cl_int status,
                         ClWatchedEvent::Notify notify, void *user_data);
    void     deliver_watched(ClObject &event) override;
    void     release_watched(ClObject &event) override;

    cl_int retain(void *object, ClObject::Kind kind);
    cl_int release(void *object, ClObject::Kind kind);

    void report(StateWriter &writer) override;
    bool invariants_hold() const;

private:
    friend struct ClIcdRootProbe;

    GuestClContext  *context_of(ClObject &object);
    ClientTransport *wire_of(ClObject &object);
    ClObject        *judged(const void *pointer, ClObject::Kind kind);
    cl_int           host_device_info(ClientTransport &wire, ClDevice &device, cl_device_info name,
                                      size_t value_bytes, void *value, size_t *size_ret);
    cl_int           host_mem_info(ClientTransport &wire, ClObject &object, cl_mem_info name, size_t value_bytes,
                                   void *value, size_t *size_ret);
    bool             spells(const char *name, const char *wanted) const;
    cl_int           import_handle_types(ClDevice &device, size_t value_bytes, void *value,
                                         size_t *size_ret);
    cl_int           device_extensions(ClDevice &device, size_t value_bytes, void *value,
                                       size_t *size_ret);
    cl_int           host_extensions(ClDevice &device, ExtensionWords &words);
    cl_int           device_extension_versions(ClDevice &device, size_t value_bytes, void *value,
                                               size_t *size_ret);
    cl_int           c_features(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret);
    cl_int           platform_extension_versions(size_t value_bytes, void *value, size_t *size_ret) const;
    cl_int           free_memory(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret);
    template <typename Property>
    cl_int           judge(const Property *properties, const uint64_t *allowed,
                           uint32_t allowed_count, ClPropertyList &read) const;
    cl_int           deliver_reads(ClientTransport &wire, ClArenaBlocks &blocks, const uint64_t *events,
                                   uint32_t count);
    cl_int           deliver_queued(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t queue);
    cl_int           drain(ClientTransport &wire, uint64_t queue, bool driven, const uint64_t *waits,
                           uint32_t count);
    [[nodiscard]] cl_int settle(ClientTransport &wire, ClObject &queue, const ClWaitList &waits);
    uint32_t             without_unset_user_events(ClientTransport &wire, const ClWaitList &waits, uint64_t *listed);
    void             describe_adapter(uint32_t adapter, ClDevice &device);
    void             learn_devices();
    uint64_t         host_device_of(ClObject &object);
    cl_int           device_ids_for_luid(uint64_t luid, cl_device_id *devices, cl_uint *num_devices);
    cl_int           d3d11_device_for(const ClPropertyList &list, const ClDevice &on, void *&device);
    cl_int           d3d11_resource(cl_context context, cl_mem_flags flags, void *resource, uint32_t subresource,
                                    D3d11Resource::Kind kind, ClObject *&held, GuestClContext *&owner,
                                    D3d11Resource &described);
    cl_mem           d3d11_object(cl_mem made, GuestClContext &owner, void *resource, uint32_t subresource,
                                  const D3d11Resource &described, cl_mem_flags flags, cl_int *error);
    cl_mem           d3d11_twin(ClObject &held, GuestClContext &owner, void *resource, uint32_t subresource,
                                void *&twin);
    cl_int           d3d11_queue(cl_command_queue queue, cl_uint waits, const cl_event *events, ClObject *&held,
                                 GuestClContext *&context, ClientTransport *&wire);
    cl_int           d3d11_copy_in(cl_command_queue queue, GuestClContext &context, const cl_mem *objects,
                                   cl_uint count);
    cl_int           d3d11_copy_out(cl_command_queue queue, GuestClContext &context, const cl_mem *objects,
                                    cl_uint count);
    D3d11Object     *d3d11_object_of(ClObject &mem);
    D3d11Object     *d3d11_object_in(GuestClContext &owner, const void *mem);
    void             drained_queue(ClObject &queue);
    void             let_go_of_memory(GuestClContext &context, ClObject &buffer);
    [[nodiscard]] cl_int drain_unless_idle(ClientTransport &wire, ClObject &queue);
    ClObject        *busied(cl_command_queue queue);
    [[nodiscard]] cl_int judge_host_transfer(const ClObject &queue, const ClObject &mem, bool host_reads);
    [[nodiscard]] cl_int judge_host_map(const ClObject &queue, const ClObject &mem, cl_map_flags map_flags);
    [[nodiscard]] cl_int judge_buffer_range(const ClObject &mem, uint64_t offset, uint64_t bytes);
    bool                 same_backing(const ClObject &first, const ClObject &second);
    bool                 buffer_ranges_overlap(const ClObject &first, uint64_t first_offset, const ClObject &second,
                                               uint64_t second_offset, uint64_t bytes);
    [[nodiscard]] cl_int judge_rect_copy(const ClObject &source, const ClObject &target, const size_t *from_origin,
                                         const size_t *to_origin, const size_t *region, size_t from_row_pitch,
                                         size_t from_slice_pitch, size_t to_row_pitch, size_t to_slice_pitch);
    [[nodiscard]] cl_int judge_rect_transfer(const ClObject &mem, const ClBufferRect &rect);
    [[nodiscard]] cl_int judge_fill(const ClObject &mem, size_t pattern_bytes, size_t offset, size_t bytes);
    ClKernelShape   *kernel_shape(ClObject &kernel, ClientTransport &wire);
    [[nodiscard]] cl_int judge_kernel_argument(ClKernelShape &shape, uint32_t index, size_t bytes, const void *value);
    cl_int           marker_event(ClientTransport &wire, ClObject &queue, cl_event *event, cl_command_type type = 0u);
    cl_int           wait_list(ClObject &queue, cl_uint count, const cl_event *events, ClHostNumbers &room,
                               uint64_t *&into);
    void             heal_queue(ClObject &queue);
    cl_int           gl_share_for(const ClPropertyList &list, uint32_t &share);
    ClObject        *gl_sharing(cl_context context, cl_int *error);
    static bool      texture_target(cl_GLenum target);
    bool             source_needs_unreported_extension(ClObject &program);
    uint64_t         failed_link_stand_in(ClientTransport &wire, uint64_t context);
    void             note_build_log(ClientTransport &wire, uint64_t program, uint64_t device, const char *options);
    cl_int           program_devices(ClObject &context, cl_uint num_devices, const cl_device_id *device_list);
    cl_int           program_list(ClObject &context, cl_uint count, const cl_program *programs, ClHostNumbers &room,
                                  uint64_t *&into);
    cl_int           program_binaries(ClientTransport &wire, ClObject &program, size_t value_bytes, void *value,
                                      size_t *size_ret);
    cl_mem           gl_object(ClientTransport &wire, ClObject &context, uint64_t made, cl_int host_error,
                               cl_int *error);
    using GlObjectsCommand = cl_int (*)(ClientTransport &, uint64_t, cl_uint, const uint64_t *, cl_uint,
                                        const uint64_t *, uint64_t *);
    cl_int           enqueue_gl_objects(GlObjectsCommand command, cl_command_queue queue, cl_uint count,
                                        const cl_mem *objects, cl_uint waits, const cl_event *events,
                                        cl_event *event);
    cl_int           unmap_image(ClientTransport &wire, ClObject &queue, uint64_t image, MappedRegion &region,
                                 cl_event *event);
    cl_int           svm_from_guest(ClientTransport &wire, ClObject &queue, uint64_t to, const void *from,
                                    size_t bytes);
    cl_int           svm_to_guest(ClientTransport &wire, ClObject &queue, void *to, uint64_t from, size_t bytes);
    void             hand_over(ClientTransport &wire, ClObject &queue, uint64_t produced, cl_event *event);
    using PointCommand = cl_int (*)(ClientTransport &, uint64_t, cl_uint, const uint64_t *, uint64_t *);
    cl_int           enqueue_point(PointCommand command, cl_command_queue queue, cl_uint count,
                                   const cl_event *events, cl_event *event);
    size_t           element_bytes(ClientTransport &wire, uint64_t image);
    cl_int           fill_image(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t device,
                                uint64_t image, const cl_image_desc &desc, const void *from);
    cl_int           fill_buffer(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t device,
                                 uint64_t buffer, size_t bytes, const void *from);
    uint64_t         own_queue(ClientTransport &wire, uint64_t context, uint64_t device, cl_int &error);
    [[nodiscard]] Status open_device(ClDevice &device, bool &exhausted);
    void             give_back_to_host(ClientTransport &wire, ClObject::Kind kind, uint64_t handle);
    void            *wrap(ClientTransport &transport, ClObject::Kind kind, uint64_t wire,
                          GuestClContext *context,
                          ClObject *parent, cl_int *error);
    ClImageLimits    image_limits(ClObject &context, bool dimensions, bool pitch);
    uint64_t         max_allocation(ClObject &context);
    cl_int           build_with_includes(ClObject &program, ClientTransport &wire, const char *options);
    cl_int           refusal_for(ClObject::Kind kind) const;
    ClObject        *drop_reference(void *object, ClObject &held, ClObject::Kind kind, ClDestructorCallbacks::Due &due);
    cl_uint          reference_count(const ClObject &held);

    ThreadLock            &lock_;
    KeyedLog              &lines_;
    GlShareSource         &gl_share_;
    D3dAdapterSource      &d3d_adapters_;
    D3d11Bridge           &d3d11_;
    HeaderSource          &headers_;
    ClIcdBinding          &binding_;
    ClDispatchTable        dispatch_;
    ClPlatform             platform_;
    ClObjectTable          objects_;
    Kmt                   &kmt_;
    ClDevices              devices_;
    icd::SharedHandles     shared_handles_;
    ObjectPool<GuestClContext> contexts_;
    ObjectPool<ClKernelShape>  shapes_;
    ObjectPool<ClProgramSource> sources_;
    BlockSource           &memory_;
    ThreadLock            &send_lock_;
    ThreadLock            &blocks_lock_;
    ClEventWatcher         watcher_;
    ClBlockingWait         waits_;
    ClInfoAnswers<host_extensions_bytes> device_answers_;
    ClPendingReads         reads_;
    icd::RefusedCalls      refused_;
    ClRefusalLog           refusals_;
    ClDestructorCallbacks  destructor_callbacks_;
};

}

#endif

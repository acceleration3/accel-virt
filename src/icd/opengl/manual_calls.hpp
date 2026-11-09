#ifndef AV_GLICD_MANUAL_CALLS_HPP
#define AV_GLICD_MANUAL_CALLS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class Kmt;
}

namespace av::glicd {

class CurrentContexts;
class GlContextTable;
class GuestGlContext;

class ManualCalls {
public:
    enum : uint32_t {
        gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u,
        gl_read_only = 0x88B8u, gl_write_only = 0x88B9u, gl_read_write = 0x88BAu, gl_dynamic_storage_bit = 0x0100u,
        gl_extensions = 0x1F03u, gl_num_extensions = 0x821Du, gl_debug_callback_function = 0x8244u, gl_debug_callback_user_param = 0x8245u,
        gl_vertex_attrib_array_pointer = 0x8645u, gl_buffer_map_pointer = 0x88BDu, gl_uniform = 0x92E1u,
        gl_type = 0x92FAu, gl_invalid_index = 0xFFFFFFFFu, gl_texture_width = 0x1000u, gl_texture_height = 0x1001u,
        gl_texture_depth = 0x8071u, gl_texture_compressed_image_size = 0x86A0u,
    };

    ManualCalls(CurrentContexts &currents, GlContextTable &contexts, const Kmt &kmt);
    ManualCalls(const ManualCalls &) = delete;
    ManualCalls &operator=(const ManualCalls &) = delete;

    void *map_buffer(uint32_t target, uint32_t access);
    void *map_buffer_range(uint32_t target, intptr_t offset, intptr_t length, uint32_t access);
    void *map_named_buffer(uint32_t buffer, uint32_t access);
    void *map_named_buffer_range(uint32_t buffer, intptr_t offset, intptr_t length, uint32_t access);
    bool  unmap_buffer(uint32_t target);
    bool  unmap_named_buffer(uint32_t buffer);
    void  flush_mapped_range(uint32_t target, intptr_t offset, intptr_t length);
    void  flush_mapped_named_range(uint32_t buffer, intptr_t offset, intptr_t length);
    void  buffer_pointer(uint32_t target, uint32_t pname, void **params);
    void  named_buffer_pointer(uint32_t buffer, uint32_t pname, void **params);
    bool  buffer_map_state(uint32_t target, uint32_t pname, int64_t &value);
    bool  named_buffer_map_state(uint32_t buffer, uint32_t pname, int64_t &value);
    void  refuse();

    void  buffer_sub_data(uint32_t target, intptr_t offset, intptr_t size, const void *data);
    void  named_buffer_sub_data(uint32_t buffer, intptr_t offset, intptr_t size, const void *data);
    void  named_buffer_sub_data_ext(uint32_t buffer, intptr_t offset, intptr_t size, const void *data);
    void  get_buffer_sub_data(uint32_t target, intptr_t offset, intptr_t size, void *data);
    void  get_named_buffer_sub_data(uint32_t buffer, intptr_t offset, intptr_t size, void *data);
    void  get_named_buffer_sub_data_ext(uint32_t buffer, intptr_t offset, intptr_t size, void *data);
    bool  stage_upload(const void *pixels, size_t bytes);
    void  finish_upload();
    bool  stage_download(const void *pixels, size_t bytes);
    void  finish_download(void *pixels, size_t bytes);
    void  host_writes_target(uint32_t target);
    void  host_writes_buffer(uint32_t buffer);
    void  bind_buffer_indexed(uint32_t target, uint32_t buffer);
    void  bind_buffers_indexed(uint32_t target);
    void  shaders_write_buffers();
    bool  unpack_slice_bound();
    bool  slice_unpack_pixels(const void *&pixels, size_t bytes);
    void  unpack_slice_done();
    bool  pack_slice_bound();
    bool  slice_pack_pixels(void *&pixels, size_t bytes);
    void  pack_slice_done();
    bool  refuse_sliced_unpack();
    bool  refuse_sliced_pack();
    bool  clear_bound_buffer(uint32_t target, uint32_t internal_format, uint32_t format, uint32_t type,
                             const void *data);
    bool  clear_bound_buffer_range(uint32_t target, uint32_t internal_format, intptr_t offset, intptr_t size,
                                   uint32_t format, uint32_t type, const void *data);
    bool  clear_named_buffer(uint32_t buffer, uint32_t internal_format, uint32_t format, uint32_t type,
                             const void *data);
    bool  clear_named_buffer_range(uint32_t buffer, uint32_t internal_format, intptr_t offset, intptr_t size,
                                   uint32_t format, uint32_t type, const void *data);
    bool  invalidate_named_buffer(uint32_t buffer);
    bool  invalidate_named_buffer_range(uint32_t buffer, intptr_t offset, intptr_t length);
    void  client_attributes_pushed(uint32_t mask);
    void  client_attributes_popped();
    void  client_attributes_reset(uint32_t mask);
    void  vertex_array_elements(uint32_t array, uint32_t buffer);

    void  create_memory_objects(int32_t count, uint32_t *objects);
    void  delete_memory_objects(int32_t count, const uint32_t *objects);
    void  import_memory_win32(uint32_t memory, uint64_t size, uint32_t handle_type, void *handle);
    void  gen_semaphores(int32_t count, uint32_t *semaphores);
    void  delete_semaphores(int32_t count, const uint32_t *semaphores);
    void  import_semaphore_win32(uint32_t semaphore, uint32_t handle_type, void *handle);
    bool  is_memory_object(uint32_t memory);
    bool  is_semaphore(uint32_t semaphore);
    void  memory_object_parameter(uint32_t memory, uint32_t pname, const int32_t *params);
    void  get_memory_object_parameter(uint32_t memory, uint32_t pname, int32_t *params);
    void  semaphore_parameter(uint32_t semaphore, uint32_t pname, const uint64_t *params);
    void  get_semaphore_parameter(uint32_t semaphore, uint32_t pname, uint64_t *params);
    void  import_memory_win32_name(uint32_t memory, uint64_t size, uint32_t handle_type, const void *name);
    void  import_semaphore_win32_name(uint32_t semaphore, uint32_t handle_type, const void *name);
    void  buffer_storage_mem(uint32_t target, intptr_t size, uint32_t memory, uint64_t offset);
    void  named_buffer_storage_mem(uint32_t buffer, intptr_t size, uint32_t memory, uint64_t offset);
    void  tex_storage_mem_1d(uint32_t target, int32_t levels, uint32_t format, int32_t width,
                             uint32_t memory, uint64_t offset);
    void  tex_storage_mem_2d(uint32_t target, int32_t levels, uint32_t format, int32_t width,
                             int32_t height, uint32_t memory, uint64_t offset);
    void  tex_storage_mem_2d_multisample(uint32_t target, int32_t samples, uint32_t format, int32_t width,
                                         int32_t height, bool fixed_locations, uint32_t memory,
                                         uint64_t offset);
    void  tex_storage_mem_3d(uint32_t target, int32_t levels, uint32_t format, int32_t width, int32_t height,
                             int32_t depth, uint32_t memory, uint64_t offset);
    void  tex_storage_mem_3d_multisample(uint32_t target, int32_t samples, uint32_t format, int32_t width,
                                         int32_t height, int32_t depth, bool fixed_locations,
                                         uint32_t memory, uint64_t offset);
    void  texture_storage_mem_1d(uint32_t texture, int32_t levels, uint32_t format, int32_t width,
                                 uint32_t memory, uint64_t offset);
    void  texture_storage_mem_2d(uint32_t texture, int32_t levels, uint32_t format, int32_t width,
                                 int32_t height, uint32_t memory, uint64_t offset);
    void  texture_storage_mem_2d_multisample(uint32_t texture, int32_t samples, uint32_t format,
                                             int32_t width, int32_t height, bool fixed_locations,
                                             uint32_t memory, uint64_t offset);
    void  texture_storage_mem_3d(uint32_t texture, int32_t levels, uint32_t format, int32_t width,
                                 int32_t height, int32_t depth, uint32_t memory, uint64_t offset);
    void  texture_storage_mem_3d_multisample(uint32_t texture, int32_t samples, uint32_t format,
                                             int32_t width, int32_t height, int32_t depth,
                                             bool fixed_locations, uint32_t memory, uint64_t offset);
    void  signal_semaphore(uint32_t semaphore, uint32_t buffer_barriers, const uint32_t *buffers,
                           uint32_t texture_barriers, const uint32_t *textures, const uint32_t *layouts);
    void  wait_semaphore(uint32_t semaphore, uint32_t buffer_barriers, const uint32_t *buffers,
                         uint32_t texture_barriers, const uint32_t *textures, const uint32_t *layouts);
    void  attribute_pointer(uint32_t index, uint32_t pname, void **pointer);
    void  pointer(uint32_t pname, void **params);

    void pixel_store(uint32_t pname, int32_t value);
    void pixel_store(uint32_t pname, float value);
    void bind_buffer(uint32_t target, uint32_t buffer);
    void delete_buffers(int32_t count, const uint32_t *buffers);
    void gen_buffers(int32_t count, uint32_t *buffers);
    void gen_textures(int32_t count, uint32_t *textures);
    void gen_vertex_arrays(int32_t count, uint32_t *arrays);
    void bind_vertex_array(uint32_t array);
    void delete_vertex_arrays(int32_t count, const uint32_t *arrays);
    void delete_transform_feedbacks(int32_t count, const uint32_t *ids);
    void buffer_data(uint32_t target, intptr_t size, const void *data, uint32_t usage);
    void buffer_storage(uint32_t target, intptr_t size, const void *data, uint32_t flags);
    void named_buffer_data(uint32_t buffer, intptr_t size, const void *data, uint32_t usage);
    void named_buffer_storage(uint32_t buffer, intptr_t size, const void *data, uint32_t flags);
    void named_buffer_data_ext(uint32_t buffer, intptr_t size, const void *data, uint32_t usage);
    void named_buffer_storage_ext(uint32_t buffer, intptr_t size, const void *data, uint32_t flags);
    void vertex_attrib_pointer(uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride,
                               const void *pointer);
    void enable_vertex_attrib_array(uint32_t index, bool enable);
    void array_pointer(uint32_t slot, int32_t size, uint32_t type, int32_t stride, const void *pointer);
    void client_state(uint32_t capability, bool enable);
    void client_active_texture(uint32_t texture);
    void interleaved_arrays(uint32_t format, int32_t stride, const void *pointer);
    void array_element(int32_t index);
    void begin_primitives();
    bool stage_arrays(int32_t first, int32_t count);
    bool stage_elements(uint32_t type, int32_t count, const void *&indices, int32_t base_vertex);
    bool stage_multi_arrays(const int32_t *first, const int32_t *count, int32_t draws);
    bool stage_multi_elements(const int32_t *count, uint32_t type, const void *const *indices, int32_t draws,
                              const int32_t *base_vertex, const uint64_t *&host_indices);
    bool refuse_indirect_draw();
    void finish_client_draw();
    void draw_arrays(uint32_t mode, int32_t first, int32_t count, int32_t instances);
    void draw_elements(uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t instances);
    void draw_range_elements(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                             const void *indices);
    void flush();
    void bind_framebuffer(uint32_t target, uint32_t framebuffer);
    void draw_buffer(uint32_t buffer);
    void read_buffer(uint32_t buffer);
    uint32_t client_wait_sync(void *sync, uint32_t flags, uint64_t timeout);
    void     finish();
    void debug_message_callback(void *callback, const void *user);
    void link_program(uint32_t program);
    void delete_program(uint32_t program);
    int32_t attrib_location(uint32_t program, const char *name);
    int32_t uniform_location(uint32_t program, const char *name);
    size_t level_bytes(uint32_t target, int32_t level, uint32_t format, uint32_t type);
    size_t compressed_level_bytes(uint32_t target, int32_t level);
    void           integers(uint32_t pname, int32_t *data);
    void           read_pixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                               void *pixels);
    const uint8_t *string(uint32_t name);
    const uint8_t *string_i(uint32_t name, uint32_t index);

private:
    GuestGlContext *sending();
    bool            substituted(uint32_t kind, uint32_t name, GuestGlContext *&context, uint32_t &made);
    void            record(uint32_t error);
    bool            bound_to(GuestGlContext &context, uint32_t target, uint32_t &buffer);
    void           *map_name(GuestGlContext &context, uint32_t buffer, int64_t offset, int64_t length,
                             uint32_t access);
    bool            unmap_name(GuestGlContext &context, uint32_t buffer);
    void            mark_name(GuestGlContext &context, uint32_t buffer, intptr_t offset, intptr_t length);
    uint32_t        sliced_buffer(GuestGlContext &context, uint32_t target);
    void            forget_bound(GuestGlContext &context, uint32_t target);
    void            respecify_bound(GuestGlContext &context, uint32_t target, intptr_t size, bool with_data);
    void            pin_storage(GuestGlContext &context, intptr_t size, const void *data);
    void            wrote_bound(GuestGlContext &context, uint32_t target, intptr_t offset, intptr_t size,
                                const void *data);

    CurrentContexts &currents_;
    GlContextTable  &contexts_;
    const Kmt       &kmt_;
};

}

#endif

/* What the OpenGL entries the generator leaves to the ICD do (research.md §2,
 * the profile's driver_implemented list and the calls whose guest state the
 * generated client needs): each works on the calling thread's current context
 * and does nothing without one, as OpenGL does. The entries themselves
 * (manual_entries.cpp) are one line each into this.
 *
 * A map is a guest copy of the range (MappedBuffers); a target names the
 * buffer the host has bound there, which is asked. The GL extensions answered
 * are the context's own list (GlExtensionPolicy). A client-side
 * vertex or index array, which the core profile removed, is
 * GL_INVALID_OPERATION with no host call. */
#ifndef AV_GLICD_MANUAL_CALLS_HPP
#define AV_GLICD_MANUAL_CALLS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::glicd {

class CurrentContexts;
class GuestGlContext;

class ManualCalls {
public:
    enum : uint32_t {
        gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u,
        gl_read_only = 0x88B8u, gl_write_only = 0x88B9u, gl_read_write = 0x88BAu, gl_dynamic_storage_bit = 0x0100u,
        gl_extensions = 0x1F03u, gl_num_extensions = 0x821Du, gl_debug_callback_function = 0x8244u, gl_debug_callback_user_param = 0x8245u,
        gl_vertex_attrib_array_pointer = 0x8645u, gl_buffer_map_pointer = 0x88BDu, gl_uniform = 0x8E17u,
        gl_type = 0x92FAu, gl_invalid_index = 0xFFFFFFFFu, gl_texture_width = 0x1000u, gl_texture_height = 0x1001u,
        gl_texture_depth = 0x8071u, gl_texture_compressed_image_size = 0x86A0u,
    };

    explicit ManualCalls(CurrentContexts &currents);
    ManualCalls(const ManualCalls &) = delete;
    ManualCalls &operator=(const ManualCalls &) = delete;

    /* Buffer mapping. glMapBuffer's access is the enum, the others' the bits. */
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
    /* A call only a direct-state extension this ICD does not report has. */
    void  refuse();

    /* GL_EXT_memory_object and GL_EXT_semaphore over a Win32 handle
     * (feature 053). The first four are answered here and never sent: an
     * object is made by the device inside the import, so a create has nothing
     * to ask the host and an import is the open. The last four *are* sent, but
     * not as the application wrote them - the name it holds is this ICD's and
     * the host knows another, so each substitutes before the generated command
     * goes out, and a name that was never imported names nothing and is
     * GL_INVALID_OPERATION with nothing sent. */
    void  create_memory_objects(int32_t count, uint32_t *objects);
    void  delete_memory_objects(int32_t count, const uint32_t *objects);
    void  import_memory_win32(uint32_t memory, uint64_t size, uint32_t handle_type, void *handle);
    void  gen_semaphores(int32_t count, uint32_t *semaphores);
    void  delete_semaphores(int32_t count, const uint32_t *semaphores);
    void  import_semaphore_win32(uint32_t semaphore, uint32_t handle_type, void *handle);
    void  buffer_storage_mem(uint32_t target, intptr_t size, uint32_t memory, uint64_t offset);
    void  texture_storage_mem_2d(uint32_t texture, int32_t levels, uint32_t format, int32_t width,
                                 int32_t height, uint32_t memory, uint64_t offset);
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
    void buffer_data(uint32_t target, intptr_t size, const void *data, uint32_t usage);
    void buffer_storage(uint32_t target, intptr_t size, const void *data, uint32_t flags);
    void named_buffer_data(uint32_t buffer, intptr_t size, const void *data, uint32_t usage);
    void named_buffer_storage(uint32_t buffer, intptr_t size, const void *data, uint32_t flags);
    void vertex_attrib_pointer(uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride,
                               const void *pointer);
    void enable_vertex_attrib_array(uint32_t index, bool enable);
    void draw_arrays(uint32_t mode, int32_t first, int32_t count, int32_t instances);
    /* `instances` below 0 is a draw that names none (glDrawElements). */
    void draw_elements(uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t instances);
    void draw_range_elements(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                             const void *indices);
    void flush();
    /* The window's framebuffer and buffers, translated for the host
     * (WindowFramebuffers). */
    void bind_framebuffer(uint32_t target, uint32_t framebuffer);
    void draw_buffer(uint32_t buffer);
    void read_buffer(uint32_t buffer);
    uint32_t client_wait_sync(void *sync, uint32_t flags, uint64_t timeout);
    void debug_message_callback(void *callback, const void *user);
    void link_program(uint32_t program);
    void delete_program(uint32_t program);
    int32_t attrib_location(uint32_t program, const char *name);
    int32_t uniform_location(uint32_t program, const char *name);
    /* The bytes a glGetTexImage of the bound texture's `level` writes, its
     * size asked of the host; 0 with nothing current. */
    size_t level_bytes(uint32_t target, int32_t level, uint32_t format, uint32_t type);
    /* GL_TEXTURE_COMPRESSED_IMAGE_SIZE of the bound texture's `level`. */
    size_t compressed_level_bytes(uint32_t target, int32_t level);
    /* glGetIntegerv, but GL_NUM_EXTENSIONS answers the context's list. */
    void           integers(uint32_t pname, int32_t *data);
    /* glReadPixels: a read of the window's GL_FRONT is answered from the frame
     * the swap put there, since the host has no front buffer; everything else
     * goes to the host as it did (feature 051). */
    void           read_pixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                               void *pixels);
    const uint8_t *string(uint32_t name);
    const uint8_t *string_i(uint32_t name, uint32_t index);

private:
    /* The current context's transport's context, when it can send. */
    GuestGlContext *sending();
    /* The context and the host's own name for `name`, or false with the GL
     * error already recorded: a name never imported names no host object, and
     * sending it would name whatever the host has under that number. */
    bool            substituted(uint32_t kind, uint32_t name, GuestGlContext *&context, uint32_t &made);
    void            record(uint32_t error);
    /* Whether a draw's indices can cross: an offset, or none. */
    bool            indices_cross(const void *indices);
    /* The buffer the host has bound at `target`; false, with
     * GL_INVALID_ENUM recorded, for a target that binds no buffer. */
    bool            bound_to(GuestGlContext &context, uint32_t target, uint32_t &buffer);
    void           *map_name(GuestGlContext &context, uint32_t buffer, int64_t offset, int64_t length,
                             uint32_t access);
    bool            unmap_name(GuestGlContext &context, uint32_t buffer);
    void            mark_name(GuestGlContext &context, uint32_t buffer, intptr_t offset, intptr_t length);

    CurrentContexts &currents_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_MANUAL_CALLS_HPP

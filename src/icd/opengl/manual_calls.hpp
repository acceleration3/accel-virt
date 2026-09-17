/* What the OpenGL entries the generator leaves to the ICD do (research.md §2,
 * the profile's driver_implemented list and the calls whose guest state the
 * generated client needs): each works on the calling thread's current context
 * and does nothing without one, as OpenGL does. The entries themselves
 * (manual_entries.cpp) are one line each into this.
 *
 * Until its task arrives, buffer mapping (T036) records GL_INVALID_OPERATION
 * and maps nothing. The GL extensions answered are the context's own list
 * (GlExtensionPolicy). A client-side
 * vertex or index array, which the core profile removed, is
 * GL_INVALID_OPERATION with no host call. */
#ifndef AV_GLICD_MANUAL_CALLS_HPP
#define AV_GLICD_MANUAL_CALLS_HPP

#include <stdint.h>

namespace av::glicd {

class CurrentContexts;
class GuestGlContext;

class ManualCalls {
public:
    enum : uint32_t {
        gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u,
        gl_extensions = 0x1F03u, gl_num_extensions = 0x821Du, gl_debug_callback_function = 0x8244u, gl_debug_callback_user_param = 0x8245u,
        gl_vertex_attrib_array_pointer = 0x8645u, gl_buffer_map_pointer = 0x88BDu, gl_uniform = 0x8E17u,
        gl_type = 0x92FAu, gl_invalid_index = 0xFFFFFFFFu,
    };

    explicit ManualCalls(CurrentContexts &currents);
    ManualCalls(const ManualCalls &) = delete;
    ManualCalls &operator=(const ManualCalls &) = delete;

    /* Buffer mapping, and the queries of pointers only a mapping or a
     * direct-state extension would have. */
    void *refuse_map();
    bool  refuse_unmap();
    void  refuse();
    void  buffer_pointer(uint32_t pname, void **params);
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
    /* glGetIntegerv, but GL_NUM_EXTENSIONS answers the context's list. */
    void           integers(uint32_t pname, int32_t *data);
    const uint8_t *string(uint32_t name);
    const uint8_t *string_i(uint32_t name, uint32_t index);

private:
    /* The current context's transport's context, when it can send. */
    GuestGlContext *sending();
    void            record(uint32_t error);
    /* Whether a draw's indices can cross: an offset, or none. */
    bool            indices_cross(const void *indices);

    CurrentContexts &currents_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_MANUAL_CALLS_HPP

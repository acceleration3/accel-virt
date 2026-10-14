#include "icd/opengl/manual_calls.hpp"

#include "icd/opengl/gl_external_objects.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include "proto/client/gl_icd_support.hpp"

#include "icd/opengl/granted_profile.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/opengl/guest_gl_context.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

uint64_t address_of(const void *pointer)
{
    AV_SCOPE("address_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer));
}

/* Each buffer target and the query that names what is bound there. */
struct TargetBinding {
    uint32_t target;
    uint32_t binding;
};

constexpr TargetBinding target_bindings[] = {
    {0x8892u, 0x8894u}, {0x8893u, 0x8895u}, {0x88EBu, 0x88EDu}, {0x88ECu, 0x88EFu}, {0x8A11u, 0x8A28u},
    {0x8C2Au, 0x8C2Bu}, {0x8C8Eu, 0x8C8Fu}, {0x8F36u, 0x8F36u}, {0x8F37u, 0x8F37u}, {0x8F3Fu, 0x8F43u},
    {0x90D2u, 0x90D3u}, {0x90EEu, 0x90EFu}, {0x9192u, 0x9193u}, {0x92C0u, 0x92C1u},
};

/* The targets whose indexed bindings a shader writes through: transform
 * feedback, shader storage and atomic counters. */
bool shader_writable(uint32_t target)
{
    AV_SCOPE("shader_writable");
    return target == 0x8C8Eu || target == 0x90D2u || target == 0x92C0u;
}

/* glMapBuffer's access enum as glMapBufferRange's bits; 0 for no such enum. */
uint32_t access_bits(uint32_t access)
{
    AV_SCOPE("access_bits");
    if (access == ManualCalls::gl_read_only) return BufferMapping::map_read;
    if (access == ManualCalls::gl_write_only) return BufferMapping::map_write;
    if (access == ManualCalls::gl_read_write) return BufferMapping::map_read | BufferMapping::map_write;
    return 0u;
}

}  // namespace

ManualCalls::ManualCalls(CurrentContexts &currents) : currents_(currents)
{
    AV_SCOPE("ManualCalls::ManualCalls");
}

void *ManualCalls::map_buffer(uint32_t target, uint32_t access)
{
    AV_SCOPE("ManualCalls::map_buffer");
    GuestGlContext *const context = sending();
    uint32_t              buffer = 0u;
    if (context == nullptr || !bound_to(*context, target, buffer)) return nullptr;
    const uint32_t bits = access_bits(access);
    if (bits == 0u) {
        context->record(gl_invalid_enum);
        return nullptr;
    }
    return map_name(*context, buffer, 0, MappedBuffers::whole_buffer, bits);
}

void *ManualCalls::map_buffer_range(uint32_t target, intptr_t offset, intptr_t length, uint32_t access)
{
    AV_SCOPE("ManualCalls::map_buffer_range");
    GuestGlContext *const context = sending();
    uint32_t              buffer = 0u;
    if (context == nullptr || !bound_to(*context, target, buffer)) return nullptr;
    /* A negative length is not glMapBuffer's whole buffer. */
    if (length < 0) {
        context->record(gl_invalid_value);
        return nullptr;
    }
    return map_name(*context, buffer, offset, length, access);
}

void *ManualCalls::map_named_buffer(uint32_t buffer, uint32_t access)
{
    AV_SCOPE("ManualCalls::map_named_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return nullptr;
    const uint32_t bits = access_bits(access);
    if (bits == 0u) {
        context->record(gl_invalid_enum);
        return nullptr;
    }
    return map_name(*context, buffer, 0, MappedBuffers::whole_buffer, bits);
}

void *ManualCalls::map_named_buffer_range(uint32_t buffer, intptr_t offset, intptr_t length, uint32_t access)
{
    AV_SCOPE("ManualCalls::map_named_buffer_range");
    GuestGlContext *const context = sending();
    if (context == nullptr) return nullptr;
    if (length < 0) {
        context->record(gl_invalid_value);
        return nullptr;
    }
    return map_name(*context, buffer, offset, length, access);
}

bool ManualCalls::unmap_buffer(uint32_t target)
{
    AV_SCOPE("ManualCalls::unmap_buffer");
    GuestGlContext *const context = sending();
    uint32_t              buffer = 0u;
    return context != nullptr && bound_to(*context, target, buffer) && unmap_name(*context, buffer);
}

bool ManualCalls::unmap_named_buffer(uint32_t buffer)
{
    AV_SCOPE("ManualCalls::unmap_named_buffer");
    GuestGlContext *const context = sending();
    return context != nullptr && unmap_name(*context, buffer);
}

void ManualCalls::flush_mapped_range(uint32_t target, intptr_t offset, intptr_t length)
{
    AV_SCOPE("ManualCalls::flush_mapped_range");
    GuestGlContext *const context = sending();
    uint32_t              buffer = 0u;
    if (context != nullptr && bound_to(*context, target, buffer)) mark_name(*context, buffer, offset, length);
}

void ManualCalls::flush_mapped_named_range(uint32_t buffer, intptr_t offset, intptr_t length)
{
    AV_SCOPE("ManualCalls::flush_mapped_named_range");
    GuestGlContext *const context = sending();
    if (context != nullptr) mark_name(*context, buffer, offset, length);
}

void ManualCalls::refuse()
{
    AV_SCOPE("ManualCalls::refuse");
    record(gl_invalid_operation);
}

void ManualCalls::wrote_buffer(uint32_t target, intptr_t offset, intptr_t size, const void *data)
{
    AV_SCOPE("ManualCalls::wrote_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t buffer = context->arrays().bound_buffer(target);
    if (buffer == ClientArrayState::unknown)
        context->shadows().forget_all();
    else if (offset >= 0 && size >= 0)
        context->shadows().wrote(buffer, static_cast<uint64_t>(offset), static_cast<uint64_t>(size), data);
}

void ManualCalls::wrote_named_buffer(uint32_t buffer, intptr_t offset, intptr_t size, const void *data)
{
    AV_SCOPE("ManualCalls::wrote_named_buffer");
    GuestGlContext *const context = sending();
    if (context != nullptr && offset >= 0 && size >= 0)
        context->shadows().wrote(buffer, static_cast<uint64_t>(offset), static_cast<uint64_t>(size), data);
}

void ManualCalls::host_writes_target(uint32_t target)
{
    AV_SCOPE("ManualCalls::host_writes_target");
    GuestGlContext *const context = sending();
    if (context != nullptr) forget_bound(*context, target);
}

void ManualCalls::host_writes_buffer(uint32_t buffer)
{
    AV_SCOPE("ManualCalls::host_writes_buffer");
    GuestGlContext *const context = sending();
    if (context != nullptr) context->shadows().forget(buffer);
}

void ManualCalls::bind_buffer_indexed(uint32_t target, uint32_t buffer)
{
    AV_SCOPE("ManualCalls::bind_buffer_indexed");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().bind_buffer(target, buffer);
    if (buffer != 0u && shader_writable(target)) context->shadows().distrust();
}

void ManualCalls::bind_buffers_indexed(uint32_t target)
{
    AV_SCOPE("ManualCalls::bind_buffers_indexed");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().lose_buffer(target);
    if (shader_writable(target)) context->shadows().distrust();
}

void ManualCalls::shaders_write_buffers()
{
    AV_SCOPE("ManualCalls::shaders_write_buffers");
    GuestGlContext *const context = sending();
    if (context != nullptr) context->shadows().distrust();
}

void ManualCalls::bindings_restored()
{
    AV_SCOPE("ManualCalls::bindings_restored");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().restore_buffers();
    context->shadows().forget_all();
}

void ManualCalls::buffer_pointer(uint32_t target, uint32_t pname, void **params)
{
    AV_SCOPE("ManualCalls::buffer_pointer");
    GuestGlContext *const context = sending();
    if (context == nullptr || params == nullptr) return;
    if (pname != gl_buffer_map_pointer) return context->record(gl_invalid_enum);
    uint32_t buffer = 0u;
    if (!bound_to(*context, target, buffer)) return;
    if (buffer == 0u) return context->record(gl_invalid_operation);
    *params = context->mappings().pointer_of(buffer);
}

void ManualCalls::named_buffer_pointer(uint32_t buffer, uint32_t pname, void **params)
{
    AV_SCOPE("ManualCalls::named_buffer_pointer");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || params == nullptr) return;
    if (pname != gl_buffer_map_pointer) return context->record(gl_invalid_enum);
    *params = context->mappings().pointer_of(buffer);
}

/* The name is judged before the target, which costs a round trip to resolve. */
bool ManualCalls::buffer_map_state(uint32_t target, uint32_t pname, int64_t &value)
{
    AV_SCOPE("ManualCalls::buffer_map_state");
    GuestGlContext *const context = sending();
    int64_t               unmapped = 0;
    if (context == nullptr || !context->mappings().map_state(0u, pname, unmapped)) return false;
    uint32_t buffer = 0u;
    if (!bound_to(*context, target, buffer)) return true;
    if (buffer == 0u) {
        context->record(gl_invalid_operation);
        return true;
    }
    return context->mappings().map_state(buffer, pname, value);
}

bool ManualCalls::named_buffer_map_state(uint32_t buffer, uint32_t pname, int64_t &value)
{
    AV_SCOPE("ManualCalls::named_buffer_map_state");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || buffer == 0u) return false;
    return context->mappings().map_state(buffer, pname, value);
}

void ManualCalls::attribute_pointer(uint32_t index, uint32_t pname, void **pointer)
{
    AV_SCOPE("ManualCalls::attribute_pointer");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || pointer == nullptr) return;
    if (pname != gl_vertex_attrib_array_pointer) return record(gl_invalid_enum);
    if (index >= ClientArrayState::max_attributes) return record(gl_invalid_value);
    *pointer = reinterpret_cast<void *>(static_cast<uintptr_t>(context->arrays().attribute_pointer(index)));
}

void ManualCalls::pointer(uint32_t pname, void **params)
{
    AV_SCOPE("ManualCalls::pointer");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || params == nullptr) return;
    if (pname == gl_debug_callback_function) {
        *params = reinterpret_cast<void *>(static_cast<uintptr_t>(context->debug_callback()));
        return;
    }
    if (pname != gl_debug_callback_user_param) return record(gl_invalid_enum);
    *params = reinterpret_cast<void *>(static_cast<uintptr_t>(context->debug_user()));
}

void ManualCalls::pixel_store(uint32_t pname, int32_t value)
{
    AV_SCOPE("ManualCalls::pixel_store");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    static_cast<void>(context->arrays().store_pixel(pname, value));
    glproto::av_glPixelStorei(*context->wire(), pname, value);
}

void ManualCalls::pixel_store(uint32_t pname, float value)
{
    AV_SCOPE("ManualCalls::pixel_store");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    /* A float store is rounded as OpenGL rounds it (§18.2.2). */
    const float rounded = value < 0.0f ? value - 0.5f : value + 0.5f;
    if (rounded > -2147483648.0f && rounded < 2147483647.0f)
        static_cast<void>(context->arrays().store_pixel(pname, static_cast<int32_t>(rounded)));
    glproto::av_glPixelStoref(*context->wire(), pname, value);
}

void ManualCalls::bind_buffer(uint32_t target, uint32_t buffer)
{
    AV_SCOPE("ManualCalls::bind_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().bind_buffer(target, buffer);
    glproto::av_glBindBuffer(*context->wire(), target, buffer);
}

void ManualCalls::delete_buffers(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("ManualCalls::delete_buffers");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().delete_buffers(count, buffers);
    context->mappings().forget(count, buffers);
    context->shadows().forget(count, buffers);
    glproto::av_glDeleteBuffers(*context->wire(), count, buffers);
}

void ManualCalls::gen_buffers(int32_t count, uint32_t *buffers)
{
    AV_SCOPE("ManualCalls::gen_buffers");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::av_glGenBuffers(*context->wire(), count, buffers);
}

void ManualCalls::gen_textures(int32_t count, uint32_t *textures)
{
    AV_SCOPE("ManualCalls::gen_textures");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::av_glGenTextures(*context->wire(), count, textures);
}

void ManualCalls::gen_vertex_arrays(int32_t count, uint32_t *arrays)
{
    AV_SCOPE("ManualCalls::gen_vertex_arrays");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::av_glGenVertexArrays(*context->wire(), count, arrays);
}

void ManualCalls::bind_vertex_array(uint32_t array)
{
    AV_SCOPE("ManualCalls::bind_vertex_array");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().bind_vertex_array(array);
    glproto::av_glBindVertexArray(*context->wire(), array);
}

void ManualCalls::delete_vertex_arrays(int32_t count, const uint32_t *arrays)
{
    AV_SCOPE("ManualCalls::delete_vertex_arrays");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().delete_vertex_arrays(count, arrays);
    glproto::av_glDeleteVertexArrays(*context->wire(), count, arrays);
}

void ManualCalls::buffer_data(uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    AV_SCOPE("ManualCalls::buffer_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    respecify_bound(*context, target, size, data != nullptr);
    glproto::av_glBufferData(*context->wire(), target, size, data, usage);
}

void ManualCalls::buffer_storage(uint32_t target, intptr_t size, const void *data, uint32_t flags)
{
    AV_SCOPE("ManualCalls::buffer_storage");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    respecify_bound(*context, target, size, data != nullptr);
    glproto::av_glBufferStorage(*context->wire(), target, size, data, flags | gl_dynamic_storage_bit);
}

void ManualCalls::named_buffer_data(uint32_t buffer, intptr_t size, const void *data, uint32_t usage)
{
    AV_SCOPE("ManualCalls::named_buffer_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->shadows().respecify(buffer, size, data != nullptr);
    glproto::av_glNamedBufferData(*context->wire(), buffer, size, data, usage);
}

void ManualCalls::named_buffer_storage(uint32_t buffer, intptr_t size, const void *data, uint32_t flags)
{
    AV_SCOPE("ManualCalls::named_buffer_storage");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->shadows().respecify(buffer, size, data != nullptr);
    glproto::av_glNamedBufferStorage(*context->wire(), buffer, size, data, flags | gl_dynamic_storage_bit);
}

void ManualCalls::vertex_attrib_pointer(uint32_t index, int32_t size, uint32_t type, uint8_t normalized,
                                        int32_t stride, const void *pointer)
{
    AV_SCOPE("ManualCalls::vertex_attrib_pointer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->arrays().client_pointer(address_of(pointer))) return context->record(gl_invalid_operation);
    context->arrays().set_attribute_pointer(index, address_of(pointer));
    glproto::av_glVertexAttribPointer(*context->wire(), index, size, type, normalized, stride, address_of(pointer));
}

void ManualCalls::enable_vertex_attrib_array(uint32_t index, bool enable)
{
    AV_SCOPE("ManualCalls::enable_vertex_attrib_array");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (enable) return glproto::av_glEnableVertexAttribArray(*context->wire(), index);
    glproto::av_glDisableVertexAttribArray(*context->wire(), index);
}

void ManualCalls::draw_arrays(uint32_t mode, int32_t first, int32_t count, int32_t instances)
{
    AV_SCOPE("ManualCalls::draw_arrays");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->mappings().flush_persistent(*context->wire());
    if (instances < 0) return glproto::av_glDrawArrays(*context->wire(), mode, first, count);
    glproto::av_glDrawArraysInstanced(*context->wire(), mode, first, count, instances);
}

void ManualCalls::draw_elements(uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t instances)
{
    AV_SCOPE("ManualCalls::draw_elements");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!indices_cross(indices)) return context->record(gl_invalid_operation);
    context->mappings().flush_persistent(*context->wire());
    if (instances < 0) return glproto::av_glDrawElements(*context->wire(), mode, count, type, address_of(indices));
    glproto::av_glDrawElementsInstanced(*context->wire(), mode, count, type, address_of(indices), instances);
}

void ManualCalls::draw_range_elements(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                                      const void *indices)
{
    AV_SCOPE("ManualCalls::draw_range_elements");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!indices_cross(indices)) return context->record(gl_invalid_operation);
    context->mappings().flush_persistent(*context->wire());
    glproto::av_glDrawRangeElements(*context->wire(), mode, start, end, count, type, address_of(indices));
}

void ManualCalls::flush()
{
    AV_SCOPE("ManualCalls::flush");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    glproto::av_glFlush(*context->wire());
    context->end_frame();
}

/* GL_WAIT_FAILED (0x911D) with nothing to wait on. */
uint32_t ManualCalls::client_wait_sync(void *sync, uint32_t flags, uint64_t timeout)
{
    AV_SCOPE("ManualCalls::client_wait_sync");
    static constexpr uint32_t wait_failed = 0x911Du;
    GuestGlContext *const context = sending();
    if (context == nullptr) return wait_failed;
    return glproto::av_glClientWaitSync(*context->wire(), address_of(sync), flags, timeout);
}

/* The host's debug messages are not carried back yet: the callback is kept
 * for the pointer query and never called. */
void ManualCalls::debug_message_callback(void *callback, const void *user)
{
    AV_SCOPE("ManualCalls::debug_message_callback");
    GuestGlContext *const context = currents_.current();
    if (context != nullptr) context->set_debug_callback(address_of(callback), address_of(user));
}

void ManualCalls::link_program(uint32_t program)
{
    AV_SCOPE("ManualCalls::link_program");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->uniforms().forget(program);
    glproto::av_glLinkProgram(*context->wire(), program);
}

void ManualCalls::delete_program(uint32_t program)
{
    AV_SCOPE("ManualCalls::delete_program");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->uniforms().forget(program);
    glproto::av_glDeleteProgram(*context->wire(), program);
}

int32_t ManualCalls::attrib_location(uint32_t program, const char *name)
{
    AV_SCOPE("ManualCalls::attrib_location");
    GuestGlContext *const context = sending();
    return context != nullptr ? glproto::av_glGetAttribLocation(*context->wire(), program, name) : -1;
}

/* The location's type is asked at once, while nothing else is being sent, so
 * a later glGetUniform can size its answer. */
int32_t ManualCalls::uniform_location(uint32_t program, const char *name)
{
    AV_SCOPE("ManualCalls::uniform_location");
    GuestGlContext *const context = sending();
    if (context == nullptr) return -1;
    ClientTransport &wire = *context->wire();
    const int32_t    location = glproto::av_glGetUniformLocation(wire, program, name);
    if (location < 0) return location;
    const uint32_t index = glproto::av_glGetProgramResourceIndex(wire, program, gl_uniform, name);
    if (index == gl_invalid_index) return location;
    const uint32_t property = gl_type;
    int32_t        written = 0;
    int32_t        type = 0;
    glproto::av_glGetProgramResourceiv(wire, program, gl_uniform, index, 1, &property, 1, &written, &type);
    if (written == 1) context->uniforms().remember(program, location, static_cast<uint32_t>(type));
    return location;
}

size_t ManualCalls::level_bytes(uint32_t target, int32_t level, uint32_t format, uint32_t type)
{
    AV_SCOPE("ManualCalls::level_bytes");
    GuestGlContext *const context = sending();
    if (context == nullptr) return 0u;
    ClientTransport &wire = *context->wire();
    int32_t          width = 0, height = 0, depth = 0;
    glproto::av_glGetTexLevelParameteriv(wire, target, level, gl_texture_width, &width);
    glproto::av_glGetTexLevelParameteriv(wire, target, level, gl_texture_height, &height);
    glproto::av_glGetTexLevelParameteriv(wire, target, level, gl_texture_depth, &depth);
    return context->arrays().pixel_bytes(format, type, width, height, depth, true);
}

size_t ManualCalls::compressed_level_bytes(uint32_t target, int32_t level)
{
    AV_SCOPE("ManualCalls::compressed_level_bytes");
    GuestGlContext *const context = sending();
    if (context == nullptr) return 0u;
    int32_t size = 0;
    glproto::av_glGetTexLevelParameteriv(*context->wire(), target, level, gl_texture_compressed_image_size, &size);
    return size > 0 ? static_cast<size_t>(size) : 0u;
}

void ManualCalls::integers(uint32_t pname, int32_t *data)
{
    AV_SCOPE("ManualCalls::integers");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (pname == gl_num_extensions) {
        if (data != nullptr) *data = static_cast<int32_t>(context->extensions().count());
        return;
    }
    /* What the host granted was learned when the context was made, so these
     * three are answered from it rather than asked again (feature 051). */
    if (pname == GrantedProfile::mask_query || pname == GrantedProfile::major_query ||
        pname == GrantedProfile::minor_query) {
        if (data == nullptr) return;
        const GrantedProfile &profile = context->profile();
        *data = static_cast<int32_t>(pname == GrantedProfile::mask_query    ? profile.mask()
                                     : pname == GrantedProfile::major_query ? profile.major()
                                                                            : profile.minor());
        return;
    }
    glproto::av_glGetIntegerv(*context->wire(), pname, data);
    if (data != nullptr) *data = context->framebuffers().answer(pname, *data);
}

void ManualCalls::bind_framebuffer(uint32_t target, uint32_t framebuffer)
{
    AV_SCOPE("ManualCalls::bind_framebuffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    WindowFramebuffers &framebuffers = context->framebuffers();
    framebuffers.bound(target, framebuffer);
    glproto::av_glBindFramebuffer(*context->wire(), target, framebuffers.name_of(framebuffer));
}

void ManualCalls::draw_buffer(uint32_t buffer)
{
    AV_SCOPE("ManualCalls::draw_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    glproto::av_glDrawBuffer(*context->wire(), context->framebuffers().buffer_for(buffer, true));
}

void ManualCalls::read_buffer(uint32_t buffer)
{
    AV_SCOPE("ManualCalls::read_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->framebuffers().reading(buffer);
    glproto::av_glReadBuffer(*context->wire(), context->framebuffers().buffer_for(buffer, false));
}

void ManualCalls::read_pixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                              void *pixels)
{
    AV_SCOPE("ManualCalls::read_pixels");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->framebuffers().read_front(x, y, width, height, format, type, pixels)) return;
    const bool offset = glicd::pixel_buffer_bound(true);
    if (offset) forget_bound(*context, ClientArrayState::pixel_pack_buffer);
    glproto::av_glReadPixels(*context->wire(), x, y, width, height, format, type, pixels,
                              offset ? 0u : glicd::pixel_bytes(format, type, width, height, 1, true), offset);
}

const uint8_t *ManualCalls::string(uint32_t name)
{
    AV_SCOPE("ManualCalls::string");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return nullptr;
    if (name == gl_extensions) return reinterpret_cast<const uint8_t *>(context->extensions().joined());
    const char *const answer = context->string(name);
    if (answer == nullptr) context->record(gl_invalid_enum);
    return reinterpret_cast<const uint8_t *>(answer);
}

/* GL_EXTENSIONS is the one indexed string this ICD has; an index past the
 * context's list is GL_INVALID_VALUE. */
const uint8_t *ManualCalls::string_i(uint32_t name, uint32_t index)
{
    AV_SCOPE("ManualCalls::string_i");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return nullptr;
    const char *const answer = name == gl_extensions ? context->extensions().name(index) : nullptr;
    if (answer == nullptr) context->record(name == gl_extensions ? gl_invalid_value : gl_invalid_enum);
    return reinterpret_cast<const uint8_t *>(answer);
}

/* ---- GL_EXT_memory_object and GL_EXT_semaphore (feature 053) ------------- */

void ManualCalls::create_memory_objects(int32_t count, uint32_t *objects)
{
    AV_SCOPE("ManualCalls::create_memory_objects");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t error = context->externals().create(SharedObjectKind::memory, count, objects);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

void ManualCalls::gen_semaphores(int32_t count, uint32_t *semaphores)
{
    AV_SCOPE("ManualCalls::gen_semaphores");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t error = context->externals().create(SharedObjectKind::monitored_fence, count, semaphores);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

void ManualCalls::import_memory_win32(uint32_t memory, uint64_t size, uint32_t handle_type, void *handle)
{
    AV_SCOPE("ManualCalls::import_memory_win32");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t error = context->externals().import(*context->wire(), context->guest_device(),
                                                       SharedObjectKind::memory, memory, size, handle_type, handle);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

void ManualCalls::import_semaphore_win32(uint32_t semaphore, uint32_t handle_type, void *handle)
{
    AV_SCOPE("ManualCalls::import_semaphore_win32");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t error = context->externals().import(*context->wire(), context->guest_device(),
                                                       SharedObjectKind::monitored_fence, semaphore, 0u, handle_type,
                                                       handle);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

void ManualCalls::delete_memory_objects(int32_t count, const uint32_t *objects)
{
    AV_SCOPE("ManualCalls::delete_memory_objects");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t error =
        context->externals().destroy(*context->wire(), SharedObjectKind::memory, count, objects);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

void ManualCalls::delete_semaphores(int32_t count, const uint32_t *semaphores)
{
    AV_SCOPE("ManualCalls::delete_semaphores");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t error =
        context->externals().destroy(*context->wire(), SharedObjectKind::monitored_fence, count, semaphores);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

bool ManualCalls::substituted(uint32_t kind, uint32_t name, GuestGlContext *&context, uint32_t &made)
{
    AV_SCOPE("ManualCalls::substituted");
    context = sending();
    if (context == nullptr) return false;
    made = context->externals().made(kind, name);
    /* A name this context never imported names no host object. Sending it
     * would name whatever the host happens to hold under that number, which is
     * another application's object or none. */
    if (made == 0u) {
        context->record(gl_invalid_operation);
        return false;
    }
    return true;
}

/* The queries and parameters the two extensions promise. Every one of them is
 * about an object the host has not been told about - a name this ICD chose -
 * so the table is what answers, and none of them crosses the wire. */
bool ManualCalls::is_memory_object(uint32_t memory)
{
    AV_SCOPE("ManualCalls::is_memory_object");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    return context->externals().holds(SharedObjectKind::memory, memory);
}

bool ManualCalls::is_semaphore(uint32_t semaphore)
{
    AV_SCOPE("ManualCalls::is_semaphore");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    return context->externals().holds(SharedObjectKind::monitored_fence, semaphore);
}

void ManualCalls::memory_object_parameter(uint32_t memory, uint32_t pname, const int32_t *params)
{
    AV_SCOPE("ManualCalls::memory_object_parameter");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (params == nullptr) {
        context->record(gl_invalid_value);
        return;
    }
    const uint32_t error =
        context->externals().set_parameter(SharedObjectKind::memory, memory, pname, params[0]);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

void ManualCalls::get_memory_object_parameter(uint32_t memory, uint32_t pname, int32_t *params)
{
    AV_SCOPE("ManualCalls::get_memory_object_parameter");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t error =
        context->externals().get_parameter(SharedObjectKind::memory, memory, pname, params);
    if (error != GlExternalObjects::gl_no_error) context->record(error);
}

void ManualCalls::semaphore_parameter(uint32_t semaphore, uint32_t pname, const uint64_t *params)
{
    AV_SCOPE("ManualCalls::semaphore_parameter");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    static_cast<void>(params);
    /* Not branched on: a semaphore's only parameter names a D3D12 fence's
     * value and nothing here is imported from one, so the table refuses every
     * pname for a row of that kind - an unknown name with GL_INVALID_VALUE and
     * a known one with GL_INVALID_ENUM. There is no answer but a refusal. */
    context->record(
        context->externals().set_parameter(SharedObjectKind::monitored_fence, semaphore, pname, 0));
}

void ManualCalls::get_semaphore_parameter(uint32_t semaphore, uint32_t pname, uint64_t *params)
{
    AV_SCOPE("ManualCalls::get_semaphore_parameter");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    int32_t unused = 0;
    /* As above: always a refusal, so the answer is recorded rather than
     * tested. The query is still asked with `unused` or with nothing, because
     * a caller that passed nowhere to write is its own refusal. */
    context->record(context->externals().get_parameter(SharedObjectKind::monitored_fence, semaphore, pname,
                                                       params != nullptr ? &unused : nullptr));
}

void ManualCalls::import_memory_win32_name(uint32_t memory, uint64_t size, uint32_t handle_type,
                                           const void *name)
{
    AV_SCOPE("ManualCalls::import_memory_win32_name");
    static_cast<void>(memory);
    static_cast<void>(size);
    static_cast<void>(handle_type);
    static_cast<void>(name);
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    /* A named section is what another process opens *by name*, and nothing on
     * this wire carries a name: an object crosses as an id the miniport minted
     * and a handle the kernel made (feature 053). */
    context->record(gl_invalid_operation);
}

void ManualCalls::import_semaphore_win32_name(uint32_t semaphore, uint32_t handle_type, const void *name)
{
    AV_SCOPE("ManualCalls::import_semaphore_win32_name");
    static_cast<void>(semaphore);
    static_cast<void>(handle_type);
    static_cast<void>(name);
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->record(gl_invalid_operation);
}

void ManualCalls::buffer_storage_mem(uint32_t target, intptr_t size, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::buffer_storage_mem");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    /* Another API writes imported memory whenever it likes. */
    context->shadows().distrust();
    glproto::av_glBufferStorageMemEXT(*context->wire(), target, size, made, offset);
}

void ManualCalls::texture_storage_mem_2d(uint32_t texture, int32_t levels, uint32_t format, int32_t width,
                                         int32_t height, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::texture_storage_mem_2d");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTextureStorageMem2DEXT(*context->wire(), texture, levels, format, width, height, made,
                                          offset);
}

void ManualCalls::named_buffer_storage_mem(uint32_t buffer, intptr_t size, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::named_buffer_storage_mem");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    context->shadows().distrust();
    glproto::av_glNamedBufferStorageMemEXT(*context->wire(), buffer, size, made, offset);
}

void ManualCalls::tex_storage_mem_1d(uint32_t target, int32_t levels, uint32_t format, int32_t width, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::tex_storage_mem_1d");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTexStorageMem1DEXT(*context->wire(), target, levels, format, width, made, offset);
}

void ManualCalls::tex_storage_mem_2d(uint32_t target, int32_t levels, uint32_t format, int32_t width, int32_t height, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::tex_storage_mem_2d");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTexStorageMem2DEXT(*context->wire(), target, levels, format, width, height, made, offset);
}

void ManualCalls::tex_storage_mem_2d_multisample(uint32_t target, int32_t samples, uint32_t format, int32_t width, int32_t height, bool fixed_locations, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::tex_storage_mem_2d_multisample");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTexStorageMem2DMultisampleEXT(*context->wire(), target, samples, format, width, height, fixed_locations, made, offset);
}

void ManualCalls::tex_storage_mem_3d(uint32_t target, int32_t levels, uint32_t format, int32_t width, int32_t height, int32_t depth, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::tex_storage_mem_3d");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTexStorageMem3DEXT(*context->wire(), target, levels, format, width, height, depth, made, offset);
}

void ManualCalls::tex_storage_mem_3d_multisample(uint32_t target, int32_t samples, uint32_t format, int32_t width, int32_t height, int32_t depth, bool fixed_locations, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::tex_storage_mem_3d_multisample");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTexStorageMem3DMultisampleEXT(*context->wire(), target, samples, format, width, height, depth, fixed_locations, made, offset);
}

void ManualCalls::texture_storage_mem_1d(uint32_t texture, int32_t levels, uint32_t format, int32_t width, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::texture_storage_mem_1d");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTextureStorageMem1DEXT(*context->wire(), texture, levels, format, width, made, offset);
}

void ManualCalls::texture_storage_mem_2d_multisample(uint32_t texture, int32_t samples, uint32_t format, int32_t width, int32_t height, bool fixed_locations, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::texture_storage_mem_2d_multisample");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTextureStorageMem2DMultisampleEXT(*context->wire(), texture, samples, format, width, height, fixed_locations, made, offset);
}

void ManualCalls::texture_storage_mem_3d(uint32_t texture, int32_t levels, uint32_t format, int32_t width, int32_t height, int32_t depth, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::texture_storage_mem_3d");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTextureStorageMem3DEXT(*context->wire(), texture, levels, format, width, height, depth, made, offset);
}

void ManualCalls::texture_storage_mem_3d_multisample(uint32_t texture, int32_t samples, uint32_t format, int32_t width, int32_t height, int32_t depth, bool fixed_locations, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("ManualCalls::texture_storage_mem_3d_multisample");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::memory, memory, context, made)) return;
    glproto::av_glTextureStorageMem3DMultisampleEXT(*context->wire(), texture, samples, format, width, height, depth, fixed_locations, made, offset);
}

void ManualCalls::signal_semaphore(uint32_t semaphore, uint32_t buffer_barriers, const uint32_t *buffers,
                                   uint32_t texture_barriers, const uint32_t *textures,
                                   const uint32_t *layouts)
{
    AV_SCOPE("ManualCalls::signal_semaphore");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::monitored_fence, semaphore, context, made)) return;
    glproto::av_glSignalSemaphoreEXT(*context->wire(), made, buffer_barriers, buffers, texture_barriers,
                                      textures, layouts);
}

void ManualCalls::wait_semaphore(uint32_t semaphore, uint32_t buffer_barriers, const uint32_t *buffers,
                                 uint32_t texture_barriers, const uint32_t *textures,
                                 const uint32_t *layouts)
{
    AV_SCOPE("ManualCalls::wait_semaphore");
    GuestGlContext *context = nullptr;
    uint32_t        made = 0u;
    if (!substituted(SharedObjectKind::monitored_fence, semaphore, context, made)) return;
    glproto::av_glWaitSemaphoreEXT(*context->wire(), made, buffer_barriers, buffers, texture_barriers,
                                    textures, layouts);
}

GuestGlContext *ManualCalls::sending()
{
    AV_SCOPE("ManualCalls::sending");
    GuestGlContext *const context = currents_.current();
    return context != nullptr && context->wire() != nullptr ? context : nullptr;
}

void ManualCalls::record(uint32_t error)
{
    AV_SCOPE("ManualCalls::record");
    GuestGlContext *const context = currents_.current();
    if (context != nullptr) context->record(error);
}

bool ManualCalls::bound_to(GuestGlContext &context, uint32_t target, uint32_t &buffer)
{
    AV_SCOPE("ManualCalls::bound_to");
    for (const TargetBinding &row : target_bindings) {
        if (row.target != target) continue;
        /* Asking would wait for the host to run everything queued ahead. */
        if (context.arrays().known_buffer(target, buffer)) return true;
        int32_t bound = 0;
        glproto::av_glGetIntegerv(*context.wire(), row.binding, &bound);
        buffer = static_cast<uint32_t>(bound);
        return true;
    }
    context.record(gl_invalid_enum);
    return false;
}

void *ManualCalls::map_name(GuestGlContext &context, uint32_t buffer, int64_t offset, int64_t length,
                            uint32_t access)
{
    AV_SCOPE("ManualCalls::map_name");
    void          *pointer = nullptr;
    const uint32_t error = context.mappings().map(*context.wire(), buffer, offset, length, access, pointer);
    if (error != MappedBuffers::gl_no_error) context.record(error);
    return pointer;
}

bool ManualCalls::unmap_name(GuestGlContext &context, uint32_t buffer)
{
    AV_SCOPE("ManualCalls::unmap_name");
    const uint32_t error = context.mappings().unmap(*context.wire(), buffer);
    if (error != MappedBuffers::gl_no_error) context.record(error);
    return error == MappedBuffers::gl_no_error;
}

void ManualCalls::mark_name(GuestGlContext &context, uint32_t buffer, intptr_t offset, intptr_t length)
{
    AV_SCOPE("ManualCalls::mark_name");
    const uint32_t error = context.mappings().mark(buffer, offset, length);
    if (error != MappedBuffers::gl_no_error) context.record(error);
}

void ManualCalls::forget_bound(GuestGlContext &context, uint32_t target)
{
    AV_SCOPE("ManualCalls::forget_bound");
    const uint32_t buffer = context.arrays().bound_buffer(target);
    if (buffer == ClientArrayState::unknown)
        context.shadows().forget_all();
    else
        context.shadows().forget(buffer);
}

void ManualCalls::respecify_bound(GuestGlContext &context, uint32_t target, intptr_t size, bool with_data)
{
    AV_SCOPE("ManualCalls::respecify_bound");
    const uint32_t buffer = context.arrays().bound_buffer(target);
    if (buffer == ClientArrayState::unknown)
        context.shadows().forget_all();
    else
        context.shadows().respecify(buffer, size, with_data);
}

bool ManualCalls::indices_cross(const void *indices)
{
    AV_SCOPE("ManualCalls::indices_cross");
    GuestGlContext *const context = currents_.current();
    return indices == nullptr || context->arrays().indices_are_offset();
}

}  // namespace av::glicd

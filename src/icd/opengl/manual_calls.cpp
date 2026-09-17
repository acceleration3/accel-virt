#include "icd/opengl/manual_calls.hpp"

#include "core/trace/function_scope.hpp"
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

}  // namespace

ManualCalls::ManualCalls(CurrentContexts &currents) : currents_(currents)
{
    AV_SCOPE("ManualCalls::ManualCalls");
}

void *ManualCalls::refuse_map()
{
    AV_SCOPE("ManualCalls::refuse_map");
    record(gl_invalid_operation);
    return nullptr;
}

bool ManualCalls::refuse_unmap()
{
    AV_SCOPE("ManualCalls::refuse_unmap");
    record(gl_invalid_operation);
    return false;
}

void ManualCalls::refuse()
{
    AV_SCOPE("ManualCalls::refuse");
    record(gl_invalid_operation);
}

void ManualCalls::buffer_pointer(uint32_t pname, void **params)
{
    AV_SCOPE("ManualCalls::buffer_pointer");
    if (currents_.current() == nullptr || params == nullptr) return;
    if (pname != gl_buffer_map_pointer) {
        record(gl_invalid_enum);
        return;
    }
    *params = nullptr;
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
    glproto::rvk_glPixelStorei(*context->wire(), pname, value);
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
    glproto::rvk_glPixelStoref(*context->wire(), pname, value);
}

void ManualCalls::bind_buffer(uint32_t target, uint32_t buffer)
{
    AV_SCOPE("ManualCalls::bind_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().bind_buffer(target, buffer);
    glproto::rvk_glBindBuffer(*context->wire(), target, buffer);
}

void ManualCalls::delete_buffers(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("ManualCalls::delete_buffers");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().delete_buffers(count, buffers);
    glproto::rvk_glDeleteBuffers(*context->wire(), count, buffers);
}

void ManualCalls::gen_buffers(int32_t count, uint32_t *buffers)
{
    AV_SCOPE("ManualCalls::gen_buffers");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glGenBuffers(*context->wire(), count, buffers);
}

void ManualCalls::gen_textures(int32_t count, uint32_t *textures)
{
    AV_SCOPE("ManualCalls::gen_textures");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glGenTextures(*context->wire(), count, textures);
}

void ManualCalls::gen_vertex_arrays(int32_t count, uint32_t *arrays)
{
    AV_SCOPE("ManualCalls::gen_vertex_arrays");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glGenVertexArrays(*context->wire(), count, arrays);
}

void ManualCalls::bind_vertex_array(uint32_t array)
{
    AV_SCOPE("ManualCalls::bind_vertex_array");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().bind_vertex_array(array);
    glproto::rvk_glBindVertexArray(*context->wire(), array);
}

void ManualCalls::delete_vertex_arrays(int32_t count, const uint32_t *arrays)
{
    AV_SCOPE("ManualCalls::delete_vertex_arrays");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().delete_vertex_arrays(count, arrays);
    glproto::rvk_glDeleteVertexArrays(*context->wire(), count, arrays);
}

void ManualCalls::buffer_data(uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    AV_SCOPE("ManualCalls::buffer_data");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glBufferData(*context->wire(), target, size, data, usage);
}

void ManualCalls::buffer_storage(uint32_t target, intptr_t size, const void *data, uint32_t flags)
{
    AV_SCOPE("ManualCalls::buffer_storage");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glBufferStorage(*context->wire(), target, size, data, flags);
}

void ManualCalls::named_buffer_data(uint32_t buffer, intptr_t size, const void *data, uint32_t usage)
{
    AV_SCOPE("ManualCalls::named_buffer_data");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glNamedBufferData(*context->wire(), buffer, size, data, usage);
}

void ManualCalls::named_buffer_storage(uint32_t buffer, intptr_t size, const void *data, uint32_t flags)
{
    AV_SCOPE("ManualCalls::named_buffer_storage");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glNamedBufferStorage(*context->wire(), buffer, size, data, flags);
}

void ManualCalls::vertex_attrib_pointer(uint32_t index, int32_t size, uint32_t type, uint8_t normalized,
                                        int32_t stride, const void *pointer)
{
    AV_SCOPE("ManualCalls::vertex_attrib_pointer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->arrays().client_pointer(address_of(pointer))) return context->record(gl_invalid_operation);
    context->arrays().set_attribute_pointer(index, address_of(pointer));
    glproto::rvk_glVertexAttribPointer(*context->wire(), index, size, type, normalized, stride, address_of(pointer));
}

void ManualCalls::enable_vertex_attrib_array(uint32_t index, bool enable)
{
    AV_SCOPE("ManualCalls::enable_vertex_attrib_array");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (enable) return glproto::rvk_glEnableVertexAttribArray(*context->wire(), index);
    glproto::rvk_glDisableVertexAttribArray(*context->wire(), index);
}

void ManualCalls::draw_arrays(uint32_t mode, int32_t first, int32_t count, int32_t instances)
{
    AV_SCOPE("ManualCalls::draw_arrays");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (instances < 0) return glproto::rvk_glDrawArrays(*context->wire(), mode, first, count);
    glproto::rvk_glDrawArraysInstanced(*context->wire(), mode, first, count, instances);
}

void ManualCalls::draw_elements(uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t instances)
{
    AV_SCOPE("ManualCalls::draw_elements");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!indices_cross(indices)) return context->record(gl_invalid_operation);
    if (instances < 0) return glproto::rvk_glDrawElements(*context->wire(), mode, count, type, address_of(indices));
    glproto::rvk_glDrawElementsInstanced(*context->wire(), mode, count, type, address_of(indices), instances);
}

void ManualCalls::draw_range_elements(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                                      const void *indices)
{
    AV_SCOPE("ManualCalls::draw_range_elements");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!indices_cross(indices)) return context->record(gl_invalid_operation);
    glproto::rvk_glDrawRangeElements(*context->wire(), mode, start, end, count, type, address_of(indices));
}

void ManualCalls::flush()
{
    AV_SCOPE("ManualCalls::flush");
    GuestGlContext *const context = sending();
    if (context != nullptr) glproto::rvk_glFlush(*context->wire());
}

/* GL_WAIT_FAILED (0x911D) with nothing to wait on. */
uint32_t ManualCalls::client_wait_sync(void *sync, uint32_t flags, uint64_t timeout)
{
    AV_SCOPE("ManualCalls::client_wait_sync");
    static constexpr uint32_t wait_failed = 0x911Du;
    GuestGlContext *const context = sending();
    if (context == nullptr) return wait_failed;
    return glproto::rvk_glClientWaitSync(*context->wire(), address_of(sync), flags, timeout);
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
    glproto::rvk_glLinkProgram(*context->wire(), program);
}

void ManualCalls::delete_program(uint32_t program)
{
    AV_SCOPE("ManualCalls::delete_program");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->uniforms().forget(program);
    glproto::rvk_glDeleteProgram(*context->wire(), program);
}

int32_t ManualCalls::attrib_location(uint32_t program, const char *name)
{
    AV_SCOPE("ManualCalls::attrib_location");
    GuestGlContext *const context = sending();
    return context != nullptr ? glproto::rvk_glGetAttribLocation(*context->wire(), program, name) : -1;
}

/* The location's type is asked at once, while nothing else is being sent, so
 * a later glGetUniform can size its answer. */
int32_t ManualCalls::uniform_location(uint32_t program, const char *name)
{
    AV_SCOPE("ManualCalls::uniform_location");
    GuestGlContext *const context = sending();
    if (context == nullptr) return -1;
    ClientTransport &wire = *context->wire();
    const int32_t    location = glproto::rvk_glGetUniformLocation(wire, program, name);
    if (location < 0) return location;
    const uint32_t index = glproto::rvk_glGetProgramResourceIndex(wire, program, gl_uniform, name);
    if (index == gl_invalid_index) return location;
    const uint32_t property = gl_type;
    int32_t        written = 0;
    int32_t        type = 0;
    glproto::rvk_glGetProgramResourceiv(wire, program, gl_uniform, index, 1, &property, 1, &written, &type);
    if (written == 1) context->uniforms().remember(program, location, static_cast<uint32_t>(type));
    return location;
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
    glproto::rvk_glGetIntegerv(*context->wire(), pname, data);
    if (data != nullptr) *data = context->framebuffers().answer(pname, *data);
}

void ManualCalls::bind_framebuffer(uint32_t target, uint32_t framebuffer)
{
    AV_SCOPE("ManualCalls::bind_framebuffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    WindowFramebuffers &framebuffers = context->framebuffers();
    framebuffers.bound(target, framebuffer);
    glproto::rvk_glBindFramebuffer(*context->wire(), target, framebuffers.name_of(framebuffer));
}

void ManualCalls::draw_buffer(uint32_t buffer)
{
    AV_SCOPE("ManualCalls::draw_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    glproto::rvk_glDrawBuffer(*context->wire(), context->framebuffers().buffer_for(buffer, true));
}

void ManualCalls::read_buffer(uint32_t buffer)
{
    AV_SCOPE("ManualCalls::read_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    glproto::rvk_glReadBuffer(*context->wire(), context->framebuffers().buffer_for(buffer, false));
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

bool ManualCalls::indices_cross(const void *indices)
{
    AV_SCOPE("ManualCalls::indices_cross");
    GuestGlContext *const context = currents_.current();
    return indices == nullptr || context->arrays().indices_are_offset();
}

}  // namespace av::glicd

#include "icd/opengl/manual_calls.hpp"

#include "guest/transport/host_allocation.hpp"

#include "icd/common/host_completion_wait.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "icd/opengl/sync_wait_probe.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include "proto/client/gl_icd_support.hpp"

#include "icd/opengl/granted_profile.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/bindless_handles.hpp"
#include "icd/opengl/buffer_pieces.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/client_vertex_draw.hpp"
#include "icd/opengl/client_vertex_state.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/opengl/gl_context_table.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/sliced_buffers.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

uint64_t address_of(const void *pointer)
{
    AV_SCOPE("address_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer));
}

bool shader_writable(uint32_t target)
{
    AV_SCOPE("shader_writable");
    return target == 0x8C8Eu || target == 0x90D2u || target == 0x92C0u;
}

uint32_t access_bits(uint32_t access)
{
    AV_SCOPE("access_bits");
    if (access == ManualCalls::gl_read_only) return BufferMapping::map_read;
    if (access == ManualCalls::gl_write_only) return BufferMapping::map_write;
    if (access == ManualCalls::gl_read_write) return BufferMapping::map_read | BufferMapping::map_write;
    return 0u;
}

}

ManualCalls::ManualCalls(CurrentContexts &currents, GlContextTable &contexts, const Kmt &kmt)
    : currents_(currents), contexts_(contexts), kmt_(kmt)
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

void ManualCalls::buffer_sub_data(uint32_t target, intptr_t offset, intptr_t size, const void *data)
{
    AV_SCOPE("ManualCalls::buffer_sub_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (sliced_buffer(*context, target) != 0u) return context->record(gl_invalid_operation);
    uint32_t bound = 0u;
    if (bound_to(*context, target, bound) && context->mappings().refuses_subdata(bound))
        return context->record(gl_invalid_operation);
    wrote_bound(*context, target, offset, size, data);
    BufferPieces pieces(*context->wire());
    if (pieces.cuts_write(size, data))
        return pieces.write(target, offset, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
    glproto::av_glBufferSubData(*context->wire(), target, offset, size, data);
}

void ManualCalls::named_buffer_sub_data(uint32_t buffer, intptr_t offset, intptr_t size, const void *data)
{
    AV_SCOPE("ManualCalls::named_buffer_sub_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->mappings().refuses_subdata(buffer)) return context->record(gl_invalid_operation);
    if (context->slices().holds(buffer)) return context->record(gl_invalid_operation);
    if (offset >= 0 && size >= 0)
        context->shadows().wrote(buffer, static_cast<uint64_t>(offset), static_cast<uint64_t>(size), data);
    BufferPieces pieces(*context->wire());
    if (pieces.cuts_write(size, data))
        return pieces.write_named(buffer, offset, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
    glproto::av_glNamedBufferSubData(*context->wire(), buffer, offset, size, data);
}

void ManualCalls::named_buffer_sub_data_ext(uint32_t buffer, intptr_t offset, intptr_t size, const void *data)
{
    AV_SCOPE("ManualCalls::named_buffer_sub_data_ext");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->mappings().refuses_subdata(buffer)) return context->record(gl_invalid_operation);
    if (context->slices().holds(buffer)) return context->record(gl_invalid_operation);
    if (offset >= 0 && size >= 0)
        context->shadows().wrote(buffer, static_cast<uint64_t>(offset), static_cast<uint64_t>(size), data);
    BufferPieces pieces(*context->wire());
    if (pieces.cuts_write(size, data))
        return pieces.write_named_ext(buffer, offset, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
    glproto::av_glNamedBufferSubDataEXT(*context->wire(), buffer, offset, size, data);
}

void ManualCalls::get_buffer_sub_data(uint32_t target, intptr_t offset, intptr_t size, void *data)
{
    AV_SCOPE("ManualCalls::get_buffer_sub_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const uint32_t sliced = sliced_buffer(*context, target);
    if (sliced != 0u) return context->read_sliced(sliced, offset, size, data);
    BufferPieces pieces(*context->wire());
    if (pieces.cuts_read(size, data))
        return pieces.read(target, offset, static_cast<size_t>(size), static_cast<uint8_t *>(data));
    glproto::av_glGetBufferSubData(*context->wire(), target, offset, size, data);
}

void ManualCalls::get_named_buffer_sub_data(uint32_t buffer, intptr_t offset, intptr_t size, void *data)
{
    AV_SCOPE("ManualCalls::get_named_buffer_sub_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->slices().holds(buffer)) return context->read_sliced(buffer, offset, size, data);
    BufferPieces pieces(*context->wire());
    if (pieces.cuts_read(size, data))
        return pieces.read_named(buffer, offset, static_cast<size_t>(size), static_cast<uint8_t *>(data));
    glproto::av_glGetNamedBufferSubData(*context->wire(), buffer, offset, size, data);
}

void ManualCalls::get_named_buffer_sub_data_ext(uint32_t buffer, intptr_t offset, intptr_t size, void *data)
{
    AV_SCOPE("ManualCalls::get_named_buffer_sub_data_ext");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->slices().holds(buffer)) return context->read_sliced(buffer, offset, size, data);
    BufferPieces pieces(*context->wire());
    if (pieces.cuts_read(size, data))
        return pieces.read_named_ext(buffer, offset, static_cast<size_t>(size), static_cast<uint8_t *>(data));
    glproto::av_glGetNamedBufferSubDataEXT(*context->wire(), buffer, offset, size, data);
}

bool ManualCalls::stage_upload(const void *pixels, size_t bytes)
{
    AV_SCOPE("ManualCalls::stage_upload");
    GuestGlContext *const context = sending();
    return context != nullptr && context->staging().stage_upload(*context->wire(), pixels, bytes);
}

void ManualCalls::finish_upload()
{
    AV_SCOPE("ManualCalls::finish_upload");
    GuestGlContext *const context = sending();
    if (context != nullptr) context->staging().finish_upload(*context->wire());
}

bool ManualCalls::stage_download(const void *pixels, size_t bytes)
{
    AV_SCOPE("ManualCalls::stage_download");
    GuestGlContext *const context = sending();
    return context != nullptr && context->staging().stage_download(*context->wire(), pixels, bytes);
}

void ManualCalls::finish_download(void *pixels, size_t bytes)
{
    AV_SCOPE("ManualCalls::finish_download");
    GuestGlContext *const context = sending();
    if (context != nullptr) context->staging().finish_download(*context->wire(), pixels, bytes);
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
    context->arrays().forget_buffer(target);
    if (shader_writable(target)) context->shadows().distrust();
}

void ManualCalls::shaders_write_buffers()
{
    AV_SCOPE("ManualCalls::shaders_write_buffers");
    GuestGlContext *const context = sending();
    if (context != nullptr) context->shadows().distrust();
}

bool ManualCalls::unpack_slice_bound()
{
    AV_SCOPE("ManualCalls::unpack_slice_bound");
    GuestGlContext *const context = sending();
    if (context == nullptr || context->slices().held() == 0u) return false;
    return context->slices().holds(context->arrays().bound_buffer(ClientArrayState::pixel_unpack_buffer));
}

bool ManualCalls::slice_unpack_pixels(const void *&pixels, size_t bytes)
{
    AV_SCOPE("ManualCalls::slice_unpack_pixels");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    const uint8_t *data = nullptr;
    const uint32_t error = context->slices().unpack_source(
        context->arrays().bound_buffer(ClientArrayState::pixel_unpack_buffer), address_of(pixels), bytes, data);
    if (error != SlicedBuffers::gl_no_error) {
        context->record(error);
        return false;
    }
    glproto::av_glBindBuffer(*context->wire(), ClientArrayState::pixel_unpack_buffer, 0u);
    pixels = data;
    return true;
}

void ManualCalls::unpack_slice_done()
{
    AV_SCOPE("ManualCalls::unpack_slice_done");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    glproto::av_glBindBuffer(*context->wire(), ClientArrayState::pixel_unpack_buffer,
                             context->arrays().bound_buffer(ClientArrayState::pixel_unpack_buffer));
}

bool ManualCalls::pack_slice_bound()
{
    AV_SCOPE("ManualCalls::pack_slice_bound");
    GuestGlContext *const context = sending();
    if (context == nullptr || context->slices().held() == 0u) return false;
    return context->slices().holds(context->arrays().bound_buffer(ClientArrayState::pixel_pack_buffer));
}

bool ManualCalls::slice_pack_pixels(void *&pixels, size_t bytes)
{
    AV_SCOPE("ManualCalls::slice_pack_pixels");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    uint8_t       *data = nullptr;
    const uint32_t error = context->slices().pack_target(
        context->arrays().bound_buffer(ClientArrayState::pixel_pack_buffer), address_of(pixels), bytes, data);
    if (error != SlicedBuffers::gl_no_error) {
        context->record(error);
        return false;
    }
    glproto::av_glBindBuffer(*context->wire(), ClientArrayState::pixel_pack_buffer, 0u);
    pixels = data;
    return true;
}

void ManualCalls::pack_slice_done()
{
    AV_SCOPE("ManualCalls::pack_slice_done");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    glproto::av_glBindBuffer(*context->wire(), ClientArrayState::pixel_pack_buffer,
                             context->arrays().bound_buffer(ClientArrayState::pixel_pack_buffer));
}

bool ManualCalls::refuse_sliced_unpack()
{
    AV_SCOPE("ManualCalls::refuse_sliced_unpack");
    if (!unpack_slice_bound()) return false;
    record(gl_invalid_operation);
    return true;
}

bool ManualCalls::refuse_sliced_pack()
{
    AV_SCOPE("ManualCalls::refuse_sliced_pack");
    if (!pack_slice_bound()) return false;
    record(gl_invalid_operation);
    return true;
}

bool ManualCalls::clear_bound_buffer(uint32_t target, uint32_t internal_format, uint32_t format, uint32_t type,
                                     const void *data)
{
    AV_SCOPE("ManualCalls::clear_bound_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    const uint32_t buffer = sliced_buffer(*context, target);
    if (buffer == 0u) return false;
    return clear_named_buffer(buffer, internal_format, format, type, data);
}

bool ManualCalls::clear_bound_buffer_range(uint32_t target, uint32_t internal_format, intptr_t offset, intptr_t size,
                                           uint32_t format, uint32_t type, const void *data)
{
    AV_SCOPE("ManualCalls::clear_bound_buffer_range");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    const uint32_t buffer = sliced_buffer(*context, target);
    if (buffer == 0u) return false;
    return clear_named_buffer_range(buffer, internal_format, offset, size, format, type, data);
}

bool ManualCalls::clear_named_buffer(uint32_t buffer, uint32_t internal_format, uint32_t format, uint32_t type,
                                     const void *data)
{
    AV_SCOPE("ManualCalls::clear_named_buffer");
    GuestGlContext *const context = sending();
    if (context == nullptr || !context->slices().holds(buffer)) return false;
    const uint32_t error = context->slices().clear_all(buffer, internal_format, format, type, data);
    if (error != SlicedBuffers::gl_no_error) context->record(error);
    return true;
}

bool ManualCalls::clear_named_buffer_range(uint32_t buffer, uint32_t internal_format, intptr_t offset, intptr_t size,
                                           uint32_t format, uint32_t type, const void *data)
{
    AV_SCOPE("ManualCalls::clear_named_buffer_range");
    GuestGlContext *const context = sending();
    if (context == nullptr || !context->slices().holds(buffer)) return false;
    const uint32_t error = context->slices().clear_range(buffer, internal_format, offset, size, format, type, data);
    if (error != SlicedBuffers::gl_no_error) context->record(error);
    return true;
}

bool ManualCalls::invalidate_named_buffer(uint32_t buffer)
{
    AV_SCOPE("ManualCalls::invalidate_named_buffer");
    GuestGlContext *const context = sending();
    return context != nullptr && context->slices().holds(buffer);
}

bool ManualCalls::invalidate_named_buffer_range(uint32_t buffer, intptr_t offset, intptr_t length)
{
    AV_SCOPE("ManualCalls::invalidate_named_buffer_range");
    GuestGlContext *const context = sending();
    if (context == nullptr || !context->slices().holds(buffer)) return false;
    const uint32_t error = context->slices().invalidate(buffer, offset, length);
    if (error != SlicedBuffers::gl_no_error) context->record(error);
    return true;
}

void ManualCalls::client_attributes_pushed(uint32_t mask)
{
    AV_SCOPE("ManualCalls::client_attributes_pushed");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().push_client(mask);
    context->vertex().push(mask);
}

void ManualCalls::client_attributes_popped()
{
    AV_SCOPE("ManualCalls::client_attributes_popped");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().pop_client();
    context->vertex().pop();
    context->shadows().forget_all();
}

void ManualCalls::client_attributes_reset(uint32_t mask)
{
    AV_SCOPE("ManualCalls::client_attributes_reset");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().reset_client(mask);
    context->vertex().reset(mask);
}

void ManualCalls::vertex_array_elements(uint32_t array, uint32_t buffer)
{
    AV_SCOPE("ManualCalls::vertex_array_elements");
    GuestGlContext *const context = sending();
    if (context != nullptr) context->arrays().set_vertex_array_elements(array, buffer);
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
        *params = reinterpret_cast<void *>(static_cast<uintptr_t>(context->debug().function()));
        return;
    }
    if (pname != gl_debug_callback_user_param) return record(gl_invalid_enum);
    *params = reinterpret_cast<void *>(static_cast<uintptr_t>(context->debug().user()));
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
    if (target == PinnedBuffers::target) return context->pinned().bind(buffer);
    context->arrays().bind_buffer(target, buffer);
    glproto::av_glBindBuffer(*context->wire(), target, buffer);
}

void ManualCalls::delete_buffers(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("ManualCalls::delete_buffers");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().delete_buffers(count, buffers);
    context->pinned().forget(count, buffers);
    context->mappings().forget(count, buffers);
    context->shadows().forget(count, buffers);
    glproto::av_glDeleteBuffers(*context->wire(), count, buffers);
    context->slices().forget(count, buffers);
    contexts_.reap();
}

void ManualCalls::gen_buffers(int32_t count, uint32_t *buffers)
{
    AV_SCOPE("ManualCalls::gen_buffers");
    GuestGlContext *const context = sending();
    if (context != nullptr && count < 0) return context->record(gl_invalid_value);
    if (context != nullptr) glproto::av_glGenBuffers(*context->wire(), count, buffers);
}

void ManualCalls::gen_textures(int32_t count, uint32_t *textures)
{
    AV_SCOPE("ManualCalls::gen_textures");
    GuestGlContext *const context = sending();
    if (context != nullptr && count < 0) return context->record(gl_invalid_value);
    if (context != nullptr) glproto::av_glGenTextures(*context->wire(), count, textures);
}

void ManualCalls::gen_vertex_arrays(int32_t count, uint32_t *arrays)
{
    AV_SCOPE("ManualCalls::gen_vertex_arrays");
    GuestGlContext *const context = sending();
    if (context != nullptr && count < 0) return context->record(gl_invalid_value);
    if (context != nullptr) glproto::av_glGenVertexArrays(*context->wire(), count, arrays);
}

void ManualCalls::bind_vertex_array(uint32_t array)
{
    AV_SCOPE("ManualCalls::bind_vertex_array");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (context->arrays().bind_vertex_array(array) != Status::ok || context->vertex().bind(array) != Status::ok)
        context->record(GuestGlContext::gl_out_of_memory);
    glproto::av_glBindVertexArray(*context->wire(), array);
}

void ManualCalls::delete_vertex_arrays(int32_t count, const uint32_t *arrays)
{
    AV_SCOPE("ManualCalls::delete_vertex_arrays");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->arrays().delete_vertex_arrays(count, arrays);
    context->vertex().forget(count, arrays);
    glproto::av_glDeleteVertexArrays(*context->wire(), count, arrays);
}

void ManualCalls::delete_transform_feedbacks(int32_t count, const uint32_t *ids)
{
    AV_SCOPE("ManualCalls::delete_transform_feedbacks");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (count > 0 && ids != nullptr) context->arrays().forget_buffer(ClientArrayState::transform_feedback_buffer);
    glproto::av_glDeleteTransformFeedbacks(*context->wire(), count, ids);
}

void ManualCalls::pin_storage(GuestGlContext &context, intptr_t size, const void *data)
{
    AV_SCOPE("ManualCalls::pin_storage");
    static constexpr uint32_t copy_write_buffer = 0x8F37u;
    if (size <= 0 || data == nullptr) return context.record(gl_invalid_value);
    HostAllocation *lent = nullptr;
    uint64_t        offset = 0u;
    const uint32_t  buffer = context.pinned().bound();
    if (context.pinned().pin(data, static_cast<uint64_t>(size), lent, offset) != Status::ok)
        return context.record(gl_invalid_operation);
    uint32_t previous = 0u;
    static_cast<void>(bound_to(context, copy_write_buffer, previous));
    ClientTransport &wire = *context.wire();
    glproto::av_glBindBuffer(wire, copy_write_buffer, buffer);
    glproto::av_glBufferStorageMemEXT(wire, copy_write_buffer, size, static_cast<uint32_t>(lent->handle()), offset);
    glproto::av_glBindBuffer(wire, copy_write_buffer, previous);
}

void ManualCalls::buffer_data(uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    AV_SCOPE("ManualCalls::buffer_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (size < 0) return context->record(gl_invalid_value);
    if (target == PinnedBuffers::target) return pin_storage(*context, size, data);
    respecify_bound(*context, target, size, data != nullptr);
    BufferPieces pieces(*context->wire());
    const bool   cut = pieces.cuts_write(size, data);
    glproto::av_glBufferData(*context->wire(), target, size, cut ? nullptr : data, usage);
    if (cut) pieces.write(target, 0, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
}

void ManualCalls::buffer_storage(uint32_t target, intptr_t size, const void *data, uint32_t flags)
{
    AV_SCOPE("ManualCalls::buffer_storage");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (size < 0) return context->record(gl_invalid_value);
    uint32_t buffer = 0u;
    if (context->slices().offers(size, flags) && bound_to(*context, target, buffer) &&
        context->slices().place(*context->wire(), buffer, static_cast<uint64_t>(size), flags, data) == Status::ok)
        return;
    respecify_bound(*context, target, size, data != nullptr);
    BufferPieces pieces(*context->wire());
    const bool   cut = pieces.cuts_write(size, data);
    glproto::av_glBufferStorage(*context->wire(), target, size, cut ? nullptr : data, flags | gl_dynamic_storage_bit);
    if (bound_to(*context, target, buffer)) context->mappings().keep_flags(buffer, flags);
    if (cut) pieces.write(target, 0, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
}

void ManualCalls::named_buffer_data(uint32_t buffer, intptr_t size, const void *data, uint32_t usage)
{
    AV_SCOPE("ManualCalls::named_buffer_data");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (size < 0) return context->record(gl_invalid_value);
    context->shadows().respecify(buffer, size, data != nullptr);
    BufferPieces pieces(*context->wire());
    const bool   cut = pieces.cuts_write(size, data);
    glproto::av_glNamedBufferData(*context->wire(), buffer, size, cut ? nullptr : data, usage);
    if (cut) pieces.write_named(buffer, 0, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
}

void ManualCalls::named_buffer_storage(uint32_t buffer, intptr_t size, const void *data, uint32_t flags)
{
    AV_SCOPE("ManualCalls::named_buffer_storage");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (size < 0) return context->record(gl_invalid_value);
    if (context->slices().place(*context->wire(), buffer, static_cast<uint64_t>(size), flags, data) == Status::ok)
        return;
    context->shadows().respecify(buffer, size, data != nullptr);
    BufferPieces pieces(*context->wire());
    const bool   cut = pieces.cuts_write(size, data);
    glproto::av_glNamedBufferStorage(*context->wire(), buffer, size, cut ? nullptr : data,
                                     flags | gl_dynamic_storage_bit);
    context->mappings().keep_flags(buffer, flags);
    if (cut) pieces.write_named(buffer, 0, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
}

void ManualCalls::named_buffer_data_ext(uint32_t buffer, intptr_t size, const void *data, uint32_t usage)
{
    AV_SCOPE("ManualCalls::named_buffer_data_ext");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (size < 0) return context->record(gl_invalid_value);
    context->shadows().respecify(buffer, size, data != nullptr);
    BufferPieces pieces(*context->wire());
    const bool   cut = pieces.cuts_write(size, data);
    glproto::av_glNamedBufferDataEXT(*context->wire(), buffer, size, cut ? nullptr : data, usage);
    if (cut) pieces.write_named_ext(buffer, 0, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
}

void ManualCalls::named_buffer_storage_ext(uint32_t buffer, intptr_t size, const void *data, uint32_t flags)
{
    AV_SCOPE("ManualCalls::named_buffer_storage_ext");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (size < 0) return context->record(gl_invalid_value);
    if (context->slices().place(*context->wire(), buffer, static_cast<uint64_t>(size), flags, data) == Status::ok)
        return;
    context->shadows().respecify(buffer, size, data != nullptr);
    BufferPieces pieces(*context->wire());
    const bool   cut = pieces.cuts_write(size, data);
    glproto::av_glNamedBufferStorageEXT(*context->wire(), buffer, size, cut ? nullptr : data,
                                        flags | gl_dynamic_storage_bit);
    context->mappings().keep_flags(buffer, flags);
    if (cut) pieces.write_named_ext(buffer, 0, static_cast<size_t>(size), static_cast<const uint8_t *>(data));
}

void ManualCalls::vertex_attrib_pointer(uint32_t index, int32_t size, uint32_t type, uint8_t normalized,
                                        int32_t stride, const void *pointer)
{
    AV_SCOPE("ManualCalls::vertex_attrib_pointer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const bool client = context->arrays().client_pointer(address_of(pointer));
    if (index >= ClientVertexState::generic_slots) {
        if (client) return context->record(gl_invalid_operation);
        context->arrays().set_attribute_pointer(index, address_of(pointer));
        return glproto::av_glVertexAttribPointer(*context->wire(), index, size, type, normalized, stride,
                                                 address_of(pointer));
    }
    if (client && !context->profile().legacy_calls_live()) return context->record(gl_invalid_operation);
    const uint32_t error = context->vertex().validate(index, size, type, stride);
    if (error != 0u) return context->record(error);
    context->arrays().set_attribute_pointer(index, address_of(pointer));
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    draw.pointer(index, size, type, normalized != 0u, stride, pointer);
}

void ManualCalls::enable_vertex_attrib_array(uint32_t index, bool enable)
{
    AV_SCOPE("ManualCalls::enable_vertex_attrib_array");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (index < ClientVertexState::generic_slots) {
        ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
        return draw.enable(index, enable);
    }
    if (enable) return glproto::av_glEnableVertexAttribArray(*context->wire(), index);
    glproto::av_glDisableVertexAttribArray(*context->wire(), index);
}

void ManualCalls::program_string(uint32_t target, uint32_t pname, void *string)
{
    AV_SCOPE("ManualCalls::program_string");
    static constexpr uint32_t gl_program_length = 0x8627u;
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    int32_t length = 0;
    glproto::av_glGetProgramivARB(*context->wire(), target, gl_program_length, &length);
    context->vertex().set_program_length(length);
    glproto::av_glGetProgramStringARB(*context->wire(), target, pname, string);
}

void ManualCalls::array_pointer(uint32_t slot, int32_t size, uint32_t type, int32_t stride, const void *pointer)
{
    AV_SCOPE("ManualCalls::array_pointer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!context->profile().legacy_calls_live()) return context->record(gl_invalid_operation);
    if (slot == ClientVertexState::texcoord_slot)
        slot = context->vertex().texcoord_slot_of_unit(context->vertex().texcoord_unit());
    const uint32_t error = context->vertex().validate(slot, size, type, stride);
    if (error != 0u) return context->record(error);
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    draw.pointer(slot, size, type, false, stride, pointer);
}

void ManualCalls::client_state(uint32_t capability, bool enable)
{
    AV_SCOPE("ManualCalls::client_state");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!context->profile().legacy_calls_live()) return context->record(gl_invalid_operation);
    const uint32_t slot = context->vertex().slot_of_capability(capability);
    if (slot == ClientVertexState::no_slot) return context->record(gl_invalid_enum);
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    draw.enable(slot, enable);
}

void ManualCalls::client_active_texture(uint32_t texture)
{
    AV_SCOPE("ManualCalls::client_active_texture");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!context->profile().legacy_calls_live()) return context->record(gl_invalid_operation);
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    const uint32_t       error = draw.active_texture(texture);
    if (error != 0u) context->record(error);
}

void ManualCalls::interleaved_arrays(uint32_t format, int32_t stride, const void *pointer)
{
    AV_SCOPE("ManualCalls::interleaved_arrays");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!context->profile().legacy_calls_live()) return context->record(gl_invalid_operation);
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    const uint32_t       error = draw.interleaved(format, stride, pointer);
    if (error != 0u) context->record(error);
}

void ManualCalls::array_element(int32_t index)
{
    AV_SCOPE("ManualCalls::array_element");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (!context->profile().legacy_calls_live()) return context->record(gl_invalid_operation);
    if (index < 0) return context->record(gl_invalid_value);
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    draw.array_element(index);
}

void ManualCalls::begin_primitives()
{
    AV_SCOPE("ManualCalls::begin_primitives");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || !context->vertex().client_active() || context->wire() == nullptr) return;
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    draw.begin_immediate();
}

bool ManualCalls::stage_arrays(int32_t first, int32_t count)
{
    AV_SCOPE("ManualCalls::stage_arrays");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || context->wire() == nullptr || !context->vertex().client_active()) return true;
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    const uint32_t       error = draw.arrays(first, count);
    if (error == 0u) return true;
    draw.finish();
    context->record(error);
    return false;
}

bool ManualCalls::stage_elements(uint32_t type, int32_t count, const void *&indices, int32_t base_vertex)
{
    AV_SCOPE("ManualCalls::stage_elements");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || context->wire() == nullptr) return true;
    if (!context->profile().legacy_calls_live()) {
        if (indices != nullptr && context->arrays().bound_buffer(ClientArrayState::element_array_buffer) == 0u) {
            context->record(gl_invalid_operation);
            return false;
        }
        return true;
    }
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    uint64_t             host = 0u;
    const uint32_t       error = draw.elements(type, count, indices, base_vertex, host);
    if (error != 0u) {
        draw.finish();
        context->record(error);
        return false;
    }
    indices = reinterpret_cast<const void *>(static_cast<uintptr_t>(host));
    return true;
}

bool ManualCalls::stage_multi_arrays(const int32_t *first, const int32_t *count, int32_t draws)
{
    AV_SCOPE("ManualCalls::stage_multi_arrays");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || context->wire() == nullptr || !context->vertex().client_active()) return true;
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    const uint32_t       error = draw.multi_arrays(first, count, draws);
    if (error == 0u) return true;
    draw.finish();
    context->record(error);
    return false;
}

bool ManualCalls::stage_multi_elements(const int32_t *count, uint32_t type, const void *const *indices, int32_t draws,
                                       const int32_t *base_vertex, const uint64_t *&host_indices)
{
    AV_SCOPE("ManualCalls::stage_multi_elements");
    host_indices = nullptr;
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || context->wire() == nullptr) return true;
    if (!context->profile().legacy_calls_live()) {
        if (indices != nullptr && draws > 0 &&
            context->arrays().bound_buffer(ClientArrayState::element_array_buffer) == 0u) {
            context->record(gl_invalid_operation);
            return false;
        }
        return true;
    }
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    const uint32_t       error = draw.multi_elements(count, type, indices, draws, base_vertex, host_indices);
    if (error == 0u) return true;
    draw.finish();
    context->record(error);
    return false;
}

bool ManualCalls::refuse_indirect_draw()
{
    AV_SCOPE("ManualCalls::refuse_indirect_draw");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || !context->vertex().client_active()) return false;
    context->record(gl_invalid_operation);
    return true;
}

void ManualCalls::finish_client_draw()
{
    AV_SCOPE("ManualCalls::finish_client_draw");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || !context->vertex().indexed() || context->wire() == nullptr) return;
    ClientVertexDraw draw(*context->wire(), context->vertex(), context->arrays(), context->shadows());
    draw.finish();
}

void ManualCalls::draw_arrays(uint32_t mode, int32_t first, int32_t count, int32_t instances)
{
    AV_SCOPE("ManualCalls::draw_arrays");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->mappings().flush_persistent(*context->wire());
    if (!stage_arrays(first, count)) return;
    if (instances < 0) return glproto::av_glDrawArrays(*context->wire(), mode, first, count);
    glproto::av_glDrawArraysInstanced(*context->wire(), mode, first, count, instances);
}

void ManualCalls::draw_elements(uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t instances)
{
    AV_SCOPE("ManualCalls::draw_elements");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->mappings().flush_persistent(*context->wire());
    if (!stage_elements(type, count, indices, 0)) return;
    if (instances < 0)
        glproto::av_glDrawElements(*context->wire(), mode, count, type, address_of(indices));
    else
        glproto::av_glDrawElementsInstanced(*context->wire(), mode, count, type, address_of(indices), instances);
    finish_client_draw();
}

void ManualCalls::draw_range_elements(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                                      const void *indices)
{
    AV_SCOPE("ManualCalls::draw_range_elements");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    context->mappings().flush_persistent(*context->wire());
    if (!stage_elements(type, count, indices, 0)) return;
    glproto::av_glDrawRangeElements(*context->wire(), mode, start, end, count, type, address_of(indices));
    finish_client_draw();
}

void ManualCalls::flush()
{
    AV_SCOPE("ManualCalls::flush");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    glproto::av_glFlush(*context->wire());
    context->end_frame();
}

uint32_t ManualCalls::client_wait_sync(void *sync, uint32_t flags, uint64_t timeout)
{
    AV_SCOPE("ManualCalls::client_wait_sync");
    static constexpr uint32_t wait_failed = 0x911Du;
    GuestGlContext *const context = sending();
    if (context == nullptr) return wait_failed;
    SyncWaitProbe           probe(*context->wire(), address_of(sync), flags);
    icd::HostCompletionWait wait(kmt_, timeout);
    static_cast<void>(wait.run(probe));
    const uint32_t result = probe.result();
    if (result == 0x911Au || result == 0x911Cu) context->mappings().refresh_persistent(*context->wire());
    return result;
}

void ManualCalls::finish()
{
    AV_SCOPE("ManualCalls::finish");
    static constexpr uint32_t gl_sync_gpu_commands_complete = 0x9117u;
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    ClientTransport &wire = *context->wire();
    const uint64_t   fence = glproto::av_glFenceSync(wire, gl_sync_gpu_commands_complete, 0u);
    if (fence == 0u) return glproto::av_glFinish(wire);
    SyncWaitProbe           probe(wire, fence, SyncWaitProbe::gl_sync_flush_commands_bit);
    icd::HostCompletionWait wait(kmt_, icd::SlicedWait::forever);
    static_cast<void>(wait.run(probe));
    glproto::av_glDeleteSync(wire, fence);
    context->mappings().refresh_persistent(wire);
    context->pinned().reclaim();
}

void ManualCalls::debug_message_callback(void *callback, const void *user)
{
    AV_SCOPE("ManualCalls::debug_message_callback");
    GuestGlContext *const context = currents_.current();
    if (context != nullptr) context->debug().set_callback(address_of(callback), address_of(user));
}

void ManualCalls::capability(uint32_t cap, bool on)
{
    AV_SCOPE("ManualCalls::capability");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return;
    if (context->debug().tracks(cap)) context->debug().enable(cap, on);
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return;
    if (on) glproto::av_glEnable(*wire, cap);
    else glproto::av_glDisable(*wire, cap);
}

uint64_t ManualCalls::texture_handle(uint32_t texture, uint32_t sampler, bool with_sampler)
{
    AV_SCOPE("ManualCalls::texture_handle");
    GuestGlContext *const context = sending();
    if (context == nullptr) return 0u;
    ClientTransport &wire = *context->wire();
    const uint64_t   handle = with_sampler ? glproto::av_glGetTextureSamplerHandleARB(wire, texture, sampler)
                                          : glproto::av_glGetTextureHandleARB(wire, texture);
    if (handle != 0u &&
        contexts_.bindless().add(context->group_root(), handle, BindlessHandles::Kind::texture, texture) != Status::ok)
        return 0u;
    return handle;
}

uint64_t ManualCalls::image_handle(uint32_t texture, int32_t level, bool layered, int32_t layer, uint32_t format)
{
    AV_SCOPE("ManualCalls::image_handle");
    GuestGlContext *const context = sending();
    if (context == nullptr) return 0u;
    const uint64_t handle =
        glproto::av_glGetImageHandleARB(*context->wire(), texture, level, layered ? 1 : 0, layer, format);
    if (handle != 0u &&
        contexts_.bindless().add(context->group_root(), handle, BindlessHandles::Kind::image, texture) != Status::ok)
        return 0u;
    return handle;
}

void ManualCalls::make_handle_resident(uint64_t handle, bool image, bool resident, uint32_t access)
{
    AV_SCOPE("ManualCalls::make_handle_resident");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    const BindlessHandles::Kind kind = image ? BindlessHandles::Kind::image : BindlessHandles::Kind::texture;
    if (!contexts_.bindless().valid(context->group_root(), handle, kind)) return record(gl_invalid_operation);
    ClientTransport &wire = *context->wire();
    if (image && resident) glproto::av_glMakeImageHandleResidentARB(wire, handle, access);
    else if (image) glproto::av_glMakeImageHandleNonResidentARB(wire, handle);
    else if (resident) glproto::av_glMakeTextureHandleResidentARB(wire, handle);
    else glproto::av_glMakeTextureHandleNonResidentARB(wire, handle);
}

bool ManualCalls::handle_resident(uint64_t handle, bool image)
{
    AV_SCOPE("ManualCalls::handle_resident");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    const BindlessHandles::Kind kind = image ? BindlessHandles::Kind::image : BindlessHandles::Kind::texture;
    if (!contexts_.bindless().valid(context->group_root(), handle, kind)) {
        record(gl_invalid_operation);
        return false;
    }
    ClientTransport &wire = *context->wire();
    return (image ? glproto::av_glIsImageHandleResidentARB(wire, handle)
                  : glproto::av_glIsTextureHandleResidentARB(wire, handle)) != 0;
}

void ManualCalls::uniform_handles(uint32_t program, bool with_program, int32_t location, int32_t count,
                                  const uint64_t *values)
{
    AV_SCOPE("ManualCalls::uniform_handles");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    if (count < 0) return record(gl_invalid_value);
    if (values == nullptr && count > 0) return record(gl_invalid_value);
    const uint32_t group = context->group_root();
    for (int32_t at = 0; at < count; at++) {
        const bool known = contexts_.bindless().valid(group, values[at], BindlessHandles::Kind::texture) ||
                           contexts_.bindless().valid(group, values[at], BindlessHandles::Kind::image);
        if (!known) return record(gl_invalid_operation);
    }
    ClientTransport &wire = *context->wire();
    if (with_program) glproto::av_glProgramUniformHandleui64vARB(wire, program, location, count, values);
    else glproto::av_glUniformHandleui64vARB(wire, location, count, values);
}

void ManualCalls::delete_textures(int32_t count, const uint32_t *textures)
{
    AV_SCOPE("ManualCalls::delete_textures");
    forget_bindings();
    GuestGlContext *const context = currents_.current();
    if (context == nullptr || context->wire() == nullptr) return;
    contexts_.bindless().forget_textures(context->group_root(), count, textures);
    glproto::av_glDeleteTextures(*context->wire(), count, textures);
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
    if (written == 1 && context->uniforms().remember(program, location, static_cast<uint32_t>(type)) != Status::ok)
        context->record(GuestGlContext::gl_out_of_memory);
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
    context->arrays().note_framebuffer(framebuffer);
    glproto::av_glBindFramebuffer(*context->wire(), target, framebuffers.name_of(framebuffer));
}

bool ManualCalls::is_framebuffer(uint32_t framebuffer)
{
    AV_SCOPE("ManualCalls::is_framebuffer");
    GuestGlContext *const context = sending();
    if (context == nullptr) return false;
    if (!context->arrays().framebuffer_known(framebuffer)) return false;
    return glproto::av_glIsFramebuffer(*context->wire(), context->framebuffers().name_of(framebuffer)) != 0u;
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
    if (context->framebuffers().reads_window() && buffer >= 0x8CE0u && buffer <= 0x8CFFu)
        return record(gl_invalid_operation);
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
    const bool sliced = pack_slice_bound();
    const bool offset = !sliced && context->arrays().pixel_buffer_bound(true);
    if (offset) forget_bound(*context, ClientArrayState::pixel_pack_buffer);
    const size_t bytes = offset ? 0u : context->arrays().pixel_bytes(format, type, width, height, 1, true);
    if (sliced && !slice_pack_pixels(pixels, bytes)) return;
    ClientTransport &wire = *context->wire();
    const bool       staged = !offset && context->staging().stage_download(wire, pixels, bytes);
    uint64_t         start = 0u, row_bytes = 0u, stride = 0u;
    uint8_t         *kept = nullptr;
    if (!offset && !sliced && pixels != nullptr && bytes != 0u &&
        context->arrays().pixel_layout(format, type, width, height, true, start, row_bytes, stride) &&
        (stride != row_bytes || start != 0u))
        kept = context->vertex().staging(bytes);
    if (kept != nullptr) __builtin_memcpy(kept, pixels, bytes);
    glproto::av_glReadPixels(wire, x, y, width, height, format, type, staged ? nullptr : pixels,
                             staged ? 0u : bytes, offset || staged);
    if (staged) context->staging().finish_download(wire, pixels, bytes);
    if (kept != nullptr) {
        uint8_t *const into = static_cast<uint8_t *>(pixels);
        __builtin_memcpy(into, kept, static_cast<size_t>(start < bytes ? start : bytes));
        for (int32_t row = 0; row < height; row++) {
            const uint64_t from = start + static_cast<uint64_t>(row) * stride + row_bytes;
            const uint64_t to = start + static_cast<uint64_t>(row + 1) * stride;
            if (from >= bytes || to <= from) continue;
            const uint64_t end = to < bytes ? to : bytes;
            __builtin_memcpy(into + from, kept + from, static_cast<size_t>(end - from));
        }
    }
    if (sliced) pack_slice_done();
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

const uint8_t *ManualCalls::string_i(uint32_t name, uint32_t index)
{
    AV_SCOPE("ManualCalls::string_i");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return nullptr;
    const char *const answer = name == gl_extensions ? context->extensions().name(index) : nullptr;
    if (answer == nullptr) context->record(name == gl_extensions ? gl_invalid_value : gl_invalid_enum);
    return reinterpret_cast<const uint8_t *>(answer);
}

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
    if (made == 0u) {
        context->record(gl_invalid_operation);
        return false;
    }
    return true;
}

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
    context->record(
        context->externals().set_parameter(SharedObjectKind::monitored_fence, semaphore, pname, 0));
}

void ManualCalls::get_semaphore_parameter(uint32_t semaphore, uint32_t pname, uint64_t *params)
{
    AV_SCOPE("ManualCalls::get_semaphore_parameter");
    GuestGlContext *const context = sending();
    if (context == nullptr) return;
    int32_t unused = 0;
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
    if (context == nullptr || context->wire() == nullptr) return nullptr;
    context->send_forgets();
    return context;
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
    if (!context.arrays().tracks(target)) {
        context.record(gl_invalid_enum);
        return false;
    }
    const uint32_t bound = context.arrays().bound_buffer(target);
    buffer = bound == ClientArrayState::unknown ? 0u : bound;
    return true;
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

uint32_t ManualCalls::sliced_buffer(GuestGlContext &context, uint32_t target)
{
    AV_SCOPE("ManualCalls::sliced_buffer");
    uint32_t buffer = 0u;
    if (context.slices().held() == 0u || !bound_to(context, target, buffer)) return 0u;
    return context.slices().holds(buffer) ? buffer : 0u;
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

void ManualCalls::wrote_bound(GuestGlContext &context, uint32_t target, intptr_t offset, intptr_t size,
                              const void *data)
{
    AV_SCOPE("ManualCalls::wrote_bound");
    const uint32_t buffer = context.arrays().bound_buffer(target);
    if (buffer == ClientArrayState::unknown)
        context.shadows().forget_all();
    else if (offset >= 0 && size >= 0)
        context.shadows().wrote(buffer, static_cast<uint64_t>(offset), static_cast<uint64_t>(size), data);
}

}

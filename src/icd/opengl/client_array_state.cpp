#include "icd/opengl/client_array_state.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {

enum : uint32_t {
    gl_unpack_row_length = 0x0CF2u, gl_unpack_skip_rows = 0x0CF3u, gl_unpack_skip_pixels = 0x0CF4u,
    gl_unpack_alignment = 0x0CF5u, gl_pack_row_length = 0x0D02u, gl_pack_skip_rows = 0x0D03u,
    gl_pack_skip_pixels = 0x0D04u, gl_pack_alignment = 0x0D05u, gl_pack_skip_images = 0x806Bu,
    gl_pack_image_height = 0x806Cu, gl_unpack_skip_images = 0x806Du, gl_unpack_image_height = 0x806Eu,
};

/* The buffer targets whose bindings are kept, in slot order; the element
 * array buffer is each vertex array's own. */
constexpr uint32_t buffer_targets_kept[] = {
    0x8892u /* ARRAY */,         0x88EBu /* PIXEL_PACK */,        0x88ECu /* PIXEL_UNPACK */,
    0x9192u /* QUERY */,         0x8A11u /* UNIFORM */,           0x8C2Au /* TEXTURE */,
    0x8C8Eu /* TRANSFORM_FEEDBACK */, 0x8F36u /* COPY_READ */,    0x8F37u /* COPY_WRITE */,
    0x8F3Fu /* DRAW_INDIRECT */, 0x90D2u /* SHADER_STORAGE */,    0x90EEu /* DISPATCH_INDIRECT */,
    0x92C0u /* ATOMIC_COUNTER */,
};

/* The targets with indexed bindings as well as their own. */
constexpr uint32_t indexed_targets[] = {0x8A11u, 0x8C8Eu, 0x90D2u, 0x92C0u};

/* The texture targets whose bindings are kept, in slot order. */
constexpr uint32_t texture_targets_kept[] = {
    0x0DE0u /* 1D */,       0x0DE1u /* 2D */,       0x806Fu /* 3D */,         0x8C18u /* 1D_ARRAY */,
    0x8C1Au /* 2D_ARRAY */, 0x84F5u /* RECTANGLE */, 0x8513u /* CUBE_MAP */,  0x9009u /* CUBE_MAP_ARRAY */,
    0x8C2Au /* BUFFER */,   0x9100u /* 2D_MULTISAMPLE */, 0x9102u /* 2D_MULTISAMPLE_ARRAY */,
};

}  // namespace

ClientArrayState::ClientArrayState()
    : buffers_{}, buffers_restored_(false), pack_(), unpack_(), vertex_array_(0u), array_names_{}, array_elements_{}, arrays_known_(1u),
      attribute_pointers_{}, active_unit_(unknown), textures_{}, samplers_{}, offsets_{}
{
    AV_SCOPE("ClientArrayState::ClientArrayState");
    forget_bindings();
}

bool ClientArrayState::store_pixel(uint32_t pname, int32_t value)
{
    AV_SCOPE("ClientArrayState::store_pixel");
    PixelStore::Field field = PixelStore::Field::alignment;
    /* The store found is one of this object's own, which this call may change. */
    PixelStore *const store = const_cast<PixelStore *>(store_for(pname, field));
    return store != nullptr && store->set(field, value);
}

int32_t ClientArrayState::pixel_value(uint32_t pname) const
{
    AV_SCOPE("ClientArrayState::pixel_value");
    PixelStore::Field       field = PixelStore::Field::alignment;
    const PixelStore *const store = store_for(pname, field);
    return store != nullptr ? store->get(field) : 0;
}

size_t ClientArrayState::pixel_bytes(uint32_t format, uint32_t type, int32_t width, int32_t height, int32_t depth,
                                     bool packing) const
{
    AV_SCOPE("ClientArrayState::pixel_bytes");
    return (packing ? pack_ : unpack_).span(format, type, width, height, depth);
}

size_t ClientArrayState::compressed_bytes(int32_t image_size, bool packing) const
{
    AV_SCOPE("ClientArrayState::compressed_bytes");
    return packing || image_size < 0 ? 0u : static_cast<size_t>(image_size);
}

void ClientArrayState::bind_buffer(uint32_t target, uint32_t buffer)
{
    AV_SCOPE("ClientArrayState::bind_buffer");
    if (target == element_array_buffer) {
        const uint32_t row = vertex_array_row(vertex_array_);
        if (row != max_vertex_arrays) array_elements_[row] = buffer;
        return;
    }
    const uint32_t slot = buffer_slot(target);
    if (slot != buffer_targets) buffers_[slot] = buffer;
}

void ClientArrayState::lose_buffer(uint32_t target)
{
    AV_SCOPE("ClientArrayState::lose_buffer");
    if (indexed(target)) buffers_[buffer_slot(target)] = unknown;
}

void ClientArrayState::restore_buffers()
{
    AV_SCOPE("ClientArrayState::restore_buffers");
    buffers_restored_ = true;
}

bool ClientArrayState::known_buffer(uint32_t target, uint32_t &buffer) const
{
    AV_SCOPE("ClientArrayState::known_buffer");
    const uint32_t slot = buffer_slot(target);
    if (buffers_restored_ || slot == buffer_targets || buffers_[slot] == unknown) return false;
    buffer = buffers_[slot];
    return true;
}

void ClientArrayState::delete_buffers(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("ClientArrayState::delete_buffers");
    if (buffers == nullptr) return;
    for (int32_t index = 0; index < count; index++) {
        const uint32_t name = buffers[index];
        if (name == 0u) continue;
        for (uint32_t &bound : buffers_)
            if (bound == name) bound = 0u;
        for (uint32_t row = 0u; row < arrays_known_; row++)
            if (array_elements_[row] == name) array_elements_[row] = 0u;
    }
}

uint32_t ClientArrayState::bound_buffer(uint32_t target) const
{
    AV_SCOPE("ClientArrayState::bound_buffer");
    if (target == element_array_buffer) {
        const uint32_t row = vertex_array_row(vertex_array_);
        return row == max_vertex_arrays ? unknown : array_elements_[row];
    }
    const uint32_t slot = buffer_slot(target);
    return slot == buffer_targets ? 0u : buffers_[slot];
}

bool ClientArrayState::pixel_buffer_bound(bool packing) const
{
    AV_SCOPE("ClientArrayState::pixel_buffer_bound");
    return bound_buffer(packing ? pixel_pack_buffer : pixel_unpack_buffer) != 0u;
}

bool ClientArrayState::query_buffer_bound() const
{
    AV_SCOPE("ClientArrayState::query_buffer_bound");
    return bound_buffer(query_buffer) != 0u;
}

void ClientArrayState::bind_vertex_array(uint32_t array)
{
    AV_SCOPE("ClientArrayState::bind_vertex_array");
    vertex_array_ = array;
    if (vertex_array_row(array) != max_vertex_arrays || arrays_known_ == max_vertex_arrays) return;
    array_names_[arrays_known_] = array;
    array_elements_[arrays_known_] = 0u;
    arrays_known_++;
}

void ClientArrayState::delete_vertex_arrays(int32_t count, const uint32_t *arrays)
{
    AV_SCOPE("ClientArrayState::delete_vertex_arrays");
    if (arrays == nullptr) return;
    for (int32_t index = 0; index < count; index++) {
        const uint32_t name = arrays[index];
        const uint32_t row = vertex_array_row(name);
        if (name == 0u || row == max_vertex_arrays) continue;
        arrays_known_--;
        array_names_[row] = array_names_[arrays_known_];
        array_elements_[row] = array_elements_[arrays_known_];
        if (vertex_array_ == name) vertex_array_ = 0u;
    }
}

bool ClientArrayState::indices_are_offset() const
{
    AV_SCOPE("ClientArrayState::indices_are_offset");
    return bound_buffer(element_array_buffer) != 0u;
}

bool ClientArrayState::client_pointer(uint64_t pointer) const
{
    AV_SCOPE("ClientArrayState::client_pointer");
    return pointer != 0u && bound_buffer(array_buffer) == 0u;
}

void ClientArrayState::set_attribute_pointer(uint32_t index, uint64_t pointer)
{
    AV_SCOPE("ClientArrayState::set_attribute_pointer");
    if (index < max_attributes) attribute_pointers_[index] = pointer;
}

uint64_t ClientArrayState::attribute_pointer(uint32_t index) const
{
    AV_SCOPE("ClientArrayState::attribute_pointer");
    return index < max_attributes ? attribute_pointers_[index] : 0u;
}

const uint64_t *ClientArrayState::offsets_of(const void *const *pointers, size_t count)
{
    AV_SCOPE("ClientArrayState::offsets_of");
    if (count > max_offsets || (pointers == nullptr && count != 0u)) return nullptr;
    for (size_t index = 0u; index < count; index++)
        offsets_[index] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointers[index]));
    return offsets_;
}

bool ClientArrayState::skip_active_texture(uint32_t texture)
{
    AV_SCOPE("ClientArrayState::skip_active_texture");
    const uint32_t unit = texture - texture0;
    if (texture < texture0 || unit >= units) return false;
    const bool same = active_unit_ == unit;
    active_unit_ = unit;
    return same;
}

bool ClientArrayState::skip_bind_texture(uint32_t target, uint32_t texture)
{
    AV_SCOPE("ClientArrayState::skip_bind_texture");
    const uint32_t slot = texture_slot(target);
    if (active_unit_ == unknown || slot == texture_targets) return false;
    const bool same = textures_[active_unit_][slot] == texture;
    textures_[active_unit_][slot] = texture;
    return same;
}

bool ClientArrayState::skip_bind_sampler(uint32_t unit, uint32_t sampler)
{
    AV_SCOPE("ClientArrayState::skip_bind_sampler");
    if (unit >= units) return false;
    const bool same = samplers_[unit] == sampler;
    samplers_[unit] = sampler;
    return same;
}

void ClientArrayState::forget_bindings()
{
    AV_SCOPE("ClientArrayState::forget_bindings");
    active_unit_ = unknown;
    for (uint32_t unit = 0u; unit < units; unit++) {
        samplers_[unit] = unknown;
        for (uint32_t &texture : textures_[unit]) texture = unknown;
    }
}

bool ClientArrayState::invariants_hold() const
{
    AV_SCOPE("ClientArrayState::invariants_hold");
    if (!pack_.invariants_hold() || !unpack_.invariants_hold()) return false;
    if (active_unit_ != unknown && active_unit_ >= units) return false;
    if (arrays_known_ == 0u || arrays_known_ > max_vertex_arrays || array_names_[0] != 0u) return false;
    for (uint32_t row = 1u; row < arrays_known_; row++)
        for (uint32_t earlier = 0u; earlier < row; earlier++)
            if (array_names_[earlier] == array_names_[row]) return false;
    return true;
}

uint32_t ClientArrayState::buffer_slot(uint32_t target) const
{
    AV_SCOPE("ClientArrayState::buffer_slot");
    for (uint32_t slot = 0u; slot < buffer_targets; slot++)
        if (buffer_targets_kept[slot] == target) return slot;
    return buffer_targets;
}

bool ClientArrayState::indexed(uint32_t target) const
{
    AV_SCOPE("ClientArrayState::indexed");
    for (const uint32_t kept : indexed_targets)
        if (kept == target) return true;
    return false;
}

uint32_t ClientArrayState::texture_slot(uint32_t target) const
{
    AV_SCOPE("ClientArrayState::texture_slot");
    uint32_t slot = 0u;
    while (slot < texture_targets && texture_targets_kept[slot] != target) slot++;
    return slot;
}

const PixelStore *ClientArrayState::store_for(uint32_t pname, PixelStore::Field &field) const
{
    AV_SCOPE("ClientArrayState::store_for");
    using Field = PixelStore::Field;
    switch (pname) {
    case gl_pack_alignment: field = Field::alignment; return &pack_;
    case gl_pack_row_length: field = Field::row_length; return &pack_;
    case gl_pack_image_height: field = Field::image_height; return &pack_;
    case gl_pack_skip_pixels: field = Field::skip_pixels; return &pack_;
    case gl_pack_skip_rows: field = Field::skip_rows; return &pack_;
    case gl_pack_skip_images: field = Field::skip_images; return &pack_;
    case gl_unpack_alignment: field = Field::alignment; return &unpack_;
    case gl_unpack_row_length: field = Field::row_length; return &unpack_;
    case gl_unpack_image_height: field = Field::image_height; return &unpack_;
    case gl_unpack_skip_pixels: field = Field::skip_pixels; return &unpack_;
    case gl_unpack_skip_rows: field = Field::skip_rows; return &unpack_;
    case gl_unpack_skip_images: field = Field::skip_images; return &unpack_;
    default: return nullptr;
    }
}

uint32_t ClientArrayState::vertex_array_row(uint32_t array) const
{
    AV_SCOPE("ClientArrayState::vertex_array_row");
    for (uint32_t row = 0u; row < arrays_known_; row++)
        if (array_names_[row] == array) return row;
    return max_vertex_arrays;
}

}  // namespace av::glicd

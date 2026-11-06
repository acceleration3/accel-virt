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

constexpr uint32_t buffer_targets_kept[] = {
    0x8892u ,         0x88EBu ,        0x88ECu ,
    0x9192u ,         0x8A11u ,           0x8C2Au ,
    0x8C8Eu , 0x8F36u ,    0x8F37u ,
    0x8F3Fu , 0x90D2u ,    0x90EEu ,
    0x92C0u ,
};

constexpr uint32_t texture_targets_kept[] = {
    0x0DE0u ,       0x0DE1u ,       0x806Fu ,         0x8C18u ,
    0x8C1Au , 0x84F5u , 0x8513u ,  0x9009u ,
    0x8C2Au ,   0x9100u , 0x9102u ,
};

}

ClientArrayState::ClientArrayState(BlockSource &memory)
    : buffers_{}, pushed_{}, depth_(0u), unsaved_(0u), host_depth_limit_(least_depth_limit), pack_(), unpack_(), vertex_array_(0u), default_elements_(0u),
      elements_(memory, Pool::paged), attribute_pointers_{}, active_unit_(unknown), textures_{}, samplers_{},
      offsets_(memory, Pool::paged), no_offsets_(0u)
{
    AV_SCOPE("ClientArrayState::ClientArrayState");
    forget_bindings();
}

bool ClientArrayState::store_pixel(uint32_t pname, int32_t value)
{
    AV_SCOPE("ClientArrayState::store_pixel");
    PixelStore::Field field = PixelStore::Field::alignment;
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
        uint32_t *const elements = elements_of(vertex_array_);
        if (elements != nullptr) *elements = buffer;
        return;
    }
    const uint32_t slot = buffer_slot(target);
    if (slot != buffer_targets) buffers_[slot] = buffer;
}

bool ClientArrayState::tracks(uint32_t target) const
{
    AV_SCOPE("ClientArrayState::tracks");
    return target == element_array_buffer || buffer_slot(target) != buffer_targets;
}

void ClientArrayState::push_client(uint32_t mask)
{
    AV_SCOPE("ClientArrayState::push_client");
    if (depth_ + unsaved_ >= host_depth_limit_) return;
    if (depth_ >= most_pushed) {
        unsaved_++;
        return;
    }
    const uint32_t *const elements = elements_of(vertex_array_);
    pushed_[depth_] = Pushed{mask,
                             buffers_[buffer_slot(array_buffer)],
                             buffers_[buffer_slot(pixel_pack_buffer)],
                             buffers_[buffer_slot(pixel_unpack_buffer)],
                             vertex_array_,
                             elements != nullptr ? *elements : 0u,
                             pack_,
                             unpack_};
    depth_++;
}

void ClientArrayState::pop_client()
{
    AV_SCOPE("ClientArrayState::pop_client");
    if (unsaved_ != 0u) {
        unsaved_--;
        forget_unsaved_bindings();
        return;
    }
    if (depth_ == 0u) return;
    depth_--;
    const Pushed &saved = pushed_[depth_];
    if ((saved.mask_ & client_pixel_store_bit) != 0u) {
        buffers_[buffer_slot(pixel_pack_buffer)] = saved.pack_buffer_;
        buffers_[buffer_slot(pixel_unpack_buffer)] = saved.unpack_buffer_;
        pack_ = saved.pack_;
        unpack_ = saved.unpack_;
    }
    if ((saved.mask_ & client_vertex_array_bit) == 0u) return;
    buffers_[buffer_slot(array_buffer)] = saved.array_buffer_;
    if (saved.vertex_array_ != 0u && elements_.find(saved.vertex_array_) == nullptr) {
        vertex_array_ = 0u;
        return;
    }
    vertex_array_ = saved.vertex_array_;
    set_vertex_array_elements(vertex_array_, saved.elements_);
}

void ClientArrayState::limit_client_depth(int32_t reported)
{
    AV_SCOPE("ClientArrayState::limit_client_depth");
    if (reported <= 0) return;
    host_depth_limit_ = static_cast<uint32_t>(reported);
}

void ClientArrayState::forget_unsaved_bindings()
{
    AV_SCOPE("ClientArrayState::forget_unsaved_bindings");
    buffers_[buffer_slot(array_buffer)] = unknown;
    buffers_[buffer_slot(pixel_pack_buffer)] = unknown;
    buffers_[buffer_slot(pixel_unpack_buffer)] = unknown;
    set_vertex_array_elements(vertex_array_, unknown);
}

void ClientArrayState::forget_buffer(uint32_t target)
{
    AV_SCOPE("ClientArrayState::forget_buffer");
    const uint32_t slot = buffer_slot(target);
    if (slot != buffer_targets) buffers_[slot] = unknown;
}

void ClientArrayState::reset_client(uint32_t mask)
{
    AV_SCOPE("ClientArrayState::reset_client");
    if ((mask & client_pixel_store_bit) != 0u) {
        buffers_[buffer_slot(pixel_pack_buffer)] = 0u;
        buffers_[buffer_slot(pixel_unpack_buffer)] = 0u;
        pack_ = PixelStore();
        unpack_ = PixelStore();
    }
    if ((mask & client_vertex_array_bit) == 0u) return;
    buffers_[buffer_slot(array_buffer)] = 0u;
    set_vertex_array_elements(vertex_array_, 0u);
}

void ClientArrayState::set_vertex_array_elements(uint32_t array, uint32_t buffer)
{
    AV_SCOPE("ClientArrayState::set_vertex_array_elements");
    uint32_t *const elements = elements_of(array);
    if (elements != nullptr) *elements = buffer;
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
        if (default_elements_ == name) default_elements_ = 0u;
        elements_.for_each([name](uint32_t, uint32_t &elements) {
            if (elements == name) elements = 0u;
        });
    }
}

uint32_t ClientArrayState::bound_buffer(uint32_t target) const
{
    AV_SCOPE("ClientArrayState::bound_buffer");
    if (target == element_array_buffer) {
        const uint32_t *const elements = elements_of(vertex_array_);
        return elements == nullptr ? unknown : *elements;
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

Status ClientArrayState::bind_vertex_array(uint32_t array)
{
    AV_SCOPE("ClientArrayState::bind_vertex_array");
    vertex_array_ = array;
    if (elements_of(array) != nullptr) return Status::ok;
    uint32_t slot = 0u;
    return elements_.insert(array, 0u, slot);
}

void ClientArrayState::delete_vertex_arrays(int32_t count, const uint32_t *arrays)
{
    AV_SCOPE("ClientArrayState::delete_vertex_arrays");
    if (arrays == nullptr) return;
    for (int32_t index = 0; index < count; index++) {
        const uint32_t name = arrays[index];
        if (name == 0u) continue;
        static_cast<void>(elements_.erase(name));
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
    if (pointers == nullptr && count != 0u) return nullptr;
    if (count == 0u) return &no_offsets_;
    if (count > 0x7FFFFFFFu || offsets_.resize(static_cast<uint32_t>(count), 0u) != Status::ok) return nullptr;
    for (size_t index = 0u; index < count; index++)
        offsets_.at(static_cast<uint32_t>(index)) = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointers[index]));
    return offsets_.begin();
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
    if (depth_ > most_pushed || (unsaved_ != 0u && depth_ != most_pushed)) return false;
    return elements_.find(0u) == nullptr && elements_.invariants_hold();
}

uint32_t ClientArrayState::buffer_slot(uint32_t target) const
{
    AV_SCOPE("ClientArrayState::buffer_slot");
    for (uint32_t slot = 0u; slot < buffer_targets; slot++)
        if (buffer_targets_kept[slot] == target) return slot;
    return buffer_targets;
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

uint32_t *ClientArrayState::elements_of(uint32_t array)
{
    AV_SCOPE("ClientArrayState::elements_of");
    return array == 0u ? &default_elements_ : elements_.find(array);
}

const uint32_t *ClientArrayState::elements_of(uint32_t array) const
{
    AV_SCOPE("ClientArrayState::elements_of");
    return array == 0u ? &default_elements_ : elements_.find(array);
}

}

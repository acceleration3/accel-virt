#include "icd/opengl/client_vertex_state.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {

enum : uint32_t {
    gl_vertex_array = 0x8074u, gl_normal_array = 0x8075u, gl_color_array = 0x8076u, gl_index_array = 0x8077u,
    gl_texture_coord_array = 0x8078u, gl_edge_flag_array = 0x8079u, gl_fog_coord_array = 0x8457u,
    gl_secondary_color_array = 0x845Eu,
};

}

ClientVertexState::ClientVertexState(BlockSource &memory)
    : arrays_(memory, Pool::paged), default_{}, current_(&default_), bound_(0u), unit_(0u), host_unit_(0u),
      scratch_(0u), saved_(memory, Pool::paged), staging_(memory, Pool::paged), offsets_(memory, Pool::paged), saved_bits_(0u), depth_(0u), indexed_(false), program_length_(0u)
{
    AV_SCOPE("ClientVertexState::ClientVertexState");
}

ClientVertexState::Arrays *ClientVertexState::arrays_of(uint32_t array)
{
    AV_SCOPE("ClientVertexState::arrays_of");
    return array == 0u ? &default_ : arrays_.find(array);
}

Status ClientVertexState::bind(uint32_t array)
{
    AV_SCOPE("ClientVertexState::bind");
    Arrays *found = arrays_of(array);
    if (found == nullptr) {
        uint32_t slot = 0u;
        const Status inserted = arrays_.insert(array, Arrays{}, slot);
        if (inserted != Status::ok) {
            bound_ = 0u;
            current_ = &default_;
            return inserted;
        }
        found = arrays_.find(array);
    }
    bound_ = array;
    current_ = found;
    return Status::ok;
}

void ClientVertexState::forget(int32_t count, const uint32_t *arrays)
{
    AV_SCOPE("ClientVertexState::forget");
    if (arrays == nullptr) return;
    for (int32_t at = 0; at < count; at++) {
        const uint32_t name = arrays[at];
        if (name == 0u) continue;
        static_cast<void>(arrays_.erase(name));
        if (bound_ == name) {
            bound_ = 0u;
            current_ = &default_;
        }
    }
}

uint32_t ClientVertexState::bound() const
{
    AV_SCOPE("ClientVertexState::bound");
    return bound_;
}

const ClientVertexState::Arrays &ClientVertexState::current() const
{
    AV_SCOPE("ClientVertexState::current");
    return *current_;
}

const ClientVertexState::Slot &ClientVertexState::slot(uint32_t index) const
{
    AV_SCOPE("ClientVertexState::slot");
    return current_->slots_[index < slot_count ? index : 0u];
}

bool ClientVertexState::client_active() const
{
    AV_SCOPE("ClientVertexState::client_active");
    return current_->client_ != 0u;
}

uint64_t ClientVertexState::client_slots() const
{
    AV_SCOPE("ClientVertexState::client_slots");
    return current_->client_;
}

uint64_t ClientVertexState::host_slots() const
{
    AV_SCOPE("ClientVertexState::host_slots");
    return current_->host_;
}

uint32_t ClientVertexState::slot_of_capability(uint32_t capability) const
{
    AV_SCOPE("ClientVertexState::slot_of_capability");
    switch (capability) {
    case gl_vertex_array: return vertex_slot;
    case gl_normal_array: return normal_slot;
    case gl_color_array: return color_slot;
    case gl_index_array: return index_slot;
    case gl_texture_coord_array: return texcoord_slot + unit_;
    case gl_edge_flag_array: return edge_flag_slot;
    case gl_fog_coord_array: return fog_coord_slot;
    case gl_secondary_color_array: return secondary_color_slot;
    default: return no_slot;
    }
}

uint32_t ClientVertexState::texcoord_unit() const
{
    AV_SCOPE("ClientVertexState::texcoord_unit");
    return unit_;
}

bool ClientVertexState::select_texcoord_unit(uint32_t texture)
{
    AV_SCOPE("ClientVertexState::select_texcoord_unit");
    if (texture < texture0 || texture - texture0 >= texcoord_units) return false;
    unit_ = texture - texture0;
    return true;
}

uint32_t ClientVertexState::texcoord_slot_of_unit(uint32_t unit) const
{
    AV_SCOPE("ClientVertexState::texcoord_slot_of_unit");
    return texcoord_slot + (unit < texcoord_units ? unit : 0u);
}

uint32_t ClientVertexState::host_unit() const
{
    AV_SCOPE("ClientVertexState::host_unit");
    return host_unit_;
}

void ClientVertexState::host_selected_unit(uint32_t unit)
{
    AV_SCOPE("ClientVertexState::host_selected_unit");
    host_unit_ = unit;
}

void ClientVertexState::refresh(Arrays &arrays, uint32_t index) const
{
    AV_SCOPE("ClientVertexState::refresh");
    const Slot    &state = arrays.slots_[index];
    const uint64_t bit = uint64_t{1} << index;
    if (state.enabled_ != 0u && state.mode_ == static_cast<uint8_t>(Mode::client)) arrays.client_ |= bit;
    else arrays.client_ &= ~bit;
    if (state.host_enabled_ != 0u) arrays.host_ |= bit;
    else arrays.host_ &= ~bit;
}

void ClientVertexState::set_pointer(uint32_t index, int32_t size, uint32_t type, bool normalized, int32_t stride,
                                    uint64_t pointer, Mode mode)
{
    AV_SCOPE("ClientVertexState::set_pointer");
    if (index >= slot_count) return;
    Slot &state = current_->slots_[index];
    state.pointer_ = pointer;
    state.size_ = size;
    state.type_ = type;
    state.stride_ = stride;
    state.normalized_ = normalized ? 1u : 0u;
    state.mode_ = static_cast<uint8_t>(mode);
    refresh(*current_, index);
}

void ClientVertexState::set_enabled(uint32_t index, bool enabled)
{
    AV_SCOPE("ClientVertexState::set_enabled");
    if (index >= slot_count) return;
    current_->slots_[index].enabled_ = enabled ? 1u : 0u;
    refresh(*current_, index);
}

void ClientVertexState::set_host_enabled(uint32_t index, bool enabled)
{
    AV_SCOPE("ClientVertexState::set_host_enabled");
    if (index >= slot_count) return;
    current_->slots_[index].host_enabled_ = enabled ? 1u : 0u;
    refresh(*current_, index);
}

void ClientVertexState::clear_slot(uint32_t index)
{
    AV_SCOPE("ClientVertexState::clear_slot");
    if (index >= slot_count) return;
    current_->slots_[index] = Slot{};
    refresh(*current_, index);
}

uint32_t ClientVertexState::scratch() const
{
    AV_SCOPE("ClientVertexState::scratch");
    return scratch_;
}

void ClientVertexState::set_scratch(uint32_t buffer)
{
    AV_SCOPE("ClientVertexState::set_scratch");
    scratch_ = buffer;
}

void ClientVertexState::forget_scratch()
{
    AV_SCOPE("ClientVertexState::forget_scratch");
    scratch_ = 0u;
}

uint32_t ClientVertexState::program_length() const
{
    AV_SCOPE("ClientVertexState::program_length");
    return program_length_;
}

void ClientVertexState::set_program_length(int32_t length)
{
    AV_SCOPE("ClientVertexState::set_program_length");
    program_length_ = length > 0 ? static_cast<uint32_t>(length) : 0u;
}

bool ClientVertexState::indexed() const
{
    AV_SCOPE("ClientVertexState::indexed");
    return indexed_;
}

void ClientVertexState::set_indexed(bool indexed)
{
    AV_SCOPE("ClientVertexState::set_indexed");
    indexed_ = indexed;
}

uint32_t ClientVertexState::validate(uint32_t slot, int32_t size, uint32_t type, int32_t stride) const
{
    AV_SCOPE("ClientVertexState::validate");
    constexpr uint32_t bit_byte = 1u << 0u, bit_ubyte = 1u << 1u, bit_short = 1u << 2u, bit_ushort = 1u << 3u,
                       bit_int = 1u << 4u, bit_uint = 1u << 5u, bit_float = 1u << 6u, bit_double = 1u << 7u,
                       bit_half = 1u << 8u, bit_fixed = 1u << 9u, bit_packed = 1u << 10u;
    if (stride < 0) return 0x0501u;
    uint32_t allowed = 0u;
    int32_t  least = 1, most = 4;
    if (slot < generic_slots) {
        allowed = bit_byte | bit_ubyte | bit_short | bit_ushort | bit_int | bit_uint | bit_float | bit_double | bit_half |
                  bit_fixed | bit_packed;
    } else if (slot >= texcoord_slot) {
        allowed = bit_short | bit_int | bit_float | bit_double | bit_half | bit_packed;
    } else {
        switch (slot) {
        case vertex_slot: allowed = bit_short | bit_int | bit_float | bit_double | bit_half | bit_packed; least = 2; break;
        case normal_slot: allowed = bit_byte | bit_short | bit_int | bit_float | bit_double | bit_half | bit_packed; least = 3; most = 3; break;
        case color_slot:
            allowed = bit_byte | bit_ubyte | bit_short | bit_ushort | bit_int | bit_uint | bit_float | bit_double | bit_half |
                      bit_packed;
            least = 3;
            break;
        case secondary_color_slot:
            allowed = bit_byte | bit_ubyte | bit_short | bit_ushort | bit_int | bit_uint | bit_float | bit_double | bit_half;
            least = 3;
            most = 3;
            break;
        case index_slot: allowed = bit_ubyte | bit_short | bit_int | bit_float | bit_double; least = 1; most = 1; break;
        case fog_coord_slot: allowed = bit_float | bit_double | bit_half; least = 1; most = 1; break;
        default: return 0u;
        }
    }
    uint32_t have = 0u;
    switch (type) {
    case 0x1400u: have = bit_byte; break;
    case 0x1401u: have = bit_ubyte; break;
    case 0x1402u: have = bit_short; break;
    case 0x1403u: have = bit_ushort; break;
    case 0x1404u: have = bit_int; break;
    case 0x1405u: have = bit_uint; break;
    case 0x1406u: have = bit_float; break;
    case 0x140Au: have = bit_double; break;
    case 0x140Bu: have = bit_half; break;
    case 0x140Cu: have = bit_fixed; break;
    case 0x8368u:
    case 0x8D9Fu:
    case 0x8C3Bu: have = bit_packed; break;
    default: break;
    }
    if ((allowed & have) == 0u) return 0x0500u;
    const bool bgra = size == 0x80E1;
    if (!bgra && (size < least || size > most)) return 0x0501u;
    if (bgra && slot != color_slot && slot >= generic_slots) return 0x0501u;
    return 0u;
}

uint8_t *ClientVertexState::staging(uint64_t bytes)
{
    AV_SCOPE("ClientVertexState::staging");
    if (bytes == 0u) return nullptr;
    if (bytes > uint64_t{1} << 30u || staging_.resize(static_cast<uint32_t>(bytes), 0u) != Status::ok) return nullptr;
    return staging_.begin();
}

uint64_t *ClientVertexState::offset_table(uint32_t count)
{
    AV_SCOPE("ClientVertexState::offset_table");
    if (count == 0u || offsets_.resize(count, 0u) != Status::ok) return nullptr;
    return offsets_.begin();
}

void ClientVertexState::push(uint32_t mask)
{
    AV_SCOPE("ClientVertexState::push");
    if (depth_ >= most_pushed) return;
    const bool kept = (mask & client_vertex_array_bit) != 0u;
    if (kept && saved_.append(Saved{bound_, *current_}) != Status::ok) return;
    saved_bits_ = (saved_bits_ << 1u) | (kept ? 1u : 0u);
    depth_++;
}

void ClientVertexState::pop()
{
    AV_SCOPE("ClientVertexState::pop");
    if (depth_ == 0u) return;
    const bool kept = (saved_bits_ & 1u) != 0u;
    saved_bits_ >>= 1u;
    depth_--;
    if (!kept || saved_.size() == 0u) return;
    const Saved restored = saved_.at(saved_.size() - 1u);
    saved_.truncate(saved_.size() - 1u);
    Arrays *const target = arrays_of(restored.array_);
    if (target != nullptr) *target = restored.arrays_;
}

void ClientVertexState::reset(uint32_t mask)
{
    AV_SCOPE("ClientVertexState::reset");
    if ((mask & client_vertex_array_bit) == 0u) return;
    for (uint32_t index = 0u; index < slot_count; index++) {
        Slot &state = current_->slots_[index];
        state.pointer_ = 0u;
        state.size_ = 4;
        state.type_ = 0x1406u;
        state.stride_ = 0;
        state.mode_ = static_cast<uint8_t>(Mode::unset);
        state.normalized_ = 0u;
        state.enabled_ = 0u;
        state.host_enabled_ = 0u;
        refresh(*current_, index);
    }
}

bool ClientVertexState::invariants_hold() const
{
    AV_SCOPE("ClientVertexState::invariants_hold");
    if (unit_ >= texcoord_units || depth_ > most_pushed || current_ == nullptr) return false;
    if (!arrays_.invariants_hold() || !saved_.invariants_hold()) return false;
    for (uint32_t index = 0u; index < slot_count; index++) {
        const Slot    &state = current_->slots_[index];
        const uint64_t bit = uint64_t{1} << index;
        const bool     client = state.enabled_ != 0u && state.mode_ == static_cast<uint8_t>(Mode::client);
        if (((current_->client_ & bit) != 0u) != client) return false;
        if (((current_->host_ & bit) != 0u) != (state.host_enabled_ != 0u)) return false;
    }
    return true;
}

}

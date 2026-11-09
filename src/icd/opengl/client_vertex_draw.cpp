#include "icd/opengl/client_vertex_draw.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/buffer_pieces.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/client_vertex_state.hpp"
#include "proto/client/client_transport.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

enum : uint32_t {
    gl_byte = 0x1400u, gl_unsigned_byte = 0x1401u, gl_short = 0x1402u, gl_unsigned_short = 0x1403u,
    gl_int = 0x1404u, gl_unsigned_int = 0x1405u, gl_float = 0x1406u, gl_double = 0x140Au, gl_half_float = 0x140Bu,
    gl_fixed = 0x140Cu, gl_bgra = 0x80E1u, gl_int_2_10_10_10_rev = 0x8D9Fu, gl_unsigned_int_2_10_10_10_rev = 0x8368u,
    gl_stream_draw = 0x88E0u, gl_array_buffer_binding = 0x8894u, gl_element_array_buffer_binding = 0x8895u,
    gl_vertex_array = 0x8074u, gl_normal_array = 0x8075u, gl_color_array = 0x8076u, gl_index_array = 0x8077u,
    gl_texture_coord_array = 0x8078u, gl_edge_flag_array = 0x8079u, gl_fog_coord_array = 0x8457u,
    gl_secondary_color_array = 0x845Eu, texture0 = 0x84C0u,
};

constexpr uint32_t align_to = 16u;

struct Interleaved {
    uint32_t format;
    bool     texture;
    bool     color;
    bool     normal;
    int32_t  texture_size;
    int32_t  color_size;
    uint32_t color_type;
    int32_t  vertex_size;
    uint32_t color_at;
    uint32_t normal_at;
    uint32_t vertex_at;
    int32_t  stride;
};

constexpr Interleaved interleaved_formats[] = {
    {0x2A20u, false, false, false, 0, 0, gl_float, 2, 0u, 0u, 0u, 8},
    {0x2A21u, false, false, false, 0, 0, gl_float, 3, 0u, 0u, 0u, 12},
    {0x2A22u, false, true, false, 0, 4, gl_unsigned_byte, 2, 0u, 0u, 4u, 12},
    {0x2A23u, false, true, false, 0, 4, gl_unsigned_byte, 3, 0u, 0u, 4u, 16},
    {0x2A24u, false, true, false, 0, 3, gl_float, 3, 0u, 0u, 12u, 24},
    {0x2A25u, false, false, true, 0, 0, gl_float, 3, 0u, 0u, 12u, 24},
    {0x2A26u, false, true, true, 0, 4, gl_float, 3, 0u, 16u, 28u, 40},
    {0x2A27u, true, false, false, 2, 0, gl_float, 3, 0u, 0u, 8u, 20},
    {0x2A28u, true, false, false, 4, 0, gl_float, 4, 0u, 0u, 16u, 32},
    {0x2A29u, true, true, false, 2, 4, gl_unsigned_byte, 3, 8u, 0u, 12u, 24},
    {0x2A2Au, true, true, false, 2, 3, gl_float, 3, 8u, 0u, 20u, 32},
    {0x2A2Bu, true, false, true, 2, 0, gl_float, 3, 0u, 8u, 20u, 32},
    {0x2A2Cu, true, true, true, 2, 4, gl_float, 3, 8u, 24u, 36u, 48},
    {0x2A2Du, true, true, true, 4, 4, gl_float, 4, 16u, 32u, 44u, 60},
};

uint64_t address_of(const void *pointer)
{
    AV_SCOPE("address_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer));
}

uint32_t type_bytes(uint32_t type)
{
    AV_SCOPE("type_bytes");
    switch (type) {
    case gl_byte:
    case gl_unsigned_byte: return 1u;
    case gl_short:
    case gl_unsigned_short:
    case gl_half_float: return 2u;
    case gl_double: return 8u;
    default: return 4u;
    }
}

uint32_t aligned(uint64_t value)
{
    AV_SCOPE("aligned");
    return static_cast<uint32_t>((value + align_to - 1u) & ~uint64_t{align_to - 1u});
}

uint32_t capability_of(uint32_t slot)
{
    AV_SCOPE("capability_of");
    switch (slot) {
    case ClientVertexState::vertex_slot: return gl_vertex_array;
    case ClientVertexState::normal_slot: return gl_normal_array;
    case ClientVertexState::color_slot: return gl_color_array;
    case ClientVertexState::secondary_color_slot: return gl_secondary_color_array;
    case ClientVertexState::index_slot: return gl_index_array;
    case ClientVertexState::edge_flag_slot: return gl_edge_flag_array;
    case ClientVertexState::fog_coord_slot: return gl_fog_coord_array;
    default: return gl_texture_coord_array;
    }
}

float component(uint32_t type, const uint8_t *at, bool normalized)
{
    AV_SCOPE("component");
    switch (type) {
    case gl_byte: {
        int8_t value;
        __builtin_memcpy(&value, at, sizeof value);
        const float scaled = static_cast<float>(value) / 127.0f;
        return normalized ? (scaled < -1.0f ? -1.0f : scaled) : static_cast<float>(value);
    }
    case gl_unsigned_byte: {
        uint8_t value;
        __builtin_memcpy(&value, at, sizeof value);
        return normalized ? static_cast<float>(value) / 255.0f : static_cast<float>(value);
    }
    case gl_short: {
        int16_t value;
        __builtin_memcpy(&value, at, sizeof value);
        const float scaled = static_cast<float>(value) / 32767.0f;
        return normalized ? (scaled < -1.0f ? -1.0f : scaled) : static_cast<float>(value);
    }
    case gl_unsigned_short: {
        uint16_t value;
        __builtin_memcpy(&value, at, sizeof value);
        return normalized ? static_cast<float>(value) / 65535.0f : static_cast<float>(value);
    }
    case gl_int: {
        int32_t value;
        __builtin_memcpy(&value, at, sizeof value);
        const double scaled = static_cast<double>(value) / 2147483647.0;
        return normalized ? static_cast<float>(scaled < -1.0 ? -1.0 : scaled) : static_cast<float>(value);
    }
    case gl_unsigned_int: {
        uint32_t value;
        __builtin_memcpy(&value, at, sizeof value);
        return normalized ? static_cast<float>(static_cast<double>(value) / 4294967295.0) : static_cast<float>(value);
    }
    case gl_double: {
        double value;
        __builtin_memcpy(&value, at, sizeof value);
        return static_cast<float>(value);
    }
    case gl_fixed: {
        int32_t value;
        __builtin_memcpy(&value, at, sizeof value);
        return static_cast<float>(value) / 65536.0f;
    }
    case gl_float: {
        float value;
        __builtin_memcpy(&value, at, sizeof value);
        return value;
    }
    default: return 0.0f;
    }
}

}

ClientVertexDraw::ClientVertexDraw(ClientTransport &wire, ClientVertexState &state, ClientArrayState &buffers,
                                   BufferShadows &shadows)
    : wire_(wire), state_(state), buffers_(buffers), shadows_(shadows), user_array_(0u), staged_(false)
{
    AV_SCOPE("ClientVertexDraw::ClientVertexDraw");
}

uint32_t ClientVertexDraw::user_array_buffer()
{
    AV_SCOPE("ClientVertexDraw::user_array_buffer");
    const uint32_t known = buffers_.bound_buffer(ClientArrayState::array_buffer);
    if (known != ClientArrayState::unknown) return known;
    int32_t value = 0;
    glproto::av_glGetIntegerv(wire_, gl_array_buffer_binding, &value);
    buffers_.bind_buffer(ClientArrayState::array_buffer, static_cast<uint32_t>(value));
    return static_cast<uint32_t>(value);
}

uint32_t ClientVertexDraw::user_element_buffer()
{
    AV_SCOPE("ClientVertexDraw::user_element_buffer");
    const uint32_t known = buffers_.bound_buffer(ClientArrayState::element_array_buffer);
    if (known != ClientArrayState::unknown) return known;
    int32_t value = 0;
    glproto::av_glGetIntegerv(wire_, gl_element_array_buffer_binding, &value);
    buffers_.set_vertex_array_elements(state_.bound(), static_cast<uint32_t>(value));
    return static_cast<uint32_t>(value);
}

void ClientVertexDraw::host_unit(uint32_t unit)
{
    AV_SCOPE("ClientVertexDraw::host_unit");
    if (state_.host_unit() == unit) return;
    glproto::av_glClientActiveTexture(wire_, texture0 + unit);
    state_.host_selected_unit(unit);
}

void ClientVertexDraw::host_pointer(uint32_t slot, uint64_t offset)
{
    AV_SCOPE("ClientVertexDraw::host_pointer");
    const ClientVertexState::Slot &entry = state_.slot(slot);
    if (slot < ClientVertexState::generic_slots)
        return glproto::av_glVertexAttribPointer(wire_, slot, entry.size_, entry.type_, entry.normalized_,
                                                 entry.stride_, offset);
    if (slot >= ClientVertexState::texcoord_slot) {
        host_unit(slot - ClientVertexState::texcoord_slot);
        return glproto::av_glTexCoordPointer(wire_, entry.size_, entry.type_, entry.stride_, offset);
    }
    switch (slot) {
    case ClientVertexState::vertex_slot:
        return glproto::av_glVertexPointer(wire_, entry.size_, entry.type_, entry.stride_, offset);
    case ClientVertexState::normal_slot:
        return glproto::av_glNormalPointer(wire_, entry.type_, entry.stride_, offset);
    case ClientVertexState::color_slot:
        return glproto::av_glColorPointer(wire_, entry.size_, entry.type_, entry.stride_, offset);
    case ClientVertexState::secondary_color_slot:
        return glproto::av_glSecondaryColorPointer(wire_, entry.size_, entry.type_, entry.stride_, offset);
    case ClientVertexState::index_slot:
        return glproto::av_glIndexPointer(wire_, entry.type_, entry.stride_, offset);
    case ClientVertexState::edge_flag_slot: return glproto::av_glEdgeFlagPointer(wire_, entry.stride_, offset);
    default: return glproto::av_glFogCoordPointer(wire_, entry.type_, entry.stride_, offset);
    }
}

void ClientVertexDraw::host_enable(uint32_t slot, bool enabled)
{
    AV_SCOPE("ClientVertexDraw::host_enable");
    state_.set_host_enabled(slot, enabled);
    if (slot < ClientVertexState::generic_slots) {
        if (enabled) return glproto::av_glEnableVertexAttribArray(wire_, slot);
        return glproto::av_glDisableVertexAttribArray(wire_, slot);
    }
    if (slot >= ClientVertexState::texcoord_slot) host_unit(slot - ClientVertexState::texcoord_slot);
    if (enabled) return glproto::av_glEnableClientState(wire_, capability_of(slot));
    glproto::av_glDisableClientState(wire_, capability_of(slot));
}

void ClientVertexDraw::pointer(uint32_t slot, int32_t size, uint32_t type, bool normalized, int32_t stride,
                               const void *pointer)
{
    AV_SCOPE("ClientVertexDraw::pointer");
    const uint64_t address = address_of(pointer);
    if (user_array_buffer() == 0u) {
        state_.set_pointer(slot, size, type, normalized, stride, address, ClientVertexState::Mode::client);
        return;
    }
    state_.set_pointer(slot, size, type, normalized, stride, address, ClientVertexState::Mode::buffer);
    host_pointer(slot, address);
    const ClientVertexState::Slot &entry = state_.slot(slot);
    if (entry.enabled_ != 0u && entry.host_enabled_ == 0u) host_enable(slot, true);
}

void ClientVertexDraw::enable(uint32_t slot, bool enabled)
{
    AV_SCOPE("ClientVertexDraw::enable");
    state_.set_enabled(slot, enabled);
    const ClientVertexState::Slot &entry = state_.slot(slot);
    if (!enabled) {
        if (entry.host_enabled_ != 0u) host_enable(slot, false);
        return;
    }
    if (entry.host_enabled_ != 0u) return;
    const bool generic = slot < ClientVertexState::generic_slots;
    const auto mode = static_cast<ClientVertexState::Mode>(entry.mode_);
    const bool safe = generic ? mode != ClientVertexState::Mode::client : mode == ClientVertexState::Mode::buffer;
    if (safe) host_enable(slot, true);
}

uint32_t ClientVertexDraw::active_texture(uint32_t texture)
{
    AV_SCOPE("ClientVertexDraw::active_texture");
    if (!state_.select_texcoord_unit(texture)) return gl_invalid_enum;
    host_unit(state_.texcoord_unit());
    return 0u;
}

uint32_t ClientVertexDraw::interleaved(uint32_t format, int32_t stride, const void *pointer)
{
    AV_SCOPE("ClientVertexDraw::interleaved");
    if (stride < 0) return gl_invalid_value;
    const Interleaved *layout = nullptr;
    for (const Interleaved &row : interleaved_formats)
        if (row.format == format) layout = &row;
    if (layout == nullptr) return gl_invalid_enum;
    const int32_t  step = stride != 0 ? stride : layout->stride;
    const uint64_t base = address_of(pointer);
    enable(ClientVertexState::edge_flag_slot, false);
    enable(ClientVertexState::secondary_color_slot, false);
    enable(ClientVertexState::fog_coord_slot, false);
    enable(ClientVertexState::index_slot, false);
    const uint32_t unit_slot = state_.texcoord_slot_of_unit(state_.texcoord_unit());
    if (layout->texture) {
        this->pointer(unit_slot, layout->texture_size, gl_float, false, step, reinterpret_cast<const void *>(base));
        enable(unit_slot, true);
    } else {
        enable(unit_slot, false);
    }
    if (layout->color) {
        this->pointer(ClientVertexState::color_slot, layout->color_size, layout->color_type, true, step,
                      reinterpret_cast<const void *>(base + layout->color_at));
        enable(ClientVertexState::color_slot, true);
    } else {
        enable(ClientVertexState::color_slot, false);
    }
    if (layout->normal) {
        this->pointer(ClientVertexState::normal_slot, 3, gl_float, true, step,
                      reinterpret_cast<const void *>(base + layout->normal_at));
        enable(ClientVertexState::normal_slot, true);
    } else {
        enable(ClientVertexState::normal_slot, false);
    }
    this->pointer(ClientVertexState::vertex_slot, layout->vertex_size, gl_float, false, step,
                  reinterpret_cast<const void *>(base + layout->vertex_at));
    enable(ClientVertexState::vertex_slot, true);
    return 0u;
}

void ClientVertexDraw::begin_immediate()
{
    AV_SCOPE("ClientVertexDraw::begin_immediate");
    uint64_t live = state_.client_slots() & state_.host_slots();
    for (uint32_t slot = 0u; live != 0u; slot++, live >>= 1u)
        if ((live & 1u) != 0u) host_enable(slot, false);
}

uint32_t ClientVertexDraw::element_bytes(uint32_t slot) const
{
    AV_SCOPE("ClientVertexDraw::element_bytes");
    const ClientVertexState::Slot &entry = state_.slot(slot);
    if (entry.type_ == gl_int_2_10_10_10_rev || entry.type_ == gl_unsigned_int_2_10_10_10_rev) return 4u;
    const uint32_t count = entry.size_ == static_cast<int32_t>(gl_bgra) ? 4u : static_cast<uint32_t>(entry.size_);
    return count * type_bytes(entry.type_);
}

uint32_t ClientVertexDraw::stride_of(uint32_t slot) const
{
    AV_SCOPE("ClientVertexDraw::stride_of");
    const ClientVertexState::Slot &entry = state_.slot(slot);
    return entry.stride_ > 0 ? static_cast<uint32_t>(entry.stride_) : element_bytes(slot);
}

void ClientVertexDraw::fold(Range &range, int64_t lo, int64_t hi) const
{
    AV_SCOPE("ClientVertexDraw::fold");
    if (lo < 0) lo = 0;
    if (hi < lo) return;
    if (!range.any_ || lo < range.lo_) range.lo_ = lo;
    if (!range.any_ || hi > range.hi_) range.hi_ = hi;
    range.any_ = true;
}

void ClientVertexDraw::put(uint64_t at, const uint8_t *from, uint64_t bytes)
{
    AV_SCOPE("ClientVertexDraw::put");
    if (bytes == 0u) return;
    BufferPieces pieces(wire_);
    if (pieces.cuts_write(static_cast<intptr_t>(bytes), from))
        return pieces.write(ClientArrayState::array_buffer, static_cast<intptr_t>(at), static_cast<size_t>(bytes), from);
    glproto::av_glBufferSubData(wire_, ClientArrayState::array_buffer, static_cast<intptr_t>(at),
                                static_cast<intptr_t>(bytes), from);
}

uint32_t ClientVertexDraw::stage(const Range &range, uint64_t index_bytes, uint64_t &index_at)
{
    AV_SCOPE("ClientVertexDraw::stage");
    index_at = 0u;
    uint64_t live = state_.client_slots();
    uint64_t total = 0u;
    uint64_t starts[ClientVertexState::slot_count] = {};
    uint64_t sizes[ClientVertexState::slot_count] = {};
    if (range.any_) {
        const uint64_t spread = static_cast<uint64_t>(range.hi_ - range.lo_);
        uint64_t       left = live;
        for (uint32_t slot = 0u; left != 0u; slot++, left >>= 1u) {
            if ((left & 1u) == 0u) continue;
            const ClientVertexState::Slot &entry = state_.slot(slot);
            if (entry.pointer_ == 0u) continue;
            const uint64_t stride = stride_of(slot);
            const uint64_t wanted = spread * stride + element_bytes(slot);
            const uint64_t floor = static_cast<uint64_t>(range.lo_) * stride;
            if (floor > most_staged || wanted > most_staged) return gl_out_of_memory;
            const uint64_t at = aligned(total > floor ? total : floor);
            starts[slot] = at;
            sizes[slot] = wanted;
            total = at + wanted;
            if (total > most_staged) return gl_out_of_memory;
        }
    }
    uint64_t left = live;
    for (uint32_t slot = 0u; left != 0u; slot++, left >>= 1u)
        if ((left & 1u) != 0u && sizes[slot] == 0u && state_.slot(slot).host_enabled_ != 0u) host_enable(slot, false);
    index_at = aligned(total);
    if (index_bytes != 0u && index_at + index_bytes > most_staged) return gl_out_of_memory;
    const uint64_t capacity = (index_at + index_bytes + 255u) & ~uint64_t{255u};
    if (capacity == 0u) return 0u;
    user_array_ = user_array_buffer();
    if (state_.scratch() == 0u) {
        uint32_t made = 0u;
        glproto::av_glGenBuffers(wire_, 1, &made);
        if (made == 0u) return gl_out_of_memory;
        state_.set_scratch(made);
    }
    glproto::av_glBindBuffer(wire_, ClientArrayState::array_buffer, state_.scratch());
    glproto::av_glBufferData(wire_, ClientArrayState::array_buffer, static_cast<intptr_t>(capacity), nullptr,
                             gl_stream_draw);
    staged_ = true;
    for (uint32_t slot = 0u; live != 0u; slot++, live >>= 1u) {
        if ((live & 1u) == 0u || sizes[slot] == 0u) continue;
        const ClientVertexState::Slot &entry = state_.slot(slot);
        const uint64_t stride = stride_of(slot);
        const uint8_t *from = reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(entry.pointer_)) +
                              static_cast<uint64_t>(range.lo_) * stride;
        put(starts[slot], from, sizes[slot]);
        host_pointer(slot, starts[slot] - static_cast<uint64_t>(range.lo_) * stride);
        if (entry.host_enabled_ == 0u) host_enable(slot, true);
    }
    return 0u;
}

void ClientVertexDraw::commit(bool with_indices)
{
    AV_SCOPE("ClientVertexDraw::commit");
    if (!staged_) return;
    glproto::av_glBindBuffer(wire_, ClientArrayState::array_buffer, user_array_);
    if (!with_indices) return;
    glproto::av_glBindBuffer(wire_, ClientArrayState::element_array_buffer, state_.scratch());
    state_.set_indexed(true);
}

void ClientVertexDraw::finish()
{
    AV_SCOPE("ClientVertexDraw::finish");
    if (state_.indexed()) glproto::av_glBindBuffer(wire_, ClientArrayState::element_array_buffer, 0u);
    state_.set_indexed(false);
    staged_ = false;
}

uint32_t ClientVertexDraw::scan(uint32_t type, int64_t count, const void *indices, int64_t base_vertex, Range &range)
{
    AV_SCOPE("ClientVertexDraw::scan");
    if (type != gl_unsigned_byte && type != gl_unsigned_short && type != gl_unsigned_int) return gl_invalid_enum;
    const uint64_t width = type_bytes(type);
    const uint64_t bytes = static_cast<uint64_t>(count) * width;
    const uint8_t *data = reinterpret_cast<const uint8_t *>(indices);
    if (user_element_buffer() != 0u) {
        const uint64_t offset = address_of(indices);
        uint8_t *const kept = shadows_.covering(user_element_buffer(), offset, bytes);
        if (kept != nullptr) {
            data = kept;
        } else {
            uint8_t *const into = state_.staging(bytes);
            if (into == nullptr) return gl_out_of_memory;
            BufferPieces pieces(wire_);
            pieces.read(ClientArrayState::element_array_buffer, static_cast<intptr_t>(offset),
                        static_cast<size_t>(bytes), into);
            data = into;
        }
    }
    if (data == nullptr) return gl_invalid_operation;
    uint64_t lowest = ~uint64_t{0};
    uint64_t highest = 0u;
    for (int64_t at = 0; at < count; at++) {
        uint64_t value = 0u;
        __builtin_memcpy(&value, data + static_cast<uint64_t>(at) * width, width);
        if (value < lowest) lowest = value;
        if (value > highest) highest = value;
    }
    fold(range, static_cast<int64_t>(lowest) + base_vertex, static_cast<int64_t>(highest) + base_vertex);
    return 0u;
}

uint32_t ClientVertexDraw::arrays(int64_t first, int64_t count)
{
    AV_SCOPE("ClientVertexDraw::arrays");
    if (!state_.client_active() || count <= 0 || first < 0) return 0u;
    Range    range{0, 0, false};
    uint64_t unused = 0u;
    fold(range, first, first + count - 1);
    const uint32_t error = stage(range, 0u, unused);
    commit(false);
    return error;
}

uint32_t ClientVertexDraw::elements(uint32_t type, int64_t count, const void *indices, int64_t base_vertex,
                                    uint64_t &host_indices)
{
    AV_SCOPE("ClientVertexDraw::elements");
    host_indices = address_of(indices);
    if (count <= 0) return 0u;
    const bool client_indices = user_element_buffer() == 0u && indices != nullptr;
    const bool client_arrays = state_.client_active();
    if (!client_indices && !client_arrays) return 0u;
    if (type != gl_unsigned_byte && type != gl_unsigned_short && type != gl_unsigned_int) return gl_invalid_enum;
    Range range{0, 0, false};
    if (client_arrays) {
        const uint32_t scanned = scan(type, count, indices, base_vertex, range);
        if (scanned != 0u) return scanned;
    }
    const uint64_t bytes = static_cast<uint64_t>(count) * type_bytes(type);
    uint64_t       index_at = 0u;
    const uint32_t error = stage(range, client_indices ? bytes : 0u, index_at);
    if (error != 0u) {
        commit(false);
        return error;
    }
    if (client_indices) {
        if (!staged_) return gl_out_of_memory;
        put(index_at, reinterpret_cast<const uint8_t *>(indices), bytes);
        host_indices = index_at;
    }
    commit(client_indices);
    return 0u;
}

uint32_t ClientVertexDraw::multi_arrays(const int32_t *first, const int32_t *count, int32_t draws)
{
    AV_SCOPE("ClientVertexDraw::multi_arrays");
    if (!state_.client_active() || first == nullptr || count == nullptr || draws <= 0) return 0u;
    Range range{0, 0, false};
    for (int32_t at = 0; at < draws; at++)
        if (count[at] > 0 && first[at] >= 0) fold(range, first[at], static_cast<int64_t>(first[at]) + count[at] - 1);
    uint64_t       unused = 0u;
    const uint32_t error = stage(range, 0u, unused);
    commit(false);
    return error;
}

uint32_t ClientVertexDraw::multi_elements(const int32_t *count, uint32_t type, const void *const *indices,
                                          int32_t draws, const int32_t *base_vertex, const uint64_t *&host_indices)
{
    AV_SCOPE("ClientVertexDraw::multi_elements");
    host_indices = nullptr;
    if (count == nullptr || indices == nullptr || draws <= 0) return 0u;
    const bool client_indices = user_element_buffer() == 0u;
    const bool client_arrays = state_.client_active();
    if (!client_indices && !client_arrays) return 0u;
    if (type != gl_unsigned_byte && type != gl_unsigned_short && type != gl_unsigned_int) return gl_invalid_enum;
    const uint32_t width = type_bytes(type);
    Range          range{0, 0, false};
    uint64_t       index_bytes = 0u;
    for (int32_t at = 0; at < draws; at++) {
        if (count[at] <= 0) continue;
        if (client_arrays) {
            const uint32_t scanned = scan(type, count[at], indices[at], base_vertex != nullptr ? base_vertex[at] : 0,
                                          range);
            if (scanned != 0u) return scanned;
        }
        index_bytes += aligned(static_cast<uint64_t>(count[at]) * width);
    }
    uint64_t       index_at = 0u;
    const uint32_t error = stage(range, client_indices ? index_bytes : 0u, index_at);
    if (error != 0u) {
        commit(false);
        return error;
    }
    if (client_indices) {
        uint64_t *const table = state_.offset_table(static_cast<uint32_t>(draws));
        if (table == nullptr || !staged_) return gl_out_of_memory;
        uint64_t running = index_at;
        for (int32_t at = 0; at < draws; at++) {
            table[at] = running;
            if (count[at] <= 0 || indices[at] == nullptr) continue;
            const uint64_t bytes = static_cast<uint64_t>(count[at]) * width;
            put(running, reinterpret_cast<const uint8_t *>(indices[at]), bytes);
            running += aligned(bytes);
        }
        host_indices = table;
    }
    commit(client_indices);
    return 0u;
}

uint32_t ClientVertexDraw::indirect()
{
    AV_SCOPE("ClientVertexDraw::indirect");
    return state_.client_active() ? gl_invalid_operation : 0u;
}

bool ClientVertexDraw::emit(uint32_t slot, int32_t index)
{
    AV_SCOPE("ClientVertexDraw::emit");
    const ClientVertexState::Slot &entry = state_.slot(slot);
    if (entry.pointer_ == 0u) return false;
    const uint32_t wide = type_bytes(entry.type_);
    const uint8_t *at = reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(entry.pointer_)) +
                        static_cast<uint64_t>(index) * stride_of(slot);
    const bool     legacy_normalized = slot == ClientVertexState::normal_slot || slot == ClientVertexState::color_slot ||
                                       slot == ClientVertexState::secondary_color_slot;
    const bool     normalized = slot < ClientVertexState::generic_slots ? entry.normalized_ != 0u : legacy_normalized;
    const uint32_t count = entry.size_ == static_cast<int32_t>(gl_bgra) ? 4u : static_cast<uint32_t>(entry.size_);
    if (entry.type_ == gl_int_2_10_10_10_rev || entry.type_ == gl_unsigned_int_2_10_10_10_rev || count > 4u)
        return false;
    float values[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (uint32_t part = 0u; part < count; part++) values[part] = component(entry.type_, at + part * wide, normalized);
    if (entry.size_ == static_cast<int32_t>(gl_bgra)) {
        const float swap = values[0];
        values[0] = values[2];
        values[2] = swap;
    }
    if (slot < ClientVertexState::generic_slots) {
        glproto::av_glVertexAttrib4fv(wire_, slot, values);
        return true;
    }
    if (slot >= ClientVertexState::texcoord_slot) {
        glproto::av_glMultiTexCoord4fv(wire_, texture0 + (slot - ClientVertexState::texcoord_slot), values);
        return true;
    }
    switch (slot) {
    case ClientVertexState::vertex_slot: glproto::av_glVertex4fv(wire_, values); return true;
    case ClientVertexState::normal_slot: glproto::av_glNormal3fv(wire_, values); return true;
    case ClientVertexState::color_slot: glproto::av_glColor4fv(wire_, values); return true;
    case ClientVertexState::secondary_color_slot: glproto::av_glSecondaryColor3fv(wire_, values); return true;
    case ClientVertexState::index_slot: glproto::av_glIndexf(wire_, values[0]); return true;
    case ClientVertexState::edge_flag_slot: glproto::av_glEdgeFlag(wire_, values[0] != 0.0f ? 1u : 0u); return true;
    default: glproto::av_glFogCoordf(wire_, values[0]); return true;
    }
}

void ClientVertexDraw::array_element(int32_t index)
{
    AV_SCOPE("ClientVertexDraw::array_element");
    if (index < 0) return;
    const ClientVertexState::Arrays &current = state_.current();
    bool                              host_driven = false;
    for (uint32_t slot = 0u; slot < ClientVertexState::slot_count; slot++) {
        const ClientVertexState::Slot &entry = current.slots_[slot];
        if (entry.enabled_ == 0u) continue;
        const auto mode = static_cast<ClientVertexState::Mode>(entry.mode_);
        const bool generic = slot < ClientVertexState::generic_slots;
        if (generic ? mode != ClientVertexState::Mode::client : mode == ClientVertexState::Mode::buffer)
            host_driven = true;
    }
    const uint64_t client = state_.client_slots();
    const auto     emit_slot = [&](uint32_t slot) {
        if ((client & (uint64_t{1} << slot)) != 0u) static_cast<void>(emit(slot, index));
    };
    for (uint32_t slot = ClientVertexState::texcoord_slot; slot < ClientVertexState::slot_count; slot++) emit_slot(slot);
    for (uint32_t slot = ClientVertexState::normal_slot; slot < ClientVertexState::texcoord_slot; slot++) emit_slot(slot);
    for (uint32_t slot = 1u; slot < ClientVertexState::generic_slots; slot++) emit_slot(slot);
    if (host_driven) glproto::av_glArrayElement(wire_, index);
    emit_slot(ClientVertexState::vertex_slot);
    emit_slot(0u);
}

bool ClientVertexDraw::invariants_hold() const
{
    AV_SCOPE("ClientVertexDraw::invariants_hold");
    return user_array_ != ClientArrayState::unknown;
}

}

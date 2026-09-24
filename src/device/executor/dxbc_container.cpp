#include "device/executor/dxbc_container.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/dxbc_digest.hpp"

namespace av {

namespace {

/* D3D_NAME, as a container's signature writes it. */
constexpr uint32_t name_none = 0u;
constexpr uint32_t name_clip_distance = 2u;
constexpr uint32_t name_cull_distance = 3u;
constexpr uint32_t name_quad_edge = 11u;
constexpr uint32_t name_quad_inside = 12u;
constexpr uint32_t name_tri_edge = 13u;
constexpr uint32_t name_tri_inside = 14u;
constexpr uint32_t name_line_detail = 15u;
constexpr uint32_t name_line_density = 16u;
constexpr uint32_t name_target = 64u;
constexpr uint32_t name_depth = 65u;

/* D3D10_SB_NAME, as the DDI hands it, where it differs from D3D_NAME: the
 * token format names each tessellation factor separately. */
constexpr uint32_t sb_quad_edge_first = 11u;
constexpr uint32_t sb_quad_inside_first = 15u;
constexpr uint32_t sb_tri_edge_first = 17u;
constexpr uint32_t sb_tri_inside = 20u;
constexpr uint32_t sb_line_detail = 21u;
constexpr uint32_t sb_line_density = 22u;
constexpr uint32_t sb_last = 25u;

constexpr uint32_t no_register = 0xffffffffu;
constexpr uint32_t entry_bytes = 32u;
constexpr uint32_t full_mask = 0xfu;
constexpr uint32_t dxbc_version = 1u;

const char *const system_names[] = {"RVK",
                                    "SV_Position",
                                    "SV_ClipDistance",
                                    "SV_CullDistance",
                                    "SV_RenderTargetArrayIndex",
                                    "SV_ViewportArrayIndex",
                                    "SV_VertexID",
                                    "SV_PrimitiveID",
                                    "SV_InstanceID",
                                    "SV_IsFrontFace",
                                    "SV_SampleIndex"};

}  // namespace

DxbcContainer::DxbcContainer(uint8_t *buffer, size_t capacity)
    : buffer_(buffer), capacity_(buffer == nullptr ? 0u : capacity), used_(0u), chunks_(0u), chunk_offsets_{},
      overflowed_(false)
{
    AV_SCOPE("DxbcContainer::DxbcContainer");
    if (!reserve(header_bytes)) return;
    for (size_t index = 0u; index < header_bytes; index++) buffer_[index] = 0u;
    used_ = header_bytes;
}

bool DxbcContainer::reserve(size_t bytes)
{
    AV_SCOPE("DxbcContainer::reserve");
    if (overflowed_ || bytes > capacity_ - used_) {
        overflowed_ = true;
        return false;
    }
    return true;
}

void DxbcContainer::put_u32(uint32_t value)
{
    AV_SCOPE("DxbcContainer::put_u32");
    if (!reserve(4u)) return;
    for (uint32_t byte = 0u; byte < 4u; byte++) buffer_[used_ + byte] = static_cast<uint8_t>(value >> (8u * byte));
    used_ += 4u;
}

void DxbcContainer::put_bytes(const void *data, size_t size)
{
    AV_SCOPE("DxbcContainer::put_bytes");
    if (!reserve(size)) return;
    __builtin_memcpy(buffer_ + used_, data, size);
    used_ += size;
}

void DxbcContainer::patch_u32(size_t at, uint32_t value)
{
    AV_SCOPE("DxbcContainer::patch_u32");
    for (uint32_t byte = 0u; byte < 4u; byte++) buffer_[at + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

bool DxbcContainer::begin_chunk(const char *fourcc)
{
    AV_SCOPE("DxbcContainer::begin_chunk");
    if (chunks_ == max_chunks || !reserve(8u)) {
        overflowed_ = true;
        return false;
    }
    chunk_offsets_[chunks_++] = used_;
    put_bytes(fourcc, 4u);
    put_u32(0u);
    return true;
}

void DxbcContainer::end_chunk()
{
    AV_SCOPE("DxbcContainer::end_chunk");
    /* Chunks are four-byte aligned, and a string table rarely ends on one. */
    while (!overflowed_ && used_ % 4u != 0u) {
        if (!reserve(1u)) return;
        buffer_[used_++] = 0u;
    }
    if (overflowed_) return;
    const size_t start = chunk_offsets_[chunks_ - 1u];
    patch_u32(start + 4u, static_cast<uint32_t>(used_ - start - 8u));
}

const char *DxbcContainer::semantic_of(const d3dproto::SignatureEntry &entry, bool pixel_output,
                                       uint32_t &index) const
{
    AV_SCOPE("DxbcContainer::semantic_of");
    const uint32_t sv = entry.system_value;
    const uint32_t mask = entry.mask & full_mask;
    index = entry.register_index * 4u + (mask == 0u ? 0u : static_cast<uint32_t>(__builtin_ctz(mask)));
    if (pixel_output && sv == 0u) {
        index = entry.register_index;
        return entry.register_index == no_register ? "SV_Depth" : "SV_Target";
    }
    if (sv >= sb_quad_edge_first && sv < sb_quad_inside_first) index = sv - sb_quad_edge_first;
    if (sv >= sb_quad_inside_first && sv < sb_tri_edge_first) index = sv - sb_quad_inside_first;
    if (sv >= sb_tri_edge_first && sv < sb_tri_inside) index = sv - sb_tri_edge_first;
    if (sv == sb_tri_inside || sv == sb_line_density) index = 0u;
    if (sv == sb_line_detail) index = 1u;
    if (sv >= sb_quad_edge_first && sv <= sb_line_density)
        return sv == sb_tri_inside || (sv >= sb_quad_inside_first && sv < sb_tri_edge_first) ? "SV_InsideTessFactor"
                                                                                              : "SV_TessFactor";
    if (sv == name_clip_distance || sv == name_cull_distance) index = 0u;
    if (sv < sizeof(system_names) / sizeof(system_names[0])) return system_names[sv];
    return "RVK";
}

bool DxbcContainer::add_signature(Role role, const d3dproto::SignatureEntry *entries, uint32_t count,
                                  bool pixel_output)
{
    AV_SCOPE("DxbcContainer::add_signature");
    const char *const tag = role == Role::input ? "ISG1" : role == Role::output ? "OSG1" : "PSG1";
    if (!begin_chunk(tag)) return false;
    const size_t data = used_;
    put_u32(count);
    put_u32(8u);
    const size_t table = used_;
    if (!reserve(static_cast<size_t>(count) * entry_bytes)) return false;
    used_ += static_cast<size_t>(count) * entry_bytes;
    uint32_t distances[2] = {0u, 0u};
    for (uint32_t element = 0u; element < count; element++) {
        const d3dproto::SignatureEntry &entry = entries[element];
        uint32_t index = 0u;
        const char *const name = semantic_of(entry, pixel_output, index);
        uint32_t sv = entry.system_value > sb_last ? name_none : entry.system_value;
        if (sv == name_clip_distance || sv == name_cull_distance) index = distances[sv - name_clip_distance]++;
        if (sv >= sb_quad_edge_first && sv < sb_quad_inside_first) sv = name_quad_edge;
        else if (sv >= sb_quad_inside_first && sv < sb_tri_edge_first) sv = name_quad_inside;
        else if (sv >= sb_tri_edge_first && sv < sb_tri_inside) sv = name_tri_edge;
        else if (sv == sb_tri_inside) sv = name_tri_inside;
        else if (sv == sb_line_detail) sv = name_line_detail;
        else if (sv == sb_line_density) sv = name_line_density;
        if (pixel_output && entry.system_value == 0u)
            sv = entry.register_index == no_register ? name_depth : name_target;
        const uint32_t name_offset = static_cast<uint32_t>(used_ - data);
        size_t length = 0u;
        while (name[length] != '\0') length++;
        put_bytes(name, length + 1u);
        if (overflowed_) return false;
        const size_t at = table + static_cast<size_t>(element) * entry_bytes;
        const uint32_t mask = entry.mask & full_mask;
        /* An input's second mask byte is the components the shader reads,
         * which DXVK trims a declaration to; the DDI says only which exist. An
         * output's is the components it never writes: none, as far as this
         * side knows. */
        const uint32_t masks = role == Role::input ? (mask | mask << 8u) : mask;
        const uint32_t fields[8] = {entry.stream,           name_offset, index, sv, entry.component_type,
                                    entry.register_index, masks,       entry.min_precision};
        for (uint32_t field = 0u; field < 8u; field++) patch_u32(at + field * 4u, fields[field]);
    }
    end_chunk();
    return !overflowed_;
}

bool DxbcContainer::add_layout_signature(const uint32_t *registers, uint32_t count)
{
    AV_SCOPE("DxbcContainer::add_layout_signature");
    d3dproto::SignatureEntry entries[32] = {};
    if (count > sizeof(entries) / sizeof(entries[0])) return false;
    for (uint32_t element = 0u; element < count; element++) {
        entries[element].register_index = registers[element];
        entries[element].mask = full_mask;
        /* D3D_REGISTER_COMPONENT_FLOAT32: the layout's format says what the
         * fetch converts to, and a signature is only consulted for the
         * register here. */
        entries[element].component_type = 3u;
    }
    return add_signature(Role::input, entries, count, false);
}

bool DxbcContainer::add_code(const uint8_t *tokens, size_t size)
{
    AV_SCOPE("DxbcContainer::add_code");
    if (tokens == nullptr || size < 8u || size % 4u != 0u) return false;
    const uint32_t length = static_cast<uint32_t>(tokens[4]) | static_cast<uint32_t>(tokens[5]) << 8u |
                            static_cast<uint32_t>(tokens[6]) << 16u | static_cast<uint32_t>(tokens[7]) << 24u;
    if (static_cast<size_t>(length) * 4u != size) return false;
    if (!begin_chunk("SHEX")) return false;
    put_bytes(tokens, size);
    end_chunk();
    return !overflowed_;
}

bool DxbcContainer::add_dxil(const uint8_t *program, size_t size)
{
    AV_SCOPE("DxbcContainer::add_dxil");
    /* DxilProgramHeader: version, size in dwords, then DxilBitcodeHeader,
     * whose first word is 'DXIL'. */
    constexpr size_t   magic_at = 8u;
    constexpr uint32_t dxil_magic = 0x4c495844u;
    if (program == nullptr || size < magic_at + 4u || size % 4u != 0u) return false;
    const uint32_t dwords = read_u32(program + 4u);
    if (static_cast<size_t>(dwords) * 4u != size || read_u32(program + magic_at) != dxil_magic) return false;
    if (!begin_chunk("DXIL")) return false;
    put_bytes(program, size);
    end_chunk();
    return !overflowed_;
}

uint32_t DxbcContainer::read_u32(const uint8_t *at) const
{
    AV_SCOPE("DxbcContainer::read_u32");
    return static_cast<uint32_t>(at[0]) | static_cast<uint32_t>(at[1]) << 8u | static_cast<uint32_t>(at[2]) << 16u |
           static_cast<uint32_t>(at[3]) << 24u;
}

size_t DxbcContainer::finish()
{
    AV_SCOPE("DxbcContainer::finish");
    if (overflowed_ || buffer_ == nullptr) return 0u;
    const uint8_t magic[4] = {'D', 'X', 'B', 'C'};
    __builtin_memcpy(buffer_, magic, sizeof(magic));
    patch_u32(20u, dxbc_version);
    patch_u32(24u, static_cast<uint32_t>(used_));
    patch_u32(28u, chunks_);
    for (uint32_t chunk = 0u; chunk < chunks_; chunk++)
        patch_u32(32u + chunk * 4u, static_cast<uint32_t>(chunk_offsets_[chunk]));
    DxbcDigest digest;
    digest.compute(buffer_, used_, buffer_ + 4u);
    return used_;
}

}  // namespace av

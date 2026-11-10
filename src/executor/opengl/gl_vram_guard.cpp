#include "executor/opengl/gl_vram_guard.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opengl/gl_error_queue.hpp"
#include "executor/opengl/gl_texture_shape.hpp"
#include "executor/byte_ledger.hpp"

namespace av {

namespace {

constexpr uint32_t gl_out_of_memory = 0x0505u;
constexpr uint32_t gl_texture_2d = 0x0DE1u;
constexpr uint32_t gl_cube_face_first = 0x8515u, gl_cube_face_last = 0x851Au;
constexpr uint32_t gl_renderbuffer_binding = 0x8CA7u;
constexpr uint32_t whole_storage = 0xFFu;
constexpr uint32_t gl_texture_sparse = 0x91A6u;
constexpr int64_t  largest_region = int64_t{1} << 31u;
constexpr uint64_t unknown_texel_bytes = 4u;
constexpr uint64_t most_names = 1u << 20u;
constexpr uint32_t cube_binding = 0x8514u;
constexpr uint32_t pixel_pack_target = 0x88EBu, pixel_unpack_target = 0x88ECu;
constexpr uint64_t read_usages[] = {0x88E1u, 0x88E5u, 0x88E9u};
constexpr uint64_t host_storage_flags = 0x1u | 0x2u | 0x40u | 0x80u | 0x200u;

struct BufferRow {
    uint32_t target_;
    uint32_t binding_;
};

constexpr BufferRow buffer_targets[] = {
    {0x8892u, 0x8894u}, {0x8893u, 0x8895u}, {0x88EBu, 0x88EDu}, {0x88ECu, 0x88EFu}, {0x8A11u, 0x8A28u},
    {0x8C8Eu, 0x8C8Fu}, {0x8F36u, 0x8F36u}, {0x8F37u, 0x8F37u}, {0x8C2Au, 0x8C2Du}, {0x8F3Fu, 0x8F43u},
    {0x92C0u, 0x92C1u}, {0x90D2u, 0x90D3u}, {0x90EEu, 0x90EFu}, {0x9192u, 0x9193u}, {0x80EEu, 0x80EFu},
};

uint64_t key_of(uint64_t kind, uint32_t name, uint32_t slot)
{
    AV_SCOPE("key_of");
    return (kind << 56u) | (static_cast<uint64_t>(name) << 8u) | slot;
}

uint64_t saturating_product(uint64_t first, uint64_t second)
{
    AV_SCOPE("saturating_product");
    uint64_t product = 0u;
    return __builtin_mul_overflow(first, second, &product) ? UINT64_MAX : product;
}

bool hook_vram(void *user, uint32_t command, const uint64_t *args)
{
    AV_SCOPE("hook_vram");
    return static_cast<GlVramGuard *>(user)->admit(command, args);
}

bool resident_usage(uint32_t target, uint64_t usage)
{
    AV_SCOPE("resident_usage");
    if (target == pixel_pack_target || target == pixel_unpack_target) return true;
    for (const uint64_t read : read_usages)
        if (usage == read) return true;
    return false;
}

bool resident_storage(uint32_t target, uint64_t flags)
{
    AV_SCOPE("resident_storage");
    return target == pixel_pack_target || target == pixel_unpack_target || (flags & host_storage_flags) != 0u;
}

uint32_t u32(uint64_t value)
{
    AV_SCOPE("u32");
    return static_cast<uint32_t>(value);
}

}

GlVramGuard::GlVramGuard(BlockSource &blocks, glproto::RealGl &gl, GlErrorQueue &errors)
    : gl_(gl), errors_(errors), account_(), host_(), sizes_(blocks, Pool::paged), sparse_(blocks, Pool::paged)
{
    AV_SCOPE("GlVramGuard::GlVramGuard");
}

void GlVramGuard::bind(ByteLedger *vram, ByteLedger *host)
{
    AV_SCOPE("GlVramGuard::bind");
    sizes_.clear();
    sparse_.clear();
    account_.bind(vram);
    host_.bind(host);
}

void GlVramGuard::install()
{
    AV_SCOPE("GlVramGuard::install");
    gl_.av_vram_user = this;
    gl_.av_vram_hook = hook_vram;
}

void GlVramGuard::clear()
{
    AV_SCOPE("GlVramGuard::clear");
    sizes_.clear();
    sparse_.clear();
    static_cast<void>(account_.close());
    static_cast<void>(host_.close());
}

bool GlVramGuard::refuse()
{
    AV_SCOPE("GlVramGuard::refuse");
    errors_.record(gl_out_of_memory);
    return false;
}

uint32_t GlVramGuard::bound(uint32_t binding)
{
    AV_SCOPE("GlVramGuard::bound");
    if (gl_.glGetIntegerv == nullptr) return 0u;
    GLint name = 0;
    gl_.glGetIntegerv(binding, &name);
    return name > 0 ? static_cast<uint32_t>(name) : 0u;
}

uint64_t GlVramGuard::row_bytes(Kind kind, uint32_t name, uint32_t slot)
{
    AV_SCOPE("GlVramGuard::row_bytes");
    const uint64_t *const row = sizes_.find(key_of(static_cast<uint64_t>(kind), name, slot));
    return row != nullptr ? *row : 0u;
}

bool GlVramGuard::account(VramAccount &against, Kind kind, uint32_t name, uint32_t slot, uint64_t bytes)
{
    AV_SCOPE("GlVramGuard::account");
    const uint64_t key = key_of(static_cast<uint64_t>(kind), name, slot);
    uint64_t *const row = sizes_.find(key);
    const uint64_t before = row != nullptr ? *row : 0u;
    if (row == nullptr && bytes == 0u) return true;
    if (bytes > before && !against.take(bytes - before)) return false;
    if (bytes < before) against.give(before - bytes);
    if (row != nullptr) {
        *row = bytes;
        return true;
    }
    uint32_t position = 0u;
    if (sizes_.insert(key, bytes, position) == Status::ok) return true;
    against.give(bytes);
    return false;
}

bool GlVramGuard::buffer_bytes(uint32_t name, uint64_t size, bool resident)
{
    AV_SCOPE("GlVramGuard::buffer_bytes");
    const uint64_t before = row_bytes(Kind::host_buffer, name, 0u);
    if (!account(host_, Kind::host_buffer, name, 0u, resident ? size : 0u)) return false;
    if (account(account_, Kind::buffer, name, 0u, size)) return true;
    static_cast<void>(account(host_, Kind::host_buffer, name, 0u, before));
    return false;
}

bool GlVramGuard::buffer_named(uint32_t name, uint64_t size, bool resident)
{
    AV_SCOPE("GlVramGuard::buffer_named");
    if (name == 0u || static_cast<int64_t>(size) < 0) return true;
    return buffer_bytes(name, size, resident) || refuse();
}

bool GlVramGuard::buffer_target(uint32_t target, uint64_t size, bool resident)
{
    AV_SCOPE("GlVramGuard::buffer_target");
    for (const BufferRow &row : buffer_targets)
        if (row.target_ == target) return buffer_named(bound(row.binding_), size, resident);
    return true;
}

bool GlVramGuard::image(uint32_t target, uint32_t level, uint32_t format, uint64_t width, uint64_t height,
                        uint64_t depth, uint64_t samples)
{
    AV_SCOPE("GlVramGuard::image");
    const bool face = target >= gl_cube_face_first && target <= gl_cube_face_last;
    const uint32_t shaped = face ? gl_texture_2d : target;
    const GlTextureShape shape(shaped, format, 1, static_cast<int64_t>(samples), static_cast<int64_t>(width),
                               static_cast<int64_t>(height), static_cast<int64_t>(depth));
    if (!shape.valid()) return true;
    const uint32_t name = bound(face ? cube_binding : shape.binding());
    if (name == 0u || level >= GlTextureShape::most_levels) return true;
    uint64_t bytes = shape.least_bytes();
    if (bytes == UINT64_MAX)
        bytes = saturating_product(saturating_product(saturating_product(width, height), saturating_product(depth, samples)),
                                   unknown_texel_bytes);
    return account(account_, Kind::texture, name, face ? level + 32u * (target - gl_cube_face_first + 1u) : level, bytes) ||
           refuse();
}

bool GlVramGuard::storage(uint32_t texture, uint32_t target, uint64_t levels, uint32_t format, uint64_t width,
                          uint64_t height, uint64_t depth, uint64_t samples)
{
    AV_SCOPE("GlVramGuard::storage");
    const GlTextureShape shape(target, format, static_cast<int64_t>(levels), static_cast<int64_t>(samples),
                               static_cast<int64_t>(width), static_cast<int64_t>(height), static_cast<int64_t>(depth));
    if (!shape.valid()) return true;
    const uint32_t name = texture != 0u ? texture : bound(shape.binding());
    if (name == 0u) return true;
    if (sparse(name)) {
        const GlTextureShape texel(target, format, 1, 1, 1, 1, 1);
        uint64_t             texel_bytes = texel.valid() ? texel.least_bytes() : UINT64_MAX;
        if (texel_bytes == UINT64_MAX || texel_bytes == 0u) texel_bytes = unknown_texel_bytes;
        uint64_t *const row = sparse_.find(name);
        if (row != nullptr) *row = texel_bytes;
        return account(account_, Kind::texture, name, whole_storage, 0u) || refuse();
    }
    uint64_t bytes = shape.least_bytes();
    if (bytes == UINT64_MAX)
        bytes = saturating_product(saturating_product(saturating_product(width, height), saturating_product(depth, samples)),
                                   unknown_texel_bytes);
    return account(account_, Kind::texture, name, whole_storage, bytes) || refuse();
}

bool GlVramGuard::compressed(uint32_t target, uint32_t level, int64_t bytes)
{
    AV_SCOPE("GlVramGuard::compressed");
    const GlTextureShape shape(target >= gl_cube_face_first && target <= gl_cube_face_last ? gl_texture_2d : target,
                               0u, 1, 1, 1, 1, 1);
    if (bytes <= 0 || !shape.tracked() || level >= GlTextureShape::most_levels) return true;
    const uint32_t name = bound(shape.binding());
    if (name == 0u) return true;
    return account(account_, Kind::texture, name, level, static_cast<uint64_t>(bytes)) || refuse();
}

bool GlVramGuard::renderbuffer(uint32_t name, uint32_t format, uint64_t width, uint64_t height, uint64_t samples)
{
    AV_SCOPE("GlVramGuard::renderbuffer");
    const uint32_t target = name != 0u ? name : bound(gl_renderbuffer_binding);
    if (target == 0u) return true;
    const GlTextureShape shape(gl_texture_2d, format, 1, static_cast<int64_t>(samples != 0u ? samples : 1u),
                               static_cast<int64_t>(width), static_cast<int64_t>(height), 1);
    if (!shape.valid()) return true;
    uint64_t bytes = shape.least_bytes();
    if (bytes == UINT64_MAX)
        bytes = saturating_product(saturating_product(width, height), saturating_product(samples != 0u ? samples : 1u, unknown_texel_bytes));
    return account(account_, Kind::renderbuffer, target, 0u, bytes) || refuse();
}

bool GlVramGuard::sparse(uint32_t name) const
{
    AV_SCOPE("GlVramGuard::sparse");
    return sparse_.find(name) != nullptr;
}

void GlVramGuard::sparse_parameter(uint32_t texture, uint32_t target, uint32_t pname, int64_t value)
{
    AV_SCOPE("GlVramGuard::sparse_parameter");
    if (pname != gl_texture_sparse) return;
    const GlTextureShape shape(target, 0u, 1, 1, 1, 1, 1);
    const uint32_t       name = texture != 0u ? texture : (shape.tracked() ? bound(shape.binding()) : 0u);
    if (name == 0u) return;
    if (value == 0) {
        uint64_t unused = 0u;
        static_cast<void>(sparse_.take(name, unused));
        return;
    }
    if (sparse_.find(name) != nullptr) return;
    uint32_t position = 0u;
    static_cast<void>(sparse_.insert(name, unknown_texel_bytes, position));
}

bool GlVramGuard::commitment(uint32_t texture, uint32_t target, uint32_t level, uint64_t width, uint64_t height,
                             uint64_t depth, bool commit)
{
    AV_SCOPE("GlVramGuard::commitment");
    const GlTextureShape shape(target, 0u, 1, 1, 1, 1, 1);
    const uint32_t       name = texture != 0u ? texture : (shape.tracked() ? bound(shape.binding()) : 0u);
    if (name == 0u || level >= GlTextureShape::most_levels) return true;
    const uint64_t *const texel = sparse_.find(name);
    if (texel == nullptr) return true;
    if (static_cast<int64_t>(width) < 0 || static_cast<int64_t>(height) < 0 || static_cast<int64_t>(depth) < 0 ||
        static_cast<int64_t>(width) >= largest_region || static_cast<int64_t>(height) >= largest_region ||
        static_cast<int64_t>(depth) >= largest_region)
        return true;
    const uint64_t region = saturating_product(saturating_product(width, height), saturating_product(depth, *texel));
    const uint64_t before = row_bytes(Kind::commitment, name, level);
    uint64_t       after = 0u;
    if (commit) after = region > UINT64_MAX - before ? UINT64_MAX : before + region;
    else after = before > region ? before - region : 0u;
    return account(account_, Kind::commitment, name, level, after) || refuse();
}

void GlVramGuard::forget(Kind kind, uint64_t count, const uint32_t *names)
{
    AV_SCOPE("GlVramGuard::forget");
    if (names == nullptr) return;
    const uint32_t slots = kind == Kind::texture ? whole_storage + 1u : 1u;
    for (uint64_t at = 0u; at < count && at < most_names; at++) {
        for (uint32_t slot = 0u; slot < slots; slot++) {
            uint64_t freed = 0u;
            if (sizes_.take(key_of(static_cast<uint64_t>(kind), names[at], slot), freed)) account_.give(freed);
        }
        if (kind == Kind::texture) {
            uint64_t unused = 0u;
            static_cast<void>(sparse_.take(names[at], unused));
            for (uint32_t level = 0u; level < GlTextureShape::most_levels; level++) {
                uint64_t freed = 0u;
                if (sizes_.take(key_of(static_cast<uint64_t>(Kind::commitment), names[at], level), freed))
                    account_.give(freed);
            }
        }
        uint64_t resident = 0u;
        if (kind == Kind::buffer && sizes_.take(key_of(static_cast<uint64_t>(Kind::host_buffer), names[at], 0u), resident))
            host_.give(resident);
    }
}

bool GlVramGuard::admit(uint32_t command, const uint64_t *a)
{
    AV_SCOPE("GlVramGuard::admit");
    if (!account_.bound()) return true;
    switch (command) {
    case glproto::kVram_glBufferData: return buffer_target(u32(a[0]), a[1], resident_usage(u32(a[0]), a[3]));
    case glproto::kVram_glBufferStorage: return buffer_target(u32(a[0]), a[1], resident_storage(u32(a[0]), a[3]));
    case glproto::kVram_glNamedBufferData:
    case glproto::kVram_glNamedBufferDataEXT: return buffer_named(u32(a[0]), a[1], resident_usage(0u, a[3]));
    case glproto::kVram_glNamedBufferStorage:
    case glproto::kVram_glNamedBufferStorageEXT: return buffer_named(u32(a[0]), a[1], resident_storage(0u, a[3]));
    case glproto::kVram_glTexImage1D: return image(u32(a[0]), u32(a[1]), u32(a[2]), a[3], 1u, 1u, 1u);
    case glproto::kVram_glTexImage2D: return image(u32(a[0]), u32(a[1]), u32(a[2]), a[3], a[4], 1u, 1u);
    case glproto::kVram_glTexImage3D: return image(u32(a[0]), u32(a[1]), u32(a[2]), a[3], a[4], a[5], 1u);
    case glproto::kVram_glTexImage2DMultisample: return image(u32(a[0]), 0u, u32(a[2]), a[3], a[4], 1u, a[1]);
    case glproto::kVram_glTexImage3DMultisample: return image(u32(a[0]), 0u, u32(a[2]), a[3], a[4], a[5], a[1]);
    case glproto::kVram_glCompressedTexImage1D: return compressed(u32(a[0]), u32(a[1]), static_cast<int64_t>(a[5]));
    case glproto::kVram_glCompressedTexImage2D: return compressed(u32(a[0]), u32(a[1]), static_cast<int64_t>(a[6]));
    case glproto::kVram_glCompressedTexImage3D: return compressed(u32(a[0]), u32(a[1]), static_cast<int64_t>(a[7]));
    case glproto::kVram_glTexStorage1D: return storage(0u, u32(a[0]), a[1], u32(a[2]), a[3], 1u, 1u, 1u);
    case glproto::kVram_glTexStorage2D: return storage(0u, u32(a[0]), a[1], u32(a[2]), a[3], a[4], 1u, 1u);
    case glproto::kVram_glTexStorage3D: return storage(0u, u32(a[0]), a[1], u32(a[2]), a[3], a[4], a[5], 1u);
    case glproto::kVram_glTexStorage2DMultisample: return storage(0u, u32(a[0]), 1u, u32(a[2]), a[3], a[4], 1u, a[1]);
    case glproto::kVram_glTexStorage3DMultisample: return storage(0u, u32(a[0]), 1u, u32(a[2]), a[3], a[4], a[5], a[1]);
    case glproto::kVram_glTextureStorage1D: return storage(u32(a[0]), gl_texture_2d, a[1], u32(a[2]), a[3], 1u, 1u, 1u);
    case glproto::kVram_glTextureStorage2D: return storage(u32(a[0]), gl_texture_2d, a[1], u32(a[2]), a[3], a[4], 1u, 1u);
    case glproto::kVram_glTextureStorage3D: return storage(u32(a[0]), 0x806Fu, a[1], u32(a[2]), a[3], a[4], a[5], 1u);
    case glproto::kVram_glTextureStorage2DMultisample:
        return storage(u32(a[0]), 0x9100u, 1u, u32(a[2]), a[3], a[4], 1u, a[1]);
    case glproto::kVram_glTextureStorage3DMultisample:
        return storage(u32(a[0]), 0x9102u, 1u, u32(a[2]), a[3], a[4], a[5], a[1]);
    case glproto::kVram_glRenderbufferStorage: return renderbuffer(0u, u32(a[1]), a[2], a[3], 1u);
    case glproto::kVram_glRenderbufferStorageMultisample: return renderbuffer(0u, u32(a[2]), a[3], a[4], a[1]);
    case glproto::kVram_glNamedRenderbufferStorage: return renderbuffer(u32(a[0]), u32(a[1]), a[2], a[3], 1u);
    case glproto::kVram_glNamedRenderbufferStorageMultisample:
        return renderbuffer(u32(a[0]), u32(a[2]), a[3], a[4], a[1]);
    case glproto::kVram_glTexParameteri:
        sparse_parameter(0u, u32(a[0]), u32(a[1]), static_cast<int32_t>(a[2]));
        return true;
    case glproto::kVram_glTextureParameteri:
        sparse_parameter(u32(a[0]), 0u, u32(a[1]), static_cast<int32_t>(a[2]));
        return true;
    case glproto::kVram_glTexParameteriv:
    case glproto::kVram_glTextureParameteriv:
        if (a[2] != 0u) {
            const bool    direct = command == glproto::kVram_glTexParameteriv;
            const int32_t value = *reinterpret_cast<const int32_t *>(static_cast<uintptr_t>(a[2]));
            sparse_parameter(direct ? 0u : u32(a[0]), direct ? u32(a[0]) : 0u, u32(a[1]), value);
        }
        return true;
    case glproto::kVram_glTexPageCommitmentARB:
        return commitment(0u, u32(a[0]), u32(a[1]), a[5], a[6], a[7], a[8] != 0u);
    case glproto::kVram_glTexturePageCommitmentEXT:
        return commitment(u32(a[0]), 0x0DE1u, u32(a[1]), a[5], a[6], a[7], a[8] != 0u);
    case glproto::kVram_glDeleteBuffers:
        forget(Kind::buffer, a[0], reinterpret_cast<const uint32_t *>(static_cast<uintptr_t>(a[1])));
        return true;
    case glproto::kVram_glDeleteTextures:
        forget(Kind::texture, a[0], reinterpret_cast<const uint32_t *>(static_cast<uintptr_t>(a[1])));
        return true;
    case glproto::kVram_glDeleteRenderbuffers:
        forget(Kind::renderbuffer, a[0], reinterpret_cast<const uint32_t *>(static_cast<uintptr_t>(a[1])));
        return true;
    default: return true;
    }
}

uint64_t GlVramGuard::held() const
{
    AV_SCOPE("GlVramGuard::held");
    return account_.held();
}

uint64_t GlVramGuard::host_held() const
{
    AV_SCOPE("GlVramGuard::host_held");
    return host_.held();
}

uint32_t GlVramGuard::objects() const
{
    AV_SCOPE("GlVramGuard::objects");
    return sizes_.count();
}

bool GlVramGuard::invariants_hold() const
{
    AV_SCOPE("GlVramGuard::invariants_hold");
    uint64_t total = 0u;
    uint64_t resident = 0u;
    sizes_.for_each([&total, &resident](uint64_t key, const uint64_t &bytes) {
        if ((key >> 56u) == static_cast<uint64_t>(Kind::host_buffer)) resident += bytes;
        else total += bytes;
    });
    return total == account_.held() && resident == host_.held() && sizes_.invariants_hold();
}

}

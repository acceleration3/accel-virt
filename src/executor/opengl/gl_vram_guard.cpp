#include "executor/opengl/gl_vram_guard.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opengl/gl_error_queue.hpp"
#include "executor/opengl/gl_texture_shape.hpp"
#include "executor/vram_ledger.hpp"

namespace av {

namespace {

constexpr uint32_t gl_out_of_memory = 0x0505u;
constexpr uint32_t gl_texture_2d = 0x0DE1u;
constexpr uint32_t gl_cube_face_first = 0x8515u, gl_cube_face_last = 0x851Au;
constexpr uint32_t gl_renderbuffer_binding = 0x8CA7u;
constexpr uint32_t whole_storage = 0xFFu;
constexpr uint64_t unknown_texel_bytes = 4u;
constexpr uint64_t most_names = 1u << 20u;
constexpr uint32_t cube_binding = 0x8514u;

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

uint32_t u32(uint64_t value)
{
    AV_SCOPE("u32");
    return static_cast<uint32_t>(value);
}

}

GlVramGuard::GlVramGuard(BlockSource &blocks, glproto::RealGl &gl, GlErrorQueue &errors)
    : gl_(gl), errors_(errors), account_(), sizes_(blocks, Pool::paged)
{
    AV_SCOPE("GlVramGuard::GlVramGuard");
}

void GlVramGuard::bind(VramLedger *ledger)
{
    AV_SCOPE("GlVramGuard::bind");
    sizes_.clear();
    account_.bind(ledger);
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
    static_cast<void>(account_.close());
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

bool GlVramGuard::account(Kind kind, uint32_t name, uint32_t slot, uint64_t bytes)
{
    AV_SCOPE("GlVramGuard::account");
    const uint64_t key = key_of(static_cast<uint64_t>(kind), name, slot);
    uint64_t *const row = sizes_.find(key);
    const uint64_t before = row != nullptr ? *row : 0u;
    if (bytes > before && !account_.take(bytes - before)) return false;
    if (bytes < before) account_.give(before - bytes);
    if (row != nullptr) {
        *row = bytes;
        return true;
    }
    uint32_t position = 0u;
    if (sizes_.insert(key, bytes, position) == Status::ok) return true;
    account_.give(bytes);
    return false;
}

bool GlVramGuard::buffer_named(uint32_t name, uint64_t size)
{
    AV_SCOPE("GlVramGuard::buffer_named");
    if (name == 0u || static_cast<int64_t>(size) < 0) return true;
    return account(Kind::buffer, name, 0u, size) || refuse();
}

bool GlVramGuard::buffer_target(uint32_t target, uint64_t size)
{
    AV_SCOPE("GlVramGuard::buffer_target");
    for (const BufferRow &row : buffer_targets)
        if (row.target_ == target) return buffer_named(bound(row.binding_), size);
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
    return account(Kind::texture, name, face ? level + 32u * (target - gl_cube_face_first + 1u) : level, bytes) ||
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
    uint64_t bytes = shape.least_bytes();
    if (bytes == UINT64_MAX)
        bytes = saturating_product(saturating_product(saturating_product(width, height), saturating_product(depth, samples)),
                                   unknown_texel_bytes);
    return account(Kind::texture, name, whole_storage, bytes) || refuse();
}

bool GlVramGuard::compressed(uint32_t target, uint32_t level, int64_t bytes)
{
    AV_SCOPE("GlVramGuard::compressed");
    const GlTextureShape shape(target >= gl_cube_face_first && target <= gl_cube_face_last ? gl_texture_2d : target,
                               0u, 1, 1, 1, 1, 1);
    if (bytes <= 0 || !shape.tracked() || level >= GlTextureShape::most_levels) return true;
    const uint32_t name = bound(shape.binding());
    if (name == 0u) return true;
    return account(Kind::texture, name, level, static_cast<uint64_t>(bytes)) || refuse();
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
    return account(Kind::renderbuffer, target, 0u, bytes) || refuse();
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
    }
}

bool GlVramGuard::admit(uint32_t command, const uint64_t *a)
{
    AV_SCOPE("GlVramGuard::admit");
    if (!account_.bound()) return true;
    switch (command) {
    case glproto::kVram_glBufferData:
    case glproto::kVram_glBufferStorage: return buffer_target(u32(a[0]), a[1]);
    case glproto::kVram_glNamedBufferData:
    case glproto::kVram_glNamedBufferDataEXT:
    case glproto::kVram_glNamedBufferStorage:
    case glproto::kVram_glNamedBufferStorageEXT: return buffer_named(u32(a[0]), a[1]);
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

uint32_t GlVramGuard::objects() const
{
    AV_SCOPE("GlVramGuard::objects");
    return sizes_.count();
}

bool GlVramGuard::invariants_hold() const
{
    AV_SCOPE("GlVramGuard::invariants_hold");
    uint64_t total = 0u;
    sizes_.for_each([&total](uint64_t, const uint64_t &bytes) { total += bytes; });
    return total == account_.held() && sizes_.invariants_hold();
}

}

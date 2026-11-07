#include "executor/opengl/gl_texture_slices.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opengl/gl_error_queue.hpp"
#include "executor/opengl/gl_memory_objects.hpp"
#include "executor/opengl/gl_texture_shape.hpp"
#include "executor/shim/gl_dma_buf_images.hpp"

namespace av {

namespace {

constexpr uint32_t gl_invalid_value = 0x0501u;
constexpr uint32_t gl_out_of_memory = 0x0505u;
constexpr uint32_t one_d_targets[] = {0x0DE0u};
constexpr uint32_t two_d_targets[] = {0x0DE1u, 0x84F5u, 0x8C18u, 0x8513u};
constexpr uint32_t two_d_multisample_targets[] = {0x9100u};
constexpr uint32_t three_d_targets[] = {0x806Fu, 0x8C1Au, 0x9009u};
constexpr uint32_t three_d_multisample_targets[] = {0x9102u};

bool hook_tex_storage_mem_1d(void *user, GLenum target, GLsizei levels, GLenum format, GLsizei width, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_tex_storage_mem_1d");
    return static_cast<GlTextureSlices *>(user)->on_tex_storage_mem_1d(target, levels, format, width, memory, offset);
}

bool hook_tex_storage_mem_2d(void *user, GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_tex_storage_mem_2d");
    return static_cast<GlTextureSlices *>(user)->on_tex_storage_mem_2d(target, levels, format, width, height, memory, offset);
}

bool hook_tex_storage_mem_2d_multisample(void *user, GLenum target, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_tex_storage_mem_2d_multisample");
    return static_cast<GlTextureSlices *>(user)->on_tex_storage_mem_2d_multisample(target, samples, format, width, height, fixed, memory, offset);
}

bool hook_tex_storage_mem_3d(void *user, GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_tex_storage_mem_3d");
    return static_cast<GlTextureSlices *>(user)->on_tex_storage_mem_3d(target, levels, format, width, height, depth, memory, offset);
}

bool hook_tex_storage_mem_3d_multisample(void *user, GLenum target, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_tex_storage_mem_3d_multisample");
    return static_cast<GlTextureSlices *>(user)->on_tex_storage_mem_3d_multisample(target, samples, format, width, height, depth, fixed, memory, offset);
}

bool hook_texture_storage_mem_1d(void *user, GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_texture_storage_mem_1d");
    return static_cast<GlTextureSlices *>(user)->on_texture_storage_mem_1d(texture, levels, format, width, memory, offset);
}

bool hook_texture_storage_mem_2d(void *user, GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_texture_storage_mem_2d");
    return static_cast<GlTextureSlices *>(user)->on_texture_storage_mem_2d(texture, levels, format, width, height, memory, offset);
}

bool hook_texture_storage_mem_2d_multisample(void *user, GLuint texture, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_texture_storage_mem_2d_multisample");
    return static_cast<GlTextureSlices *>(user)->on_texture_storage_mem_2d_multisample(texture, samples, format, width, height, fixed, memory, offset);
}

bool hook_texture_storage_mem_3d(void *user, GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_texture_storage_mem_3d");
    return static_cast<GlTextureSlices *>(user)->on_texture_storage_mem_3d(texture, levels, format, width, height, depth, memory, offset);
}

bool hook_texture_storage_mem_3d_multisample(void *user, GLuint texture, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("hook_texture_storage_mem_3d_multisample");
    return static_cast<GlTextureSlices *>(user)->on_texture_storage_mem_3d_multisample(texture, samples, format, width, height, depth, fixed, memory, offset);
}
void follow_create_textures(void *user, GLenum target, GLsizei count, GLuint *textures)
{
    AV_SCOPE("follow_create_textures");
    static_cast<GlTextureSlices *>(user)->on_textures_created(target, count, textures);
}

void follow_bind_texture(void *user, GLenum target, GLuint texture)
{
    AV_SCOPE("follow_bind_texture");
    static_cast<GlTextureSlices *>(user)->on_texture_bound(target, texture);
}

bool hook_delete_textures(void *user, GLsizei count, const GLuint *textures)
{
    AV_SCOPE("hook_delete_textures");
    return static_cast<GlTextureSlices *>(user)->on_delete_textures(count, textures);
}

bool hook_texture_view(void *user, GLuint texture, GLenum target, GLuint original, GLenum format, GLuint min_level,
                       GLuint levels, GLuint min_layer, GLuint layers)
{
    AV_SCOPE("hook_texture_view");
    return static_cast<GlTextureSlices *>(user)->on_texture_view(texture, target, original, format, min_level, levels,
                                                                 min_layer, layers);
}

}

GlTextureSlices::GlTextureSlices(BlockSource &blocks, glproto::RealGl &gl, GlMemoryObjects &memories, GlDmaBufImages &images,
                                 GlErrorQueue &errors)
    : gl_(gl), memories_(memories), images_(images), errors_(errors), targets_(blocks)
{
    AV_SCOPE("GlTextureSlices::GlTextureSlices");
}

void GlTextureSlices::install()
{
    AV_SCOPE("GlTextureSlices::install");
    gl_.av_texture_user = this;
    gl_.av_after_glCreateTextures = follow_create_textures;
    gl_.av_after_glBindTexture = follow_bind_texture;
    gl_.av_hook_glTexStorageMem1DEXT = hook_tex_storage_mem_1d;
    gl_.av_hook_glTexStorageMem2DEXT = hook_tex_storage_mem_2d;
    gl_.av_hook_glTexStorageMem2DMultisampleEXT = hook_tex_storage_mem_2d_multisample;
    gl_.av_hook_glTexStorageMem3DEXT = hook_tex_storage_mem_3d;
    gl_.av_hook_glTexStorageMem3DMultisampleEXT = hook_tex_storage_mem_3d_multisample;
    gl_.av_hook_glTextureStorageMem1DEXT = hook_texture_storage_mem_1d;
    gl_.av_hook_glTextureStorageMem2DEXT = hook_texture_storage_mem_2d;
    gl_.av_hook_glTextureStorageMem2DMultisampleEXT = hook_texture_storage_mem_2d_multisample;
    gl_.av_hook_glTextureStorageMem3DEXT = hook_texture_storage_mem_3d;
    gl_.av_hook_glTextureStorageMem3DMultisampleEXT = hook_texture_storage_mem_3d_multisample;
    gl_.av_hook_glDeleteTextures = hook_delete_textures;
    gl_.av_hook_glTextureView = hook_texture_view;
}

bool GlTextureSlices::on_tex_storage_mem_1d(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_tex_storage_mem_1d");
    const GlTextureShape shape(target, format, levels, 1, width, 1, 1);
    const uint32_t texture = shape.tracked() ? bound_to(shape.binding()) : 0u;
    if (texture == 0u) return true;
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTexStorageMem1DEXT(target, levels, format, width, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_tex_storage_mem_2d(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_tex_storage_mem_2d");
    const GlTextureShape shape(target, format, levels, 1, width, height, 1);
    const uint32_t texture = shape.tracked() ? bound_to(shape.binding()) : 0u;
    if (texture == 0u) return true;
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTexStorageMem2DEXT(target, levels, format, width, height, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_tex_storage_mem_2d_multisample(GLenum target, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_tex_storage_mem_2d_multisample");
    const GlTextureShape shape(target, format, 1, samples, width, height, 1);
    const uint32_t texture = shape.tracked() ? bound_to(shape.binding()) : 0u;
    if (texture == 0u) return true;
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTexStorageMem2DMultisampleEXT(target, samples, format, width, height, fixed, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_tex_storage_mem_3d(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_tex_storage_mem_3d");
    const GlTextureShape shape(target, format, levels, 1, width, height, depth);
    const uint32_t texture = shape.tracked() ? bound_to(shape.binding()) : 0u;
    if (texture == 0u) return true;
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTexStorageMem3DEXT(target, levels, format, width, height, depth, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_tex_storage_mem_3d_multisample(GLenum target, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_tex_storage_mem_3d_multisample");
    const GlTextureShape shape(target, format, 1, samples, width, height, depth);
    const uint32_t texture = shape.tracked() ? bound_to(shape.binding()) : 0u;
    if (texture == 0u) return true;
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTexStorageMem3DMultisampleEXT(target, samples, format, width, height, depth, fixed, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_texture_storage_mem_1d(GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_texture_storage_mem_1d");
    const GlTextureShape shape = shape_of(texture, Entry::one_d, format, levels, 1, width, 1, 1);
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTextureStorageMem1DEXT(texture, levels, format, width, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_texture_storage_mem_2d(GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_texture_storage_mem_2d");
    const GlTextureShape shape = shape_of(texture, Entry::two_d, format, levels, 1, width, height, 1);
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    if (levels == 1 && over_image(texture, shape, memory, offset, width, height, format)) return false;
    errors_.collect();
    gl_.glTextureStorageMem2DEXT(texture, levels, format, width, height, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_texture_storage_mem_2d_multisample(GLuint texture, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_texture_storage_mem_2d_multisample");
    const GlTextureShape shape = shape_of(texture, Entry::two_d_multisample, format, 1, samples, width, height, 1);
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTextureStorageMem2DMultisampleEXT(texture, samples, format, width, height, fixed, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_texture_storage_mem_3d(GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_texture_storage_mem_3d");
    const GlTextureShape shape = shape_of(texture, Entry::three_d, format, levels, 1, width, height, depth);
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTextureStorageMem3DEXT(texture, levels, format, width, height, depth, memory, offset);
    return settle(texture, shape, memory, offset);
}

bool GlTextureSlices::on_texture_storage_mem_3d_multisample(GLuint texture, GLsizei samples, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixed, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("GlTextureSlices::on_texture_storage_mem_3d_multisample");
    const GlTextureShape shape = shape_of(texture, Entry::three_d_multisample, format, 1, samples, width, height, depth);
    const uint32_t error = vet(texture, shape, memory, offset);
    if (error != 0u) return refuse(error);
    errors_.collect();
    gl_.glTextureStorageMem3DMultisampleEXT(texture, samples, format, width, height, depth, fixed, memory, offset);
    return settle(texture, shape, memory, offset);
}
bool GlTextureSlices::on_delete_textures(GLsizei count, const GLuint *textures)
{
    AV_SCOPE("GlTextureSlices::on_delete_textures");
    for (GLsizei at = 0; textures != nullptr && at < count; at++) {
        memories_.leave_texture(textures[at]);
        targets_.forget(textures[at]);
    }
    return true;
}

void GlTextureSlices::on_textures_created(GLenum target, GLsizei count, const GLuint *textures)
{
    AV_SCOPE("GlTextureSlices::on_textures_created");
    if (!GlTextureShape(target, 0u, 1, 1, 1, 1, 1).tracked()) return;
    for (GLsizei at = 0; textures != nullptr && at < count; at++) static_cast<void>(targets_.note(textures[at], target));
}

void GlTextureSlices::on_texture_bound(GLenum target, GLuint texture)
{
    AV_SCOPE("GlTextureSlices::on_texture_bound");
    if (!GlTextureShape(target, 0u, 1, 1, 1, 1, 1).tracked()) return;
    static_cast<void>(targets_.note(texture, target));
}

bool GlTextureSlices::on_texture_view(GLuint texture, GLenum target, GLuint original, GLenum format, GLuint min_level,
                                      GLuint levels, GLuint min_layer, GLuint layers)
{
    AV_SCOPE("GlTextureSlices::on_texture_view");
    errors_.collect();
    gl_.glTextureView(texture, target, original, format, min_level, levels, min_layer, layers);
    const uint32_t error = errors_.take();
    errors_.collect();
    if (error != 0u) return refuse(error);
    const Status inherited = memories_.inherit_texture(texture, original);
    if (inherited == Status::no_memory) errors_.record(gl_out_of_memory);
    return false;
}

uint32_t GlTextureSlices::bound_to(uint32_t binding)
{
    AV_SCOPE("GlTextureSlices::bound_to");
    if (gl_.glGetIntegerv == nullptr) return 0u;
    GLint bound = 0;
    gl_.glGetIntegerv(binding, &bound);
    return static_cast<uint32_t>(bound);
}

GlTextureShape GlTextureSlices::shape_of(uint32_t texture, Entry entry, GLenum format, int64_t levels,
                                         int64_t samples, int64_t width, int64_t height, int64_t depth) const
{
    AV_SCOPE("GlTextureSlices::shape_of");
    const uint32_t known = targets_.target_of(texture);
    if (known != 0u) return GlTextureShape(known, format, levels, samples, width, height, depth);
    const uint32_t *candidates = one_d_targets;
    uint32_t        count = sizeof one_d_targets / sizeof one_d_targets[0];
    if (entry == Entry::two_d) {
        candidates = two_d_targets;
        count = sizeof two_d_targets / sizeof two_d_targets[0];
    } else if (entry == Entry::two_d_multisample) {
        candidates = two_d_multisample_targets;
        count = sizeof two_d_multisample_targets / sizeof two_d_multisample_targets[0];
    } else if (entry == Entry::three_d) {
        candidates = three_d_targets;
        count = sizeof three_d_targets / sizeof three_d_targets[0];
    } else if (entry == Entry::three_d_multisample) {
        candidates = three_d_multisample_targets;
        count = sizeof three_d_multisample_targets / sizeof three_d_multisample_targets[0];
    }
    GlTextureShape widest(candidates[0], format, levels, samples, width, height, depth);
    for (uint32_t at = 1u; at < count; at++) {
        const GlTextureShape other(candidates[at], format, levels, samples, width, height, depth);
        if (other.least_bytes() > widest.least_bytes()) widest = other;
    }
    return widest;
}

uint32_t GlTextureSlices::vet(uint32_t texture, const GlTextureShape &shape, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("GlTextureSlices::vet");
    const Status admitted = memories_.admit_texture(texture, memory, offset, shape.least_bytes());
    if (admitted == Status::ok) return 0u;
    return admitted == Status::no_memory ? gl_out_of_memory : gl_invalid_value;
}

bool GlTextureSlices::settle(uint32_t texture, const GlTextureShape &shape, uint32_t memory, uint64_t offset)
{
    AV_SCOPE("GlTextureSlices::settle");
    const uint32_t error = errors_.take();
    errors_.collect();
    if (error != 0u) return refuse(error);
    if (memories_.place_texture(texture, memory, offset, shape.least_bytes()) != Status::ok)
        errors_.record(gl_out_of_memory);
    return false;
}

bool GlTextureSlices::over_image(uint32_t texture, const GlTextureShape &shape, uint32_t memory, uint64_t offset,
                                 GLsizei width, GLsizei height, GLenum format)
{
    AV_SCOPE("GlTextureSlices::over_image");
    if (offset != 0u || width <= 0 || height <= 0 ||
        !images_.covers(memory, static_cast<uint32_t>(width), static_cast<uint32_t>(height), format))
        return false;
    errors_.collect();
    if (!images_.bind(texture, memory)) return false;
    const uint32_t error = errors_.take();
    errors_.collect();
    if (error != 0u) return false;
    if (memories_.place_texture(texture, memory, offset, shape.least_bytes()) != Status::ok)
        errors_.record(gl_out_of_memory);
    return true;
}

bool GlTextureSlices::refuse(uint32_t error)
{
    AV_SCOPE("GlTextureSlices::refuse");
    errors_.record(error);
    return false;
}

}

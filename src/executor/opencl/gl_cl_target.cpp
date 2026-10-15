#include "executor/opencl/gl_cl_target.hpp"

#include "core/trace/function_scope.hpp"

#include <CL/cl_gl.h>

namespace av {

namespace {

constexpr uint32_t gl_texture_cube_map = 0x8513u;

struct Named {
    uint32_t target;
    uint32_t bind;
    uint32_t image;
    uint32_t object;
    uint32_t dimensions;
};

/* Targets on the left, as glcorearb.h numbers them. */
constexpr Named targets[] = {
    {0x0DE0u, 0x0DE0u, CL_MEM_OBJECT_IMAGE1D, CL_GL_OBJECT_TEXTURE1D, 1u},                /* TEXTURE_1D */
    {0x8C18u, 0x8C18u, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_GL_OBJECT_TEXTURE1D_ARRAY, 2u},    /* TEXTURE_1D_ARRAY */
    {0x0DE1u, 0x0DE1u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},                /* TEXTURE_2D */
    {0x84F5u, 0x84F5u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},                /* TEXTURE_RECTANGLE */
    {0x8515u, gl_texture_cube_map, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},    /* CUBE_MAP_POSITIVE_X */
    {0x8516u, gl_texture_cube_map, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},    /* CUBE_MAP_NEGATIVE_X */
    {0x8517u, gl_texture_cube_map, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},    /* CUBE_MAP_POSITIVE_Y */
    {0x8518u, gl_texture_cube_map, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},    /* CUBE_MAP_NEGATIVE_Y */
    {0x8519u, gl_texture_cube_map, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},    /* CUBE_MAP_POSITIVE_Z */
    {0x851Au, gl_texture_cube_map, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},    /* CUBE_MAP_NEGATIVE_Z */
    {0x8C1Au, 0x8C1Au, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_GL_OBJECT_TEXTURE2D_ARRAY, 3u},    /* TEXTURE_2D_ARRAY */
    {0x806Fu, 0x806Fu, CL_MEM_OBJECT_IMAGE3D, CL_GL_OBJECT_TEXTURE3D, 3u},                /* TEXTURE_3D */
};

}  // namespace

GlClTarget::GlClTarget(uint32_t target)
    : image_target_(target), bind_target_(0u), image_type_(0u), object_type_(0u), dimensions_(0u)
{
    AV_SCOPE("GlClTarget::GlClTarget");
    for (const Named &named : targets)
        if (named.target == target) {
            bind_target_ = named.bind;
            image_type_ = named.image;
            object_type_ = named.object;
            dimensions_ = named.dimensions;
        }
}

bool GlClTarget::known() const
{
    AV_SCOPE("GlClTarget::known");
    return bind_target_ != 0u;
}

uint32_t GlClTarget::image_target() const
{
    AV_SCOPE("GlClTarget::image_target");
    return image_target_;
}

uint32_t GlClTarget::bind_target() const
{
    AV_SCOPE("GlClTarget::bind_target");
    return bind_target_;
}

uint32_t GlClTarget::image_type() const
{
    AV_SCOPE("GlClTarget::image_type");
    return image_type_;
}

uint32_t GlClTarget::object_type() const
{
    AV_SCOPE("GlClTarget::object_type");
    return object_type_;
}

uint32_t GlClTarget::dimensions() const
{
    AV_SCOPE("GlClTarget::dimensions");
    return dimensions_;
}

void GlClTarget::describe(uint32_t width, uint32_t height, uint32_t depth, cl_image_desc &desc) const
{
    AV_SCOPE("GlClTarget::describe");
    desc = cl_image_desc{};
    desc.image_type = image_type_;
    desc.image_width = width;
    if (image_type_ == CL_MEM_OBJECT_IMAGE1D_ARRAY) desc.image_array_size = height;
    if (image_type_ == CL_MEM_OBJECT_IMAGE2D || image_type_ == CL_MEM_OBJECT_IMAGE2D_ARRAY ||
        image_type_ == CL_MEM_OBJECT_IMAGE3D)
        desc.image_height = height;
    if (image_type_ == CL_MEM_OBJECT_IMAGE2D_ARRAY) desc.image_array_size = depth;
    if (image_type_ == CL_MEM_OBJECT_IMAGE3D) desc.image_depth = depth;
}

}  // namespace av

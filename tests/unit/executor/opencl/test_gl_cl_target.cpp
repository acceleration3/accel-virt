/* A texture target as the copy path needs it (GlClTarget): where it binds -
 * a cube map's faces at the cube map - the image OpenCL makes, the object type
 * the guest is told, how many dimensions a level's write takes, and the image
 * description a level's extent becomes. */
#include "executor/opencl/gl_cl_target.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl_gl.h>
#include <cstddef>
#include <cstdint>

using av::GlClTarget;

TEST(gl_cl_target, every_target_the_extension_lists_with_texels_of_its_own_is_known)
{ struct Case {
      std::uint32_t target, bind, image, object, dimensions;
  };
  const Case cases[] = {
      {0x0DE0u, 0x0DE0u, CL_MEM_OBJECT_IMAGE1D, CL_GL_OBJECT_TEXTURE1D, 1u},
      {0x8C18u, 0x8C18u, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_GL_OBJECT_TEXTURE1D_ARRAY, 2u},
      {0x0DE1u, 0x0DE1u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x84F5u, 0x84F5u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x8515u, 0x8513u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x8516u, 0x8513u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x8517u, 0x8513u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x8518u, 0x8513u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x8519u, 0x8513u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x851Au, 0x8513u, CL_MEM_OBJECT_IMAGE2D, CL_GL_OBJECT_TEXTURE2D, 2u},
      {0x8C1Au, 0x8C1Au, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_GL_OBJECT_TEXTURE2D_ARRAY, 3u},
      {0x806Fu, 0x806Fu, CL_MEM_OBJECT_IMAGE3D, CL_GL_OBJECT_TEXTURE3D, 3u},
  };
  for (const Case &item : cases) {
      const GlClTarget target(item.target);
      CHECK(target.known());
      CHECK_EQ(target.image_target(), item.target);
      CHECK_EQ(target.bind_target(), item.bind);
      CHECK_EQ(target.image_type(), item.image);
      CHECK_EQ(target.object_type(), item.object);
      CHECK_EQ(target.dimensions(), item.dimensions);
  } }

/* A buffer texture's texels are its buffer's, a multisampled one is
 * cl_khr_gl_msaa_sharing's, and a cube map is named by its faces. */
TEST(gl_cl_target, a_buffer_texture_a_multisampled_one_and_a_whole_cube_map_are_unknown)
{ const std::uint32_t unknown[] = {0u, 0x8C2Au /* TEXTURE_BUFFER */, 0x9100u /* TEXTURE_2D_MULTISAMPLE */,
                                   0x9102u /* TEXTURE_2D_MULTISAMPLE_ARRAY */, 0x8513u /* TEXTURE_CUBE_MAP */,
                                   0x9009u /* TEXTURE_CUBE_MAP_ARRAY */, 0x8D41u /* RENDERBUFFER */};
  for (const std::uint32_t named : unknown) {
      const GlClTarget target(named);
      CHECK(!target.known());
      CHECK_EQ(target.image_target(), named);
      CHECK_EQ(target.bind_target(), 0u);
      CHECK_EQ(target.image_type(), 0u);
      CHECK_EQ(target.object_type(), 0u);
      CHECK_EQ(target.dimensions(), 0u);
  } }

/* GL counts a 1D array's layers as its height and a 2D array's as its depth;
 * OpenCL counts both as the array's size, and leaves at zero what an image
 * has none of. */
TEST(gl_cl_target, a_level_s_extent_is_described_as_opencl_counts_it)
{ struct Case {
      std::uint32_t target, height, depth;
      std::size_t   type, desc_height, desc_depth, desc_layers;
  };
  const Case cases[] = {
      {0x0DE0u, 1u, 1u, CL_MEM_OBJECT_IMAGE1D, 0u, 0u, 0u},
      {0x8C18u, 5u, 1u, CL_MEM_OBJECT_IMAGE1D_ARRAY, 0u, 0u, 5u},
      {0x0DE1u, 5u, 1u, CL_MEM_OBJECT_IMAGE2D, 5u, 0u, 0u},
      {0x8519u, 5u, 1u, CL_MEM_OBJECT_IMAGE2D, 5u, 0u, 0u},
      {0x8C1Au, 5u, 7u, CL_MEM_OBJECT_IMAGE2D_ARRAY, 5u, 0u, 7u},
      {0x806Fu, 5u, 7u, CL_MEM_OBJECT_IMAGE3D, 5u, 7u, 0u},
  };
  for (const Case &item : cases) {
      const GlClTarget target(item.target);
      cl_image_desc    desc = {};
      desc.image_row_pitch = 99u;
      target.describe(64u, item.height, item.depth, desc);
      CHECK_EQ(desc.image_type, static_cast<cl_mem_object_type>(item.type));
      CHECK_EQ(desc.image_width, 64u);
      CHECK_EQ(desc.image_height, item.desc_height);
      CHECK_EQ(desc.image_depth, item.desc_depth);
      CHECK_EQ(desc.image_array_size, item.desc_layers);
      /* Written over, all of it. */
      CHECK_EQ(desc.image_row_pitch, 0u);
  } }

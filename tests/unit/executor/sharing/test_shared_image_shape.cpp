#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"

using av::SharedImageShape;

TEST(shared_image_shape, it_describes_an_image_only_with_every_field_there)
{ const SharedImageShape shape(640u, 480u, 87u, 2560u);
  CHECK(shape.described());
  CHECK_EQ(shape.width(), 640u);
  CHECK_EQ(shape.height(), 480u);
  CHECK_EQ(shape.format(), 87u);
  CHECK_EQ(shape.pitch(), 2560u);
  CHECK(!SharedImageShape().described());
  CHECK(!SharedImageShape(0u, 480u, 87u, 2560u).described());
  CHECK(!SharedImageShape(640u, 0u, 87u, 2560u).described());
  CHECK(!SharedImageShape(640u, 480u, 0u, 2560u).described());
  CHECK(!SharedImageShape(640u, 480u, 87u, 0u).described()); }

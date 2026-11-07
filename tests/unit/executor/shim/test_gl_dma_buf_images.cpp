#include "executor/sharing/shared_image_shape.hpp"
#include "executor/shim/gl_dma_buf_images.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <unistd.h>

using av::GlDmaBufImages;
using av::MockPlatform;
using av::SharedImageShape;

TEST(gl_dma_buf_images, a_host_with_no_image_import_takes_no_image_and_binds_none)
{ MockPlatform platform;
  GlDmaBufImages images(platform);
  CHECK(!images.imports_images());
  CHECK(!images.import(7u, 1, SharedImageShape(16u, 16u, 28u, 64u)));
  CHECK(!images.covers(7u, 16u, 16u, 0x8058u));
  CHECK(!images.bind(3u, 7u));
  images.forget(7u);
  images.give_back();
  CHECK_EQ(images.count(), 0u); }

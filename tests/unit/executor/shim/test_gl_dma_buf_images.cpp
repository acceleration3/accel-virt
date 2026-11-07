#include "executor/sharing/shared_image_shape.hpp"
#include "executor/shim/gl_dma_buf_images.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <unistd.h>

namespace av {
struct GlDmaBufImagesProbe {
    static void install(GlDmaBufImages &images, void *(*create)(void *, void *, unsigned, void *, const int *),
                        unsigned (*destroy)(void *, void *), void (*target)(unsigned, void *, const int *))
    {
        images.create_image_ = create;
        images.destroy_image_ = destroy;
        images.target_storage_ = target;
        images.display_ = &images;
    }
    static void uninstall(GlDmaBufImages &images)
    {
        images.display_ = nullptr;
        images.images_.clear();
    }
};
}

using av::GlDmaBufImages;
using av::GlDmaBufImagesProbe;
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

namespace {

int      last_fourcc = 0;
unsigned destroyed = 0u;
int      sentinel = 0;

void *fake_create(void *, void *, unsigned, void *, const int *attributes)
{
    for (const int *at = attributes; *at != EGL_NONE; at += 2)
        if (*at == EGL_LINUX_DRM_FOURCC_EXT) last_fourcc = at[1];
    return &sentinel;
}

unsigned fake_destroy(void *, void *)
{
    destroyed++;
    return 1u;
}

void fake_target(unsigned, void *, const int *) {}

constexpr unsigned abgr8888 = 0x34324241u;
constexpr unsigned argb8888 = 0x34325241u;
constexpr unsigned gl_rgba8 = 0x8058u;
constexpr unsigned gl_srgb8_alpha8 = 0x8C43u;
constexpr unsigned gl_rgb565 = 0x8D62u;

}

TEST(gl_dma_buf_images, the_srgb_formats_import_as_the_unorm_format_of_the_same_channel_order)
{ MockPlatform platform;
  GlDmaBufImages images(platform);
  GlDmaBufImagesProbe::install(images, &fake_create, &fake_destroy, &fake_target);
  CHECK(images.import(1u, 3, SharedImageShape(16u, 16u, 28u, 64u)));
  CHECK_EQ(static_cast<unsigned>(last_fourcc), abgr8888);
  CHECK(images.import(2u, 3, SharedImageShape(16u, 16u, 29u, 64u)));
  CHECK_EQ(static_cast<unsigned>(last_fourcc), abgr8888);
  CHECK(images.import(3u, 3, SharedImageShape(16u, 16u, 87u, 64u)));
  CHECK_EQ(static_cast<unsigned>(last_fourcc), argb8888);
  CHECK(images.import(4u, 3, SharedImageShape(16u, 16u, 91u, 64u)));
  CHECK_EQ(static_cast<unsigned>(last_fourcc), argb8888);
  CHECK(!images.import(5u, 3, SharedImageShape(16u, 16u, 2u, 64u)));
  images.give_back();
  GlDmaBufImagesProbe::uninstall(images); }

TEST(gl_dma_buf_images, an_image_covers_a_texture_of_its_size_in_rgba8_or_srgb8_alpha8_and_in_no_other_format)
{ MockPlatform platform;
  GlDmaBufImages images(platform);
  GlDmaBufImagesProbe::install(images, &fake_create, &fake_destroy, &fake_target);
  CHECK(images.import(1u, 3, SharedImageShape(16u, 8u, 28u, 64u)));
  CHECK(images.covers(1u, 16u, 8u, gl_rgba8));
  CHECK(images.covers(1u, 16u, 8u, gl_srgb8_alpha8));
  CHECK(!images.covers(1u, 16u, 8u, gl_rgb565));
  CHECK(!images.covers(1u, 8u, 16u, gl_srgb8_alpha8));
  CHECK(!images.covers(2u, 16u, 8u, gl_srgb8_alpha8));
  CHECK(images.bind(5u, 1u));
  images.forget(1u);
  CHECK_EQ(destroyed, 1u);
  GlDmaBufImagesProbe::uninstall(images); }

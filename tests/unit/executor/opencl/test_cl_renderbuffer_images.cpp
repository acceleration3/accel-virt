/* The cl_mems made over a renderbuffer through a texture of its storage: each
 * answers the renderbuffer it was made over until it is forgotten, which hands
 * back the texture and image to destroy; everything the table refuses, and the
 * invariant broken each way it can be. */
#include "executor/opencl/cl_renderbuffer_images.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

struct ClRenderbufferImagesProbe {
    static void count(ClRenderbufferImages &images, std::uint32_t count) { images.count_ = count; }
    static void mem(ClRenderbufferImages &images, std::uint32_t slot, const void *mem) { images.mems_[slot] = mem; }
    static void renderbuffer(ClRenderbufferImages &images, std::uint32_t slot, std::uint32_t name)
    { images.renderbuffers_[slot] = name; }
    static void texture(ClRenderbufferImages &images, std::uint32_t slot, std::uint32_t name)
    { images.textures_[slot] = name; }
    static void image(ClRenderbufferImages &images, std::uint32_t slot, void *image) { images.images_[slot] = image; }
};

}  // namespace av

using av::ClRenderbufferImages;
using av::ClRenderbufferImagesProbe;

namespace {

/* Addresses standing for host objects; nothing here dereferences one. */
const void *mem_at(std::uintptr_t at) { return reinterpret_cast<const void *>(at); }
void       *image_at(std::uintptr_t at) { return reinterpret_cast<void *>(at); }

}  // namespace

TEST(cl_renderbuffer_images, a_remembered_mem_names_its_renderbuffer_until_it_is_forgotten)
{ ClRenderbufferImages images;
  CHECK(images.invariants_hold());
  CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u)), 0u);
  CHECK(images.remember(mem_at(0x1000u), 7u, 31u, image_at(0xA000u)));
  CHECK(images.invariants_hold());
  CHECK(images.remember(mem_at(0x2000u), 8u, 32u, image_at(0xB000u)));
  CHECK(images.invariants_hold());
  CHECK_EQ(images.count(), 2u);
  CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u)), 7u);
  CHECK_EQ(images.renderbuffer_of(mem_at(0x2000u)), 8u);
  CHECK_EQ(images.renderbuffer_of(mem_at(0x3000u)), 0u);
  std::uint32_t texture = 0u;
  void         *image = nullptr;
  CHECK(images.forget(mem_at(0x1000u), texture, image));
  CHECK(images.invariants_hold());
  CHECK_EQ(texture, 31u);
  CHECK(image == image_at(0xA000u));
  CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u)), 0u);
  /* The one that moved into the hole still answers its own. */
  CHECK_EQ(images.renderbuffer_of(mem_at(0x2000u)), 8u);
  CHECK_EQ(images.count(), 1u);
  /* Forgotten once is forgotten: a second forget leaves the answers alone. */
  texture = 99u;
  image = image_at(0xC000u);
  CHECK(!images.forget(mem_at(0x1000u), texture, image));
  CHECK_EQ(texture, 99u);
  CHECK(image == image_at(0xC000u)); }

TEST(cl_renderbuffer_images, refuses_a_null_mem_no_names_a_mem_held_twice_and_one_past_full)
{ ClRenderbufferImages images;
  CHECK(!images.remember(nullptr, 7u, 31u, image_at(0xA000u)));
  CHECK(!images.remember(mem_at(0x1000u), 0u, 31u, image_at(0xA000u)));
  CHECK(!images.remember(mem_at(0x1000u), 7u, 0u, image_at(0xA000u)));
  CHECK_EQ(images.count(), 0u);
  CHECK(images.remember(mem_at(0x1000u), 7u, 31u, image_at(0xA000u)));
  CHECK(!images.remember(mem_at(0x1000u), 9u, 33u, image_at(0xB000u)));
  CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u)), 7u);
  for (std::uint32_t more = 1u; more < ClRenderbufferImages::max_images; more++)
      CHECK(images.remember(mem_at(0x1000u + more * 0x100u), more + 7u, more + 31u, nullptr));
  CHECK_EQ(images.count(), ClRenderbufferImages::max_images);
  CHECK(!images.remember(mem_at(0x90000u), 5u, 6u, nullptr));
  CHECK(images.invariants_hold()); }

TEST(cl_renderbuffer_images, forget_any_empties_the_table_one_at_a_time)
{ ClRenderbufferImages images;
  std::uint32_t texture = 0u;
  void         *image = nullptr;
  CHECK(!images.forget_any(texture, image));
  CHECK(images.remember(mem_at(0x1000u), 7u, 31u, image_at(0xA000u)));
  CHECK(images.remember(mem_at(0x2000u), 8u, 32u, image_at(0xB000u)));
  std::uint32_t textures = 0u;
  std::uint32_t forgotten = 0u;
  while (images.forget_any(texture, image)) {
      CHECK(images.invariants_hold());
      textures += texture;
      forgotten++;
  }
  CHECK_EQ(forgotten, 2u);
  CHECK_EQ(textures, 63u);
  CHECK_EQ(images.count(), 0u); }

TEST(cl_renderbuffer_images_invariants, each_way_the_table_can_be_wrong_is_caught)
{ { ClRenderbufferImages images;
    ClRenderbufferImagesProbe::count(images, ClRenderbufferImages::max_images + 1u);
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images;
    CHECK(images.remember(mem_at(0x1000u), 7u, 31u, nullptr));
    ClRenderbufferImagesProbe::mem(images, 0u, nullptr);
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images;
    CHECK(images.remember(mem_at(0x1000u), 7u, 31u, nullptr));
    ClRenderbufferImagesProbe::renderbuffer(images, 0u, 0u);
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images;
    CHECK(images.remember(mem_at(0x1000u), 7u, 31u, nullptr));
    ClRenderbufferImagesProbe::texture(images, 0u, 0u);
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images;
    CHECK(images.remember(mem_at(0x1000u), 7u, 31u, nullptr));
    CHECK(images.remember(mem_at(0x2000u), 8u, 32u, nullptr));
    ClRenderbufferImagesProbe::mem(images, 1u, mem_at(0x1000u));
    CHECK(!images.invariants_hold()); }
  /* Past the held: each of the four left behind is caught. */
  { ClRenderbufferImages images;
    ClRenderbufferImagesProbe::mem(images, 3u, mem_at(0x1000u));
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images;
    ClRenderbufferImagesProbe::renderbuffer(images, 3u, 7u);
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images;
    ClRenderbufferImagesProbe::texture(images, 3u, 31u);
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images;
    ClRenderbufferImagesProbe::image(images, 3u, image_at(0xA000u));
    CHECK(!images.invariants_hold()); } }

/* The cl_mems made over a renderbuffer through a texture of its storage: each
 * answers the renderbuffer it was made over until it is forgotten, which hands
 * back the texture and image to destroy; everything the table refuses, and the
 * invariant broken each way it can be. */
#include "executor/opencl/cl_renderbuffer_images.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

struct ClRenderbufferImagesProbe {
    static RenderbufferImage &held(ClRenderbufferImages &images, const void *mem)
    {
        return *images.images_.find(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mem)));
    }
    static void hold_null(ClRenderbufferImages &images)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(images.images_.insert(0u, RenderbufferImage{7u, 31u, nullptr}, slot));
    }
};

}  // namespace av

using av::ClRenderbufferImages;
using av::ClRenderbufferImagesProbe;
using av::MockPlatform;
using av::Status;

namespace {

/* Addresses standing for host objects; nothing here dereferences one. */
const void *mem_at(std::uintptr_t at) { return reinterpret_cast<const void *>(at); }
void       *image_at(std::uintptr_t at) { return reinterpret_cast<void *>(at); }

}  // namespace

TEST(cl_renderbuffer_images, a_remembered_mem_names_its_renderbuffer_until_it_is_forgotten)
{ MockPlatform         platform;
  ClRenderbufferImages images(platform);
  CHECK(images.invariants_hold());
  CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u)), 0u);
  CHECK(images.remember(mem_at(0x1000u), 7u, 31u, image_at(0xA000u)) == Status::ok);
  CHECK(images.invariants_hold());
  CHECK(images.remember(mem_at(0x2000u), 8u, 32u, image_at(0xB000u)) == Status::ok);
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
  /* The other still answers its own. */
  CHECK_EQ(images.renderbuffer_of(mem_at(0x2000u)), 8u);
  CHECK_EQ(images.count(), 1u);
  /* Forgotten once is forgotten: a second forget leaves the answers alone. */
  texture = 99u;
  image = image_at(0xC000u);
  CHECK(!images.forget(mem_at(0x1000u), texture, image));
  CHECK_EQ(texture, 99u);
  CHECK(image == image_at(0xC000u)); }

TEST(cl_renderbuffer_images, refuses_a_null_mem_no_names_a_mem_held_twice_and_a_table_that_cannot_grow)
{ MockPlatform         platform;
  ClRenderbufferImages images(platform);
  CHECK(images.remember(nullptr, 7u, 31u, image_at(0xA000u)) == Status::invalid_parameter);
  CHECK(images.remember(mem_at(0x1000u), 0u, 31u, image_at(0xA000u)) == Status::invalid_parameter);
  CHECK(images.remember(mem_at(0x1000u), 7u, 0u, image_at(0xA000u)) == Status::invalid_parameter);
  CHECK_EQ(images.count(), 0u);
  CHECK(images.remember(mem_at(0x1000u), 7u, 31u, image_at(0xA000u)) == Status::ok);
  CHECK(images.remember(mem_at(0x1000u), 9u, 33u, image_at(0xB000u)) == Status::conflict);
  CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u)), 7u);
  /* Well past the 64 a context once shared at once. */
  for (std::uint32_t more = 1u; more < 1000u; more++)
      CHECK(images.remember(mem_at(0x1000u + more * 0x100u), more + 7u, more + 31u, nullptr) == Status::ok);
  CHECK_EQ(images.count(), 1000u);
  for (std::uint32_t more = 1u; more < 1000u; more++)
      CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u + more * 0x100u)), more + 7u);
  std::uint32_t next = 1000u;
  platform.fail_allocation_at(1u);
  while (images.remember(mem_at(0x1000u + next * 0x100u), 5u, 6u, nullptr) == Status::ok) next++;
  CHECK_EQ(images.renderbuffer_of(mem_at(0x1000u + next * 0x100u)), 0u);
  CHECK_EQ(images.count(), next);
  CHECK(images.invariants_hold()); }

TEST(cl_renderbuffer_images, forget_any_empties_the_table_one_at_a_time)
{ MockPlatform         platform;
  ClRenderbufferImages images(platform);
  std::uint32_t texture = 0u;
  void         *image = nullptr;
  CHECK(!images.forget_any(texture, image));
  CHECK(images.remember(mem_at(0x1000u), 7u, 31u, image_at(0xA000u)) == Status::ok);
  CHECK(images.remember(mem_at(0x2000u), 8u, 32u, image_at(0xB000u)) == Status::ok);
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
{ MockPlatform platform;
  { ClRenderbufferImages images(platform);
    ClRenderbufferImagesProbe::hold_null(images);
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images(platform);
    CHECK(images.remember(mem_at(0x1000u), 7u, 31u, nullptr) == Status::ok);
    ClRenderbufferImagesProbe::held(images, mem_at(0x1000u)).renderbuffer_ = 0u;
    CHECK(!images.invariants_hold()); }
  { ClRenderbufferImages images(platform);
    CHECK(images.remember(mem_at(0x1000u), 7u, 31u, nullptr) == Status::ok);
    ClRenderbufferImagesProbe::held(images, mem_at(0x1000u)).texture_ = 0u;
    CHECK(!images.invariants_hold()); } }

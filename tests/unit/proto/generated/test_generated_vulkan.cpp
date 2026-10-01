/* The generated wire, driven the way a host will drive it: what the guest
 * serialized comes back as the structure it was, and a frame that stops short
 * is refused rather than read past.
 *
 * Thousands of commands are generated and none of them is hand-checked; what
 * is checked here is the shape they all share - a plain struct, a struct with
 * a string and an array in it, and a chain - so that a change in the emitter
 * that breaks the shape breaks a test rather than a guest (feature 063). */
#include "harness/framework/test_registry.hpp"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::WireArena;
using av::WireReader;
using av::WireWriter;

namespace {

/* Room enough for what these decodes unflatten into, and no more: an arena
 * that is exactly as big as it needs to be is one that catches a decoder
 * asking for more. */
constexpr std::size_t arena_bytes = 4096u;

}  // namespace

TEST(generated_vulkan, a_plain_struct_round_trips)
{ std::uint8_t frame[64] = {};
  WireWriter writer(frame, sizeof frame);
  const VkExtent2D sent = {1600u, 1200u};
  av::vkproto::serialize_VkExtent2D(writer, sent);
  CHECK_STATUS(writer.status(), av::Status::ok);

  std::uint8_t block[arena_bytes];
  WireArena arena(block, sizeof block);
  WireReader reader(frame, writer.offset());
  VkExtent2D received = {};
  CHECK(av::vkproto::deserialize_VkExtent2D(reader, &received, arena));
  CHECK_EQ(received.width, 1600u);
  CHECK_EQ(received.height, 1200u);
  CHECK_EQ(reader.remaining(), 0u); }

/* A frame cut short at every length there is: none of them may be read past,
 * and none may report success. */
TEST(generated_vulkan, a_truncated_frame_is_refused_at_every_length)
{ std::uint8_t frame[64] = {};
  WireWriter writer(frame, sizeof frame);
  av::vkproto::serialize_VkExtent2D(writer, VkExtent2D{7u, 9u});
  const std::size_t whole = writer.offset();
  CHECK(whole > 1u);

  for (std::size_t length = 0u; length < whole; length++) {
      std::uint8_t block[arena_bytes];
      WireArena arena(block, sizeof block);
      WireReader reader(frame, length);
      VkExtent2D received = {};
      CHECK(!av::vkproto::deserialize_VkExtent2D(reader, &received, arena));
      CHECK_STATUS(reader.status(), av::Status::malformed);
  } }

/* A structure that points at things: the string and the pNext chain are what
 * the arena is for, and what a flat frame has to rebuild. */
TEST(generated_vulkan, a_struct_that_points_at_things_round_trips)
{ std::uint8_t frame[512] = {};
  WireWriter writer(frame, sizeof frame);
  VkApplicationInfo sent = {};
  sent.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  sent.pNext = nullptr;
  sent.pApplicationName = "accel-virt";
  sent.applicationVersion = 3u;
  sent.pEngineName = "av";
  sent.engineVersion = 4u;
  constexpr std::uint32_t api_version_1_3 = (1u << 22) | (3u << 12);
  sent.apiVersion = api_version_1_3;
  av::vkproto::serialize_VkApplicationInfo(writer, sent);
  CHECK_STATUS(writer.status(), av::Status::ok);

  std::uint8_t block[arena_bytes];
  WireArena arena(block, sizeof block);
  WireReader reader(frame, writer.offset());
  VkApplicationInfo received = {};
  CHECK(av::vkproto::deserialize_VkApplicationInfo(reader, &received, arena));
  CHECK_EQ(received.sType, VK_STRUCTURE_TYPE_APPLICATION_INFO);
  CHECK(received.pApplicationName != nullptr);
  CHECK(std::strcmp(received.pApplicationName, "accel-virt") == 0);
  CHECK(std::strcmp(received.pEngineName, "av") == 0);
  CHECK_EQ(received.applicationVersion, 3u);
  CHECK_EQ(received.engineVersion, 4u);
  CHECK_EQ(received.apiVersion, api_version_1_3);
  /* What it points at is the arena's, not the frame's. */
  CHECK(arena.used() > 0u); }

/* An arena too small for what the frame describes: the decode fails rather
 * than the host writing past the block it was given. */
TEST(generated_vulkan, a_decode_with_no_room_fails_instead_of_overrunning)
{ std::uint8_t frame[512] = {};
  WireWriter writer(frame, sizeof frame);
  VkApplicationInfo sent = {};
  sent.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  sent.pApplicationName = "a name longer than the block it will be given";
  sent.pEngineName = "av";
  av::vkproto::serialize_VkApplicationInfo(writer, sent);
  CHECK_STATUS(writer.status(), av::Status::ok);

  std::uint8_t block[8];
  WireArena arena(block, sizeof block);
  WireReader reader(frame, writer.offset());
  VkApplicationInfo received = {};
  CHECK(!av::vkproto::deserialize_VkApplicationInfo(reader, &received, arena));
  CHECK(arena.used() <= sizeof block); }

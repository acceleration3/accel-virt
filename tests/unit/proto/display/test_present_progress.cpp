/* The present engine's page as the device writes it and the guest reads it
 * back: the magic, the version and both counts at their documented offsets,
 * and anything a device could get wrong - magic, version, more refused than
 * finished - refused with the reader's counts unchanged. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/display/present_progress.hpp"

#include <cstdint>
#include <cstring>

using av::PresentProgress;
using av::Status;

namespace {

template <typename T>
T field_at(const std::uint8_t *bytes, std::size_t offset)
{
    T value = 0;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

}  // namespace

TEST(present_progress, the_counts_sit_where_the_page_documents_them)
{ std::uint8_t bytes[PresentProgress::encoded_size] = {};
  CHECK_STATUS(PresentProgress(0x100000002u, 3u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(field_at<std::uint32_t>(bytes, 0u), 0x00505641u);
  CHECK_EQ(field_at<std::uint32_t>(bytes, 4u), 1u);
  CHECK_EQ(field_at<std::uint64_t>(bytes, PresentProgress::completed_at), 0x100000002u);
  CHECK_EQ(field_at<std::uint64_t>(bytes, PresentProgress::refused_at), 3u);
  CHECK_EQ(PresentProgress::completed_at, 8u);
  CHECK_EQ(PresentProgress::refused_at, 16u);
  PresentProgress read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(read.completed(), 0x100000002u);
  CHECK_EQ(read.refused(), 3u);
  CHECK_STATUS(PresentProgress(1u, 1u).encode(bytes, sizeof bytes - 1u), Status::too_large); }

TEST(present_progress, a_page_the_guest_cannot_trust_leaves_what_it_had)
{ std::uint8_t good[PresentProgress::encoded_size] = {};
  CHECK_STATUS(PresentProgress(5u, 5u).encode(good, sizeof good), Status::ok);
  struct Damage {
      std::size_t  offset;
      std::uint8_t value;
  };
  const Damage damages[] = {{0u, 0x53u},   /* magic */
                            {4u, 2u},      /* version */
                            {16u, 6u},     /* one more refused than finished */
                            {23u, 1u}};
  for (const Damage &damage : damages) {
      std::uint8_t bytes[PresentProgress::encoded_size] = {};
      std::memcpy(bytes, good, sizeof bytes);
      bytes[damage.offset] = damage.value;
      PresentProgress progress(9u, 2u);
      CHECK_STATUS(progress.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(progress.completed(), 9u);
      CHECK_EQ(progress.refused(), 2u);
  }
  PresentProgress progress(9u, 2u);
  CHECK_STATUS(progress.decode(good, sizeof good - 1u), Status::malformed);
  CHECK_EQ(progress.completed(), 9u);
  CHECK_STATUS(progress.decode(good, sizeof good), Status::ok);
  CHECK_EQ(progress.completed(), 5u);
  CHECK_EQ(progress.refused(), 5u); }

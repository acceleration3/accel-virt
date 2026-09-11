/* One pinned blob's bytes, as a worker reads and writes them: across extents,
 * and never outside the blob. */
#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::BlobWindow;
using av::GuardedBuffer;
using av::Status;

namespace av {

struct BlobWindowProbe {
    static void miscount(BlobWindow &window) { window.length_ += 1u; }
    static void lose_pointers(BlobWindow &window) { window.pointers_ = nullptr; }
};

}  // namespace av

namespace {

/* Three extents of 3, 1 and 4 bytes: a blob of 8 bytes numbered 0..7. */
class Extents {
public:
    Extents() : first_{0u, 1u, 2u}, second_{3u}, third_{4u, 5u, 6u, 7u},
                pointers_{first_, second_, third_}, lengths_{3u, 1u, 4u} {}
    BlobWindow window() const { return BlobWindow(pointers_, lengths_, 3u, 8u); }
    std::uint8_t byte(std::uint32_t index) const
    {
        return index < 3u ? first_[index] : index < 4u ? second_[0] : third_[index - 4u];
    }

private:
    std::uint8_t  first_[3];
    std::uint8_t  second_[1];
    std::uint8_t  third_[4];
    void         *pointers_[3];
    std::uint64_t lengths_[3];
};

}  // namespace

TEST(blob_window_read, copies_across_every_extent)
{ const Extents extents;
  const BlobWindow window = extents.window();
  CHECK_EQ(window.length(), 8u);
  CHECK(window.invariants_hold());
  for (std::uint64_t offset = 0u; offset < 8u; offset++) {
      for (std::uint64_t count = 0u; offset + count <= 8u; count++) {
          GuardedBuffer buffer(static_cast<std::size_t>(count));
          CHECK_STATUS(window.read(offset, buffer.data(), static_cast<std::size_t>(count)), Status::ok);
          for (std::uint64_t i = 0u; i < count; i++) CHECK_EQ(buffer.data()[i], offset + i);
          CHECK(buffer.guards_intact());
      }
  } }

TEST(blob_window_write, lands_across_every_extent)
{ Extents extents;
  const BlobWindow window = extents.window();
  const std::uint8_t pattern[6] = {0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u};
  CHECK_STATUS(window.write(1u, pattern, sizeof pattern), Status::ok);
  CHECK_EQ(extents.byte(0u), 0u);
  for (std::uint32_t i = 0u; i < 6u; i++) CHECK_EQ(extents.byte(1u + i), 0xA0u + i);
  CHECK_EQ(extents.byte(7u), 7u); }

TEST(blob_window_read, a_range_outside_the_blob_copies_nothing)
{ const Extents extents;
  const BlobWindow window = extents.window();
  struct Case { std::uint64_t offset; std::size_t count; };
  const Case cases[] = {{8u, 1u}, {7u, 2u}, {0u, 9u}, {9u, 0u}, {~std::uint64_t{0}, 2u},
                        {1u, ~std::size_t{0}}};
  for (const Case &c : cases) {
      std::uint8_t destination[2] = {0xEEu, 0xEEu};
      CHECK_STATUS(window.read(c.offset, destination, c.count), Status::invalid_parameter);
      CHECK_EQ(destination[0], 0xEEu);
      CHECK_STATUS(window.write(c.offset, destination, c.count), Status::invalid_parameter);
  }
  for (std::uint32_t i = 0u; i < 8u; i++) CHECK_EQ(extents.byte(i), i);
  std::uint8_t nothing = 0u;
  CHECK_STATUS(window.read(8u, &nothing, 0u), Status::ok); }

TEST(blob_window, an_empty_window_holds_nothing)
{ const BlobWindow window;
  CHECK_EQ(window.length(), 0u);
  CHECK(window.invariants_hold());
  std::uint8_t byte = 0u;
  CHECK_STATUS(window.read(0u, &byte, 1u), Status::invalid_parameter);
  CHECK_STATUS(window.read(0u, &byte, 0u), Status::ok); }

TEST(blob_window_invariants, a_length_other_than_the_extents_sum_is_detected)
{ const Extents extents;
  BlobWindow window = extents.window();
  av::BlobWindowProbe::miscount(window);
  CHECK(!window.invariants_hold());
  BlobWindow lost = extents.window();
  av::BlobWindowProbe::lose_pointers(lost);
  CHECK(!lost.invariants_hold()); }

#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"
#include "device/context/ring_window.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/ring_header.hpp"

#include <cstdint>
#include <cstring>

using av::BlobWindow;
using av::ByteLayout;
using av::RingHeader;
using av::RingWindow;
using av::Status;

namespace {

constexpr std::uint64_t data_bytes = 4096u;

class RingBlob {
public:
    RingBlob() : bytes_{}, pointers_{bytes_, bytes_ + split}, lengths_{split, sizeof bytes_ - split}
    {
        static_cast<void>(RingHeader(12u, 0u, 0u).encode(bytes_, sizeof bytes_));
        for (std::uint64_t i = 0u; i < data_bytes; i++) bytes_[32u + i] = static_cast<std::uint8_t>(i * 7u);
    }
    BlobWindow    blob() const { return BlobWindow(pointers_, lengths_, 2u, sizeof bytes_); }
    RingWindow    ring() const { return RingWindow(blob(), 12u); }
    std::uint8_t *bytes() { return bytes_; }

private:
    static constexpr std::uint64_t split = 1000u;

    alignas(8) std::uint8_t bytes_[32u + data_bytes];
    void         *pointers_[2];
    std::uint64_t lengths_[2];
};

}

TEST(ring_window_snapshot, decodes_the_header_the_guest_wrote)
{ RingBlob blob;
  static_cast<void>(RingHeader(12u, 0x5000u, 0x4800u).encode(blob.bytes(), 32u));
  const RingWindow ring = blob.ring();
  CHECK(ring.invariants_hold());
  CHECK_EQ(ring.order(), 12u);
  CHECK_EQ(ring.capacity(), data_bytes);
  RingHeader header;
  CHECK_STATUS(ring.snapshot(header), Status::ok);
  CHECK_EQ(header.order(), 12u);
  CHECK(header.tail() == 0x5000u);
  CHECK(header.head() == 0x4800u); }

TEST(ring_window_snapshot, reads_the_tail_as_one_word_and_refuses_a_header_that_splits_it)
{ RingBlob blob;
  static_cast<void>(RingHeader(12u, 0x0102030405060708u, 0x10u).encode(blob.bytes(), 32u));
  RingHeader header;
  CHECK_STATUS(blob.ring().snapshot(header), Status::ok);
  CHECK(header.tail() == 0x0102030405060708u);
  void *const         pointers[2] = {blob.bytes(), blob.bytes() + 12u};
  const std::uint64_t lengths[2] = {12u, 32u + data_bytes - 12u};
  const RingWindow    split(BlobWindow(pointers, lengths, 2u, 32u + data_bytes), 12u);
  RingHeader          untouched(20u, 1u, 1u);
  CHECK_STATUS(split.snapshot(untouched), Status::invalid_parameter);
  CHECK_EQ(untouched.order(), 20u);
  CHECK_STATUS(split.publish_head(5u), Status::ok);
  void *const         head_split[2] = {blob.bytes(), blob.bytes() + 20u};
  const std::uint64_t head_lengths[2] = {20u, 32u + data_bytes - 20u};
  const RingWindow    split_head(BlobWindow(head_split, head_lengths, 2u, 32u + data_bytes), 12u);
  CHECK_STATUS(split_head.publish_head(6u), Status::invalid_parameter);
  const ByteLayout layout(blob.bytes(), 32u);
  CHECK(layout.at(16u, 8u) == 5u); }

TEST(ring_window_snapshot, a_malformed_header_is_malformed_and_leaves_the_snapshot)
{ RingBlob blob;
  blob.bytes()[0] = 0u;
  RingHeader header(20u, 1u, 1u);
  CHECK_STATUS(blob.ring().snapshot(header), Status::malformed);
  CHECK_EQ(header.order(), 20u); }

TEST(ring_window_copy, reads_from_the_counter_position_and_wraps_at_the_end)
{ const RingBlob blob;
  const RingWindow ring = blob.ring();
  struct Case { std::uint64_t counter; std::size_t count; };
  const Case cases[] = {{0u, 16u}, {990u, 20u}, {4090u, 16u}, {3u * data_bytes + 4095u, 2u},
                        {~std::uint64_t{0}, 4u}, {12u, data_bytes}, {0u, 0u}};
  for (const Case &c : cases) {
      std::uint8_t out[data_bytes] = {};
      CHECK_STATUS(ring.copy(c.counter, out, c.count), Status::ok);
      for (std::size_t i = 0u; i < c.count; i++)
          CHECK_EQ(out[i], static_cast<std::uint8_t>(((c.counter + i) & (data_bytes - 1u)) * 7u));
  } }

TEST(ring_window_copy, more_than_the_data_area_copies_nothing)
{ const RingBlob blob;
  std::uint8_t out[data_bytes + 1u] = {};
  out[0] = 0xEEu;
  CHECK_STATUS(blob.ring().copy(5u, out, data_bytes + 1u), Status::invalid_parameter);
  CHECK_EQ(out[0], 0xEEu); }

TEST(ring_window_publish_head, writes_only_the_head_field)
{ RingBlob blob;
  static_cast<void>(RingHeader(12u, 0x40u, 0u).encode(blob.bytes(), 32u));
  std::uint8_t before[32u + data_bytes];
  std::memcpy(before, blob.bytes(), sizeof before);
  CHECK_STATUS(blob.ring().publish_head(0x1122334455667788u), Status::ok);
  const ByteLayout layout(blob.bytes(), 32u);
  CHECK(layout.at(16u, 8u) == 0x1122334455667788u);
  CHECK(std::memcmp(before, blob.bytes(), 16u) == 0);
  CHECK(std::memcmp(before + 24u, blob.bytes() + 24u, sizeof before - 24u) == 0); }

TEST(ring_window, a_blob_too_small_for_its_order_refuses_what_it_cannot_reach)
{ RingBlob blob;
  const RingWindow ring(BlobWindow(blob.blob()), 13u);
  CHECK(!ring.invariants_hold());
  std::uint8_t out[8] = {};
  CHECK_STATUS(ring.copy(8000u, out, sizeof out), Status::invalid_parameter);
  const std::uint8_t header_bytes[8] = {};
  void *const pointer = static_cast<void *>(const_cast<std::uint8_t *>(header_bytes));
  const std::uint64_t length = 8u;
  const RingWindow tiny(BlobWindow(&pointer, &length, 1u, 8u), 12u);
  RingHeader header;
  CHECK_STATUS(tiny.snapshot(header), Status::invalid_parameter);
  CHECK_STATUS(tiny.publish_head(1u), Status::invalid_parameter);
  const RingWindow empty;
  CHECK_EQ(empty.capacity(), 0u);
  CHECK(!empty.invariants_hold());
  CHECK(!RingWindow(blob.blob(), 25u).invariants_hold()); }

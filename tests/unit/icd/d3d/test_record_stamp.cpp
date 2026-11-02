#include "harness/framework/test_registry.hpp"
#include "icd/d3d/record_stamp.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::RecordStamp;

namespace {

constexpr std::uint32_t resource_tag = 0x00525641u;
constexpr std::uint32_t heap_tag = 0x00485641u;

}

TEST(record_stamp, holds_the_kind_it_was_stamped_with_and_no_other)
{ const RecordStamp stamp(resource_tag, 42u);
  CHECK(stamp.holds(resource_tag));
  CHECK(!stamp.holds(heap_tag));
  CHECK(stamp.id() == 42u); }

TEST(record_stamp, an_unstamped_record_holds_no_kind)
{ const RecordStamp stamp;
  CHECK(!stamp.holds(resource_tag));
  CHECK(!stamp.holds(heap_tag));
  CHECK(stamp.id() == 0u); }

TEST(record_stamp, is_read_from_the_bytes_of_the_block)
{ const RecordStamp stamped(heap_tag, 9u);
  alignas(RecordStamp) unsigned char block[sizeof(RecordStamp)];
  std::memcpy(block, &stamped, sizeof block);
  RecordStamp read;
  std::memcpy(&read, block, sizeof block);
  CHECK(read.holds(heap_tag));
  CHECK(read.id() == 9u);
  block[0] ^= 0xFFu;
  std::memcpy(&read, block, sizeof block);
  CHECK(!read.holds(heap_tag)); }

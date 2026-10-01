/* The miniport's answer to UMDRIVERPRIVATE, both ways round and across
 * versions: three words where the caller asked for three, two where it asked
 * for two, and a reader that finds only two reads no feature. Another tag or
 * fewer than two words is no answer of this driver's. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/umd_adapter_info.hpp"

#include <cstddef>
#include <cstdint>

using av::ByteLayout;
using av::Status;
using av::UmdAdapterInfo;

TEST(umd_adapter_info, three_words_where_they_fit_and_the_first_two_where_only_those_do)
{ std::uint8_t bytes[32] = {};
  std::size_t  written = 0u;
  const UmdAdapterInfo info(0x1122334455667788ull, UmdAdapterInfo::feature_present_snapshots);
  CHECK_STATUS(info.encode(bytes, sizeof bytes, written), Status::ok);
  CHECK_EQ(written, 24u);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 8u) == 0x0044494C555641ull);
  CHECK(layout.at(8u, 8u) == 0x1122334455667788ull);
  CHECK(layout.at(16u, 8u) == 1u);
  std::uint8_t old[16] = {};
  CHECK_STATUS(info.encode(old, sizeof old, written), Status::ok);
  CHECK_EQ(written, 16u);
  CHECK_STATUS(info.encode(old, 15u, written), Status::too_large);
  CHECK_EQ(written, 0u); }

TEST(umd_adapter_info, a_reader_takes_the_features_where_there_are_three_words)
{ std::uint8_t bytes[24] = {};
  std::size_t  written = 0u;
  CHECK_STATUS(UmdAdapterInfo(5u, UmdAdapterInfo::feature_present_snapshots).encode(bytes, sizeof bytes, written),
               Status::ok);
  UmdAdapterInfo read;
  CHECK(!read.offers(UmdAdapterInfo::feature_present_snapshots));
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::ok);
  CHECK(read.luid() == 5u);
  CHECK(read.offers(UmdAdapterInfo::feature_present_snapshots));
  UmdAdapterInfo two;
  CHECK_STATUS(two.decode(bytes, 16u), Status::ok);   /* a miniport from before the features */
  CHECK(two.luid() == 5u);
  CHECK(!two.offers(UmdAdapterInfo::feature_present_snapshots)); }

TEST(umd_adapter_info, another_tag_or_too_few_words_is_malformed_and_changes_nothing)
{ std::uint8_t bytes[24] = {};
  std::size_t  written = 0u;
  CHECK_STATUS(UmdAdapterInfo(5u, 1u).encode(bytes, sizeof bytes, written), Status::ok);
  UmdAdapterInfo read(9u, 0u);
  CHECK_STATUS(read.decode(bytes, 15u), Status::malformed);
  bytes[0] ^= 1u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes), Status::malformed);
  CHECK_STATUS(read.decode(nullptr, sizeof bytes), Status::malformed);
  CHECK(read.luid() == 9u);
  CHECK(!read.offers(1u)); }

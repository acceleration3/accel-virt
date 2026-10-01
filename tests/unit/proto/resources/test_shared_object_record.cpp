/* The record that carries a shared object's identity between two processes.
 * Every process on the far side of it is a process this one has no control
 * over, so the layout is pinned byte for byte and every field the bytes alone
 * can judge is judged here - the miniport's table then judges what only it
 * knows, which is whether the id was ever minted. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::SharedObjectKind;
using av::SharedObjectRecord;
using av::Status;

namespace {

constexpr std::uint64_t page = SharedObjectRecord::page_size;
constexpr std::uint64_t adapter = 0x0000000100002b71u;

SharedObjectRecord valid()
{
    return SharedObjectRecord(0x0123456789ABCDEFu, SharedObjectKind(SharedObjectKind::memory), adapter, 8u * page);
}

void encode(const SharedObjectRecord &record, std::uint8_t *bytes)
{
    CHECK_STATUS(record.encode(bytes, SharedObjectRecord::encoded_size), Status::ok);
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    SharedObjectRecord record;
    const Status       status = record.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[SharedObjectRecord::encoded_size] = {};
        if (record.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(shared_object_record_encode, lays_out_the_magic_version_and_every_field_with_the_reserved_word_zero)
{ std::uint8_t bytes[SharedObjectRecord::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  encode(valid(), bytes);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x00535641u);                 /* "AVS" */
  CHECK(layout.at(4u, 4u) == 1u);
  CHECK(layout.at(8u, 8u) == 0x0123456789ABCDEFu);
  CHECK(layout.at(16u, 4u) == SharedObjectKind::memory);
  CHECK(layout.at(20u, 4u) == 0u);
  CHECK(layout.at(24u, 8u) == adapter);
  CHECK(layout.at(32u, 8u) == 8u * page); }

TEST(shared_object_record_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(SharedObjectRecord::encoded_size - 1u);
  CHECK_STATUS(valid().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(shared_object_record_decode, round_trips_every_field_of_a_record_a_creator_wrote)
{ std::uint8_t bytes[SharedObjectRecord::encoded_size] = {};
  encode(valid(), bytes);
  SharedObjectRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.object_id(), 0x0123456789ABCDEFu);
  CHECK_EQ(decoded.kind().value(), SharedObjectKind::memory);
  CHECK_EQ(decoded.adapter_id(), adapter);
  CHECK_EQ(decoded.size(), 8u * page);
  CHECK(decoded.assigned()); }

TEST(shared_object_record_decode, an_id_of_zero_is_a_creator_asking_for_one_and_is_not_assigned)
{ std::uint8_t bytes[SharedObjectRecord::encoded_size] = {};
  encode(SharedObjectRecord(0u, SharedObjectKind(SharedObjectKind::native_fence), 0u, 0u), bytes);
  SharedObjectRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.object_id(), 0u);
  CHECK(!decoded.assigned());
  CHECK_EQ(decoded.kind().value(), SharedObjectKind::native_fence); }

TEST(shared_object_record_assigned, needs_both_halves_because_an_id_without_an_adapter_names_nobody)
{ CHECK(!SharedObjectRecord(0u, SharedObjectKind(SharedObjectKind::memory), 0u, page).assigned());
  CHECK(!SharedObjectRecord(0u, SharedObjectKind(SharedObjectKind::memory), adapter, page).assigned());
  CHECK(!SharedObjectRecord(7u, SharedObjectKind(SharedObjectKind::memory), 0u, page).assigned());
  CHECK(SharedObjectRecord(7u, SharedObjectKind(SharedObjectKind::memory), adapter, page).assigned()); }

TEST(shared_object_record, an_assigned_id_and_adapter_are_what_the_second_process_reads_back)
{ SharedObjectRecord record(0u, SharedObjectKind(SharedObjectKind::memory), 0u, page);
  CHECK(!record.assigned());
  record.assign(9u, adapter);
  CHECK(record.assigned());
  std::uint8_t bytes[SharedObjectRecord::encoded_size] = {};
  encode(record, bytes);
  SharedObjectRecord read_back;
  CHECK_STATUS(read_back.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(read_back.object_id(), 9u);
  CHECK_EQ(read_back.adapter_id(), adapter); }

TEST(shared_object_record_decode, any_length_but_its_own_is_framing_and_leaves_the_record_alone)
{ std::uint8_t bytes[SharedObjectRecord::encoded_size + 8u] = {};
  encode(valid(), bytes);
  const std::size_t lengths[] = {0u, 1u, 4u, 8u, 16u, 24u, 32u, 36u, 39u, 41u, 48u};
  for (const std::size_t length : lengths) {
      SharedObjectRecord decoded(7u, SharedObjectKind(SharedObjectKind::monitored_fence), 3u, page);
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
      CHECK_EQ(decoded.object_id(), 7u);
      CHECK_EQ(decoded.kind().value(), SharedObjectKind::monitored_fence);
  } }

TEST(shared_object_record_decode, a_field_no_record_can_have_is_an_invalid_parameter_left_unread)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {
      {0u, 4u, 0u},                        /* no magic */
      {0u, 4u, 0x534B5653u},               /* one bit off the magic */
      {0u, 4u, 0xFFFFFFFFu},
      {4u, 4u, 0u},                        /* version 0 */
      {4u, 4u, 2u},                        /* a version this build does not know */
      {4u, 4u, 0xFFFFFFFFu},
      {16u, 4u, 0u},                       /* no kind */
      {16u, 4u, 4u},                       /* the kind after the last one */
      {16u, 4u, 0xFFFFFFFFu},
      {20u, 4u, 1u},                       /* reserved */
      {20u, 4u, 0x80000000u},
      {24u, 8u, 0u},                       /* an id with no adapter */
      {8u, 8u, 0u},                        /* an adapter with no id */
      {32u, 8u, 0u},                       /* no size */
      {32u, 8u, 1u},                       /* a size that is not whole pages */
      {32u, 8u, page - 1u},
      {32u, 8u, page + 1u},
      {32u, 8u, 0xFFFFFFFFFFFFFFFFu},
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[SharedObjectRecord::encoded_size] = {};
      encode(valid(), bytes);
      std::memcpy(bytes + c.offset, &c.value, c.width);
      SharedObjectRecord decoded(7u, SharedObjectKind(SharedObjectKind::monitored_fence), 3u, page);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::invalid_parameter);
      CHECK_EQ(decoded.object_id(), 7u);
      CHECK_EQ(decoded.size(), page);
  } }

TEST(shared_object_record_decode, the_widest_representable_record_round_trips)
{ std::uint8_t bytes[SharedObjectRecord::encoded_size] = {};
  const std::uint64_t size = 0xFFFFFFFFFFFFF000u;
  encode(SharedObjectRecord(0xFFFFFFFFFFFFFFFFu, SharedObjectKind(SharedObjectKind::memory),
                            0xFFFFFFFFFFFFFFFFu, size),
         bytes);
  SharedObjectRecord decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(decoded.object_id(), 0xFFFFFFFFFFFFFFFFu);
  CHECK_EQ(decoded.adapter_id(), 0xFFFFFFFFFFFFFFFFu);
  CHECK_EQ(decoded.size(), size); }

/* A fence has no size, exactly as the wire body says: the two describe one
 * object and an opener compares them, so a rule either one kept alone would be
 * a record no open could match (feature 053, T031). */
TEST(shared_object_record_decode, a_fence_carries_no_size_and_memory_carries_pages)
{ std::uint8_t bytes[SharedObjectRecord::encoded_size] = {};
  const std::uint32_t fences[] = {SharedObjectKind::monitored_fence, SharedObjectKind::native_fence};
  for (const std::uint32_t kind : fences) {
      encode(SharedObjectRecord(7u, SharedObjectKind(kind), 9u, 0u), bytes);
      SharedObjectRecord decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
      CHECK_EQ(decoded.size(), 0u);
      encode(SharedObjectRecord(7u, SharedObjectKind(kind), 9u, page), bytes);
      SharedObjectRecord refused;
      CHECK_STATUS(refused.decode(bytes, sizeof bytes), Status::invalid_parameter);
  }
  encode(SharedObjectRecord(7u, SharedObjectKind(SharedObjectKind::memory), 9u, 0u), bytes);
  SharedObjectRecord no_size;
  CHECK_STATUS(no_size.decode(bytes, sizeof bytes), Status::invalid_parameter); }

TEST(shared_object_record_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[SharedObjectRecord::encoded_size] = {};
  encode(valid(), bytes);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 320u);
  /* The same free fields as the request - 64 bits of object id and 51 of the
   * eight-page size - plus 64 of adapter id, which travels with the object id
   * and so need only stay non-zero, as no single flip of 0x100002b71 makes it.
   * Nothing else is free: the magic and the version are compared whole, so all
   * 64 of their bits are fixed; the kind's 32 flips leave an unknown kind or a
   * fence carrying a size; the four reserved bytes must be zero; and all-zero,
   * all-ones and every truncation fail on the magic or the length.
   *
   * The count is exact, not a bound: it was checked as an equality before
   * being written as a floor, so a decoder that starts refusing one of these
   * has changed its format. */
  const std::uint32_t acceptable = 64u + 64u + 51u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

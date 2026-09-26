/* The guest's write side of a context's ring. It writes a fresh header at setup,
 * frames each batch at the tail wrapping the data area exactly as the device's
 * reader does, leaves the header's tail for the miniport to publish, and
 * refuses a batch there is no room for
 * until the device's head frees it. The device's head, which the test writes to
 * play the device, is what bounds how far ahead the tail may run. */
#include "core/support/status.hpp"
#include "guest/transport/transport_ring.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::BatchHeader;
using av::RingHeader;
using av::Status;
using av::TransportRing;
using av::WireWriter;
using av::TransportRingProbe;

namespace av {

struct TransportRingProbe {
    /* States the class never builds: an unbound ring with a nonzero counter,
     * and a bound ring whose tail has fallen behind or run past head. */
    static void dirty_unbound(TransportRing &ring) { ring.tail_ = 5u; }
    static void dirty_order(TransportRing &ring) { ring.order_ = 5u; }
    static void tail_behind_head(TransportRing &ring) { ring.tail_ = 0u; }  /* with head > 0 */
    static void tail_past_capacity(TransportRing &ring) { ring.tail_ = ring.capacity() + 1u; }
};

}  // namespace av

namespace {

constexpr std::uint8_t order = 12u;                 /* 4096-byte data area */
constexpr std::uint64_t data_bytes = 1u << order;
constexpr std::size_t   ring_bytes = 32u + data_bytes;

struct Ring {
    std::uint8_t bytes[ring_bytes] = {};
    std::uint8_t *data() { return bytes + 32u; }
    /* Play the device: mark `head` bytes consumed. */
    void consume_to(std::uint64_t head)
    { WireWriter(bytes + 16u, 8u).write_u64(head); }
    /* Read `count` bytes from `counter`, wrapping, into `out`. */
    void read_wrapped(std::uint64_t counter, std::uint8_t *out, std::uint32_t count)
    { for (std::uint32_t i = 0u; i < count; i++) out[i] = data()[(counter + i) & (data_bytes - 1u)]; }
    BatchHeader batch_at(std::uint64_t counter)
    { std::uint8_t framing[BatchHeader::encoded_size] = {};
      read_wrapped(counter, framing, sizeof framing);
      BatchHeader header;
      static_cast<void>(header.decode(framing, sizeof framing));
      return header; }
};

}  // namespace

/* Who publishes the tail (feature 049). On the kernel path the miniport does
 * it, once it has checked the submission - so the header's tail stays 0 however
 * many batches the process wrote. A process that rings its own doorbell has no
 * kernel in the way and publishes its own, which is the one write that makes
 * its batches visible to the device. */
TEST(transport_ring, publish_tail_is_what_a_process_that_rings_its_own_doorbell_writes)
{ Ring          memory;
  TransportRing ring;
  /* Unbound, there is nothing to publish into. */
  CHECK_STATUS(ring.publish_tail(), Status::invalid_state);
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  std::uint8_t payload[64] = {};
  std::uint64_t begin = 0u;
  std::uint64_t end = 0u;
  CHECK_STATUS(ring.write_batch(payload, sizeof payload, 1u, 0u, begin, end), Status::ok);
  /* Written, not published: this is the kernel path, and the header still says
   * the device has been told of nothing. */
  RingHeader header;
  CHECK_STATUS(header.decode(memory.bytes, ring_bytes), Status::ok);
  CHECK_EQ(header.tail(), 0u);
  CHECK(ring.tail() > 0u);

  CHECK_STATUS(ring.publish_tail(), Status::ok);
  CHECK_STATUS(header.decode(memory.bytes, ring_bytes), Status::ok);
  CHECK_EQ(header.tail(), ring.tail());
  /* And publishing twice says the same thing: it is the counter, not an
   * increment. */
  CHECK_STATUS(ring.publish_tail(), Status::ok);
  CHECK_STATUS(header.decode(memory.bytes, ring_bytes), Status::ok);
  CHECK_EQ(header.tail(), ring.tail());
  CHECK(ring.invariants_hold()); }

TEST(transport_ring, is_unbound_until_initialized)
{ TransportRing ring;
  CHECK(ring.invariants_hold());
  std::uint64_t begin = 0u, end = 0u;
  const std::uint8_t payload[8] = {};
  CHECK_STATUS(ring.write_batch(payload, sizeof payload, 1u, 0u, begin, end), Status::invalid_state); }

TEST(transport_ring, initialize_writes_a_fresh_header)
{ Ring memory;
  TransportRing ring;
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  RingHeader header;
  CHECK_STATUS(header.decode(memory.bytes, 32u), Status::ok);
  CHECK_EQ(header.order(), order);
  CHECK_EQ(header.tail(), 0u);
  CHECK_EQ(header.head(), 0u);
  CHECK_EQ(ring.capacity(), data_bytes);
  CHECK_EQ(ring.free_space(), data_bytes);
  /* Binding a second time is refused. */
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::invalid_state); }

TEST(transport_ring, writes_a_batch_and_leaves_publishing_the_tail_to_the_miniport)
{ Ring memory;
  TransportRing ring;
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  const std::uint8_t payload[] = {0xDEu, 0xADu, 0xBEu, 0xEFu, 0x01u, 0x02u, 0x03u, 0x04u};
  std::uint64_t begin = 9u, end = 9u;
  CHECK_STATUS(ring.write_batch(payload, sizeof payload, 7u, 0x123456789ull, begin, end), Status::ok);
  CHECK_EQ(begin, 0u);
  CHECK_EQ(end, 40u);          /* span = round8(32 + 8) */
  CHECK_EQ(ring.tail(), 40u);
  /* The header's tail stays where it was: a device woken before the miniport
   * checked the submission must find nothing to consume. The batch decodes
   * with its fence. */
  RingHeader header;
  CHECK_STATUS(header.decode(memory.bytes, 32u), Status::ok);
  CHECK_EQ(header.tail(), 0u);
  const BatchHeader batch = memory.batch_at(0u);
  CHECK_EQ(batch.length(), 8u);
  CHECK_EQ(batch.fence_id(), 7u);
  CHECK_EQ(batch.published_at(), 0x123456789ull);
  std::uint8_t seen[8] = {};
  memory.read_wrapped(BatchHeader::encoded_size, seen, sizeof seen);
  for (std::size_t i = 0u; i < sizeof seen; i++) CHECK_EQ(seen[i], payload[i]); }

TEST(transport_ring, batches_follow_one_another)
{ Ring memory;
  TransportRing ring;
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  const std::uint8_t payload[16] = {};
  std::uint64_t begin = 0u, end = 0u;
  CHECK_STATUS(ring.write_batch(payload, sizeof payload, 1u, 0u, begin, end), Status::ok);
  CHECK_EQ(begin, 0u);
  CHECK_EQ(end, 48u);          /* round8(32+16) */
  CHECK_STATUS(ring.write_batch(payload, sizeof payload, 2u, 0u, begin, end), Status::ok);
  CHECK_EQ(begin, 48u);
  CHECK_EQ(end, 96u);
  CHECK_EQ(memory.batch_at(48u).fence_id(), 2u); }

/* A batch written near the end wraps the data area, header and payload split
 * across the boundary, exactly as the device reads it. */
TEST(transport_ring, a_batch_wraps_the_data_area)
{ Ring memory;
  TransportRing ring;
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  /* Fill to 4056 payload -> span 4088, then let the device consume it. */
  std::uint8_t big[4056] = {};
  std::uint64_t begin = 0u, end = 0u;
  CHECK_STATUS(ring.write_batch(big, sizeof big, 1u, 0u, begin, end), Status::ok);
  CHECK_EQ(end, 4088u);
  memory.consume_to(4088u);
  /* A batch at 4088 whose 32-byte header wraps at 4096. */
  const std::uint8_t payload[] = {0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu, 0x11u, 0x22u};
  CHECK_STATUS(ring.write_batch(payload, sizeof payload, 2u, 0u, begin, end), Status::ok);
  CHECK_EQ(begin, 4088u);
  CHECK_EQ(end, 4088u + 40u);
  const BatchHeader wrapped = memory.batch_at(4088u);
  CHECK_EQ(wrapped.fence_id(), 2u);
  CHECK_EQ(wrapped.length(), 8u);
  std::uint8_t seen[8] = {};
  memory.read_wrapped(4088u + BatchHeader::encoded_size, seen, sizeof seen);
  for (std::size_t i = 0u; i < sizeof seen; i++) CHECK_EQ(seen[i], payload[i]); }

TEST(transport_ring, refuses_a_batch_with_no_room_until_the_device_consumes)
{ Ring memory;
  TransportRing ring;
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  std::uint8_t big[4056] = {};      /* span 4088 */
  std::uint64_t begin = 0u, end = 0u;
  CHECK_STATUS(ring.write_batch(big, sizeof big, 1u, 0u, begin, end), Status::ok);
  /* 8 bytes free, a 40-byte batch does not fit. */
  const std::uint8_t small[8] = {};
  CHECK_STATUS(ring.write_batch(small, sizeof small, 2u, 0u, begin, end), Status::no_space);
  memory.consume_to(4000u);
  CHECK_STATUS(ring.write_batch(small, sizeof small, 2u, 0u, begin, end), Status::ok); }

TEST(transport_ring, refuses_fence_zero_a_null_payload_and_a_batch_too_large)
{ Ring memory;
  TransportRing ring;
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  std::uint64_t begin = 0u, end = 0u;
  const std::uint8_t payload[8] = {};
  CHECK_STATUS(ring.write_batch(payload, sizeof payload, 0u, 0u, begin, end), Status::invalid_parameter);
  CHECK_STATUS(ring.write_batch(nullptr, 8u, 1u, 0u, begin, end), Status::invalid_parameter);
  /* A payload that cannot fit the ring even empty of everything else. */
  std::uint8_t huge[data_bytes] = {};
  CHECK_STATUS(ring.write_batch(huge, sizeof huge, 1u, 0u, begin, end), Status::invalid_parameter);
  CHECK(ring.invariants_hold()); }

TEST(transport_ring, refuses_to_initialize_bad_arguments)
{ Ring memory;
  TransportRing a;
  CHECK_STATUS(a.initialize(nullptr, order), Status::invalid_parameter);
  TransportRing b;
  CHECK_STATUS(b.initialize(memory.bytes, 11u), Status::invalid_parameter);
  TransportRing c;
  CHECK_STATUS(c.initialize(memory.bytes, 25u), Status::invalid_parameter); }

/* The accessors answer for an unbound ring without reading memory. */
TEST(transport_ring, unbound_accessors_are_empty)
{ TransportRing ring;
  CHECK_EQ(ring.capacity(), 0u);
  CHECK_EQ(ring.head(), 0u);
  CHECK_EQ(ring.tail(), 0u);
  CHECK_EQ(ring.free_space(), 0u); }

/* The invariant catches states the class never builds. */
TEST(transport_ring, the_invariant_catches_impossible_states)
{ TransportRing unbound;
  TransportRingProbe::dirty_unbound(unbound);
  CHECK(!unbound.invariants_hold());
  TransportRing odd_order;
  TransportRingProbe::dirty_order(odd_order);
  CHECK(!odd_order.invariants_hold());
  Ring memory;
  TransportRing behind;
  CHECK_STATUS(behind.initialize(memory.bytes, order), Status::ok);
  memory.consume_to(64u);                 /* head ahead of a tail left at 0 */
  TransportRingProbe::tail_behind_head(behind);
  CHECK(!behind.invariants_hold());
  Ring other;
  TransportRing past;
  CHECK_STATUS(past.initialize(other.bytes, order), Status::ok);
  TransportRingProbe::tail_past_capacity(past);
  CHECK(!past.invariants_hold()); }

namespace {

/* Where the device's worker says it is watching the ring: the header's byte
 * after the order (RingHeader), which a fresh header leaves clear. */
constexpr std::size_t awake_byte = 5u;

}  // namespace

TEST(transport_ring, the_consumer_is_awake_only_while_the_device_says_so)
{ Ring memory;
  TransportRing ring;
  /* Unbound, there is no worker to be awake. */
  memory.bytes[awake_byte] = 1u;
  CHECK(!ring.consumer_awake());
  CHECK_STATUS(ring.initialize(memory.bytes, order), Status::ok);
  CHECK(!ring.consumer_awake());
  memory.bytes[awake_byte] = 1u;
  CHECK(ring.consumer_awake());
  /* Any value but zero: the flag is the device's to write. */
  memory.bytes[awake_byte] = 0x80u;
  CHECK(ring.consumer_awake());
  memory.bytes[awake_byte] = 0u;
  CHECK(!ring.consumer_awake());
  /* The neighbouring bytes are not the flag. */
  memory.bytes[awake_byte - 1u] = 0xFFu;
  memory.bytes[awake_byte + 1u] = 0xFFu;
  CHECK(!ring.consumer_awake()); }

#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AllocationDescriptor;
using av::AllocationKind;
using av::ByteLayout;
using av::GuardedBuffer;
using av::HostileSweep;
using av::ScanoutTarget;
using av::Status;
using av::SurfaceLayout;

namespace {

const AllocationDescriptor primary(AllocationKind::shared_primary, true, SurfaceLayout(1024u, 768u, 21u, 4096u),
                                   4096u * 768u, ScanoutTarget(0u, 60u, 1u));

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    AllocationDescriptor descriptor;
    const Status status = descriptor.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[AllocationDescriptor::encoded_size] = {};
        if (descriptor.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

std::uint8_t *encoded(const AllocationDescriptor &descriptor, std::uint8_t *bytes)
{
    if (descriptor.encode(bytes, AllocationDescriptor::encoded_size) != Status::ok)
        av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "encode failed");
    return bytes;
}

}  // namespace

TEST(allocation_descriptor_encode, places_every_field_at_its_offset)
{ GuardedBuffer bytes(AllocationDescriptor::encoded_size);
  CHECK_STATUS(primary.encode(bytes.data(), AllocationDescriptor::encoded_size), Status::ok);
  const ByteLayout layout(bytes.data(), AllocationDescriptor::encoded_size);
  CHECK(layout.at(0u, 4u) == 0x414B5652u);
  CHECK(layout.at(4u, 2u) == 1u);
  CHECK(layout.at(6u, 2u) == 1u);
  CHECK(layout.at(8u, 4u) == 1024u);
  CHECK(layout.at(12u, 4u) == 768u);
  CHECK(layout.at(16u, 4u) == 21u);
  CHECK(layout.at(20u, 4u) == 4096u);
  CHECK(layout.at(24u, 8u) == 4096u * 768u);
  CHECK(layout.at(32u, 4u) == 60u);
  CHECK(layout.at(36u, 4u) == 1u);
  CHECK(layout.at(40u, 4u) == 0u);
  CHECK(layout.at(44u, 4u) == 0u);
  CHECK(bytes.guards_intact()); }

TEST(allocation_descriptor_encode, refuses_a_short_destination)
{ std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
  CHECK_STATUS(primary.encode(bytes, sizeof bytes - 1u), Status::too_large);
  CHECK_EQ(bytes[0], 0u); }

TEST(allocation_descriptor_decode, round_trips_every_kind)
{ const AllocationDescriptor descriptors[] = {
      primary,
      AllocationDescriptor(AllocationKind::shadow, true, SurfaceLayout(8u, 8u, 22u, 32u), 256u, ScanoutTarget()),
      AllocationDescriptor(AllocationKind::staging, true, SurfaceLayout(8u, 8u, 21u, 32u), 256u, ScanoutTarget()),
      AllocationDescriptor(AllocationKind::gdi, false, SurfaceLayout(8u, 8u, 21u, 32u), 256u, ScanoutTarget()),
      AllocationDescriptor(AllocationKind::client, false, SurfaceLayout(), 1u, ScanoutTarget()),
      AllocationDescriptor(AllocationKind::context_ring, true, SurfaceLayout(), 0x4000u, ScanoutTarget()),
      AllocationDescriptor(AllocationKind::fence_page, true, SurfaceLayout(), 0x1000u, ScanoutTarget()),
  };
  for (const AllocationDescriptor &descriptor : descriptors) {
      std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
      AllocationDescriptor decoded;
      CHECK_STATUS(decoded.decode(encoded(descriptor, bytes), sizeof bytes), Status::ok);
      CHECK(decoded.kind() == descriptor.kind());
      CHECK(decoded.cpu_visible() == descriptor.cpu_visible());
      CHECK_EQ(decoded.layout().pitch(), descriptor.layout().pitch());
      CHECK(decoded.size() == descriptor.size());
      CHECK_EQ(decoded.scanout().refresh_numerator(), descriptor.scanout().refresh_numerator());
  } }

TEST(allocation_descriptor_decode, rejects_each_invalid_field_and_stays_unchanged)
{ struct Case { std::size_t offset; std::size_t width; std::uint64_t value; };
  const Case cases[] = {
      {0u, 4u, 0x414B5653u},       /* magic */
      {4u, 2u, 0u},                /* no kind */
      {4u, 2u, 5u},                /* unknown kind */
      {6u, 2u, 2u},                /* reserved flag */
      {8u, 4u, 4097u},             /* pitch below width */
      {12u, 4u, 769u},             /* rows past the size */
      {20u, 4u, 0u},               /* surface without pitch */
      {24u, 8u, 0u},               /* empty */
      {44u, 4u, 1u},               /* reserved */
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
      encoded(primary, bytes);
      for (std::size_t i = 0u; i < c.width; i++) bytes[c.offset + i] = static_cast<std::uint8_t>(c.value >> (8u * i));
      AllocationDescriptor decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(decoded.kind() == AllocationKind::client);
      CHECK(decoded.size() == 0u);
  } }

TEST(allocation_descriptor_decode, only_a_primary_names_a_scanout)
{ std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
  const AllocationDescriptor shadow(AllocationKind::shadow, true, SurfaceLayout(8u, 8u, 21u, 32u), 256u,
                                    ScanoutTarget(0u, 60u, 1u));
  AllocationDescriptor decoded;
  CHECK_STATUS(decoded.decode(encoded(shadow, bytes), sizeof bytes), Status::malformed); }

TEST(allocation_descriptor_decode, a_client_buffer_may_have_no_layout_but_a_surface_may_not)
{ std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
  AllocationDescriptor decoded;
  const AllocationDescriptor gdi(AllocationKind::gdi, false, SurfaceLayout(), 256u, ScanoutTarget());
  CHECK_STATUS(decoded.decode(encoded(gdi, bytes), sizeof bytes), Status::malformed);
  const AllocationDescriptor client(AllocationKind::client, true, SurfaceLayout(4u, 4u, 21u, 16u), 64u, ScanoutTarget());
  CHECK_STATUS(decoded.decode(encoded(client, bytes), sizeof bytes), Status::ok); }

TEST(allocation_descriptor_decode, short_input_is_malformed)
{ std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
  AllocationDescriptor decoded;
  CHECK_STATUS(decoded.decode(encoded(primary, bytes), sizeof bytes - 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, sizeof bytes), Status::malformed); }

TEST(allocation_descriptor_decode, hostile_sweep_never_accepts_what_does_not_round_trip)
{ std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
  const HostileSweep sweep(rejects, encoded(primary, bytes), sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 8u * AllocationDescriptor::encoded_size); }

/* Feature 041, US2: the pages an rvk context runs on. A ring the device reads
 * with a power-of-two mask, and the single page its fences are written to - the
 * device's own decoder refuses anything else, so a descriptor asking for it is
 * refused here first. */
TEST(allocation_descriptor_decode, a_ring_is_a_power_of_two_of_pages_and_a_fence_page_is_one)
{ struct Case { AllocationKind kind; std::uint64_t size; bool surface; Status expected; };
  const Case cases[] = {
      {AllocationKind::context_ring, 0x1000u, false, Status::ok},
      {AllocationKind::context_ring, 0x4000u, false, Status::ok},
      {AllocationKind::context_ring, 0x3000u, false, Status::malformed},   /* not a power of two */
      {AllocationKind::context_ring, 0x800u, false, Status::malformed},    /* under a page */
      {AllocationKind::context_ring, 0x4000u, true, Status::malformed},    /* not a surface */
      {AllocationKind::fence_page, 0x1000u, false, Status::ok},
      {AllocationKind::fence_page, 0x2000u, false, Status::malformed},     /* one page, exactly */
      {AllocationKind::fence_page, 0x1000u, true, Status::malformed},
      {AllocationKind::reply_slot, 0x1000u, false, Status::ok},
      {AllocationKind::reply_slot, 0x3000u, false, Status::ok},
      {AllocationKind::reply_slot, 0x1800u, false, Status::malformed},     /* whole pages */
      {AllocationKind::reply_slot, 0x1000u, true, Status::malformed},
      /* Feature 045: a host-visible chunk is any whole number of pages, never a surface. */
      {AllocationKind::host_visible_memory, 0x1000u, false, Status::ok},
      {AllocationKind::host_visible_memory, 0x4000000u, false, Status::ok},
      {AllocationKind::host_visible_memory, 0x4000001u, false, Status::malformed},
      {AllocationKind::host_visible_memory, 0x1000u, true, Status::malformed},
  };
  for (const Case &one : cases) {
      const SurfaceLayout layout = one.surface ? SurfaceLayout(8u, 8u, 21u, 32u) : SurfaceLayout();
      const AllocationDescriptor descriptor(one.kind, true, layout, one.size, ScanoutTarget());
      std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
      AllocationDescriptor decoded;
      CHECK_STATUS(decoded.decode(encoded(descriptor, bytes), sizeof bytes), one.expected);
  } }

/* A kind outside the enumeration is refused, whatever else the descriptor
 * says: a guest that invents one is asking for memory this driver has no rules
 * for. */
TEST(allocation_descriptor_decode, a_kind_past_the_last_one_is_refused)
{ const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
  static_cast<void>(encoded(ring, bytes));
  const std::uint16_t past = 21u;
  std::memcpy(bytes + 4u, &past, sizeof past);
  AllocationDescriptor decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed); }

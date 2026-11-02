#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::FencePage;
using av::GuardedBuffer;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    FencePage page;
    const Status status = page.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[FencePage::encoded_size] = {};
        if (page.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}

TEST(fence_page_encode, places_every_field_at_its_offset)
{ std::uint8_t bytes[FencePage::encoded_size] = {};
  CHECK_STATUS(FencePage(41u, 900u, 0xDEADu).raising(77u).beating(0x1234u).encode(bytes, sizeof bytes), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0x00465641u);
  CHECK(layout.at(4u, 4u) == 1u);
  CHECK(layout.at(8u, 8u) == 41u);
  CHECK(layout.at(16u, 8u) == 900u);
  CHECK(layout.at(24u, 4u) == 0xDEADu);
  CHECK(layout.at(28u, 4u) == 0u);
  CHECK(layout.at(32u, 8u) == 77u);
  CHECK(layout.at(FencePage::beat_at, 8u) == 0x1234u);
  CHECK_EQ(FencePage::beat_at + 8u, FencePage::encoded_size); }

TEST(fence_page, round_trips_healthy_and_faulted_pages)
{ std::uint8_t bytes[FencePage::encoded_size] = {};
  CHECK_STATUS(FencePage(12u, 30u).raising(4u).beating(9u).encode(bytes, sizeof bytes), Status::ok);
  FencePage decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(!decoded.faulted());
  CHECK(decoded.completed_fence() == 12u);
  CHECK(decoded.progress() == 30u);
  CHECK(decoded.raised() == 4u);
  CHECK(decoded.beat() == 9u);
  CHECK_EQ(decoded.fault_code(), 0u);
  CHECK_STATUS(FencePage(13u, 31u, 7u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.faulted());
  CHECK_EQ(decoded.fault_code(), 7u);
  CHECK(decoded.beat() == 0u); }

TEST(fence_page_decode, rejects_each_malformed_field)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {
      {0u, 0u},
      {4u, 2u},
      {24u, 1u},
      {29u, 1u},
  };
  for (const Case &c : cases) {
      std::uint8_t bytes[FencePage::encoded_size] = {};
      CHECK_STATUS(FencePage(5u, 5u).encode(bytes, sizeof bytes), Status::ok);
      bytes[c.offset] = c.value;
      FencePage decoded(99u, 99u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK(decoded.completed_fence() == 99u);
  } }

TEST(fence_page_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(FencePage::encoded_size - 1u);
  CHECK_STATUS(FencePage(1u, 1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(fence_page_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[FencePage::encoded_size] = {};
  CHECK_STATUS(FencePage(3u, 4u, 5u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 256u);
  const std::uint32_t acceptable = 256u + 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

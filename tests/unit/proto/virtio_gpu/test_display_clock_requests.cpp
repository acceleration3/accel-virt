/* DISPLAY_CLOCK and DISPLAY_TIMING as the driver writes them and the device
 * reads them: the header, then the page's address, or the head and its rate.
 * A page address that is not a page's, a rate outside 1 Hz to 1 kHz, or a
 * request too short for its fields decodes to nothing. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/display_clock_request.hpp"
#include "proto/virtio_gpu/display_timing_request.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::DisplayClockRequest;
using av::DisplayTimingRequest;
using av::RvkRequestWriter;
using av::Status;
using av::WireReader;

namespace {

std::uint32_t type_of(const std::uint8_t *bytes)
{
    std::uint32_t type = 0u;
    std::memcpy(&type, bytes, sizeof type);
    return type;
}

}  // namespace

TEST(display_clock_request, the_writer_s_request_decodes_to_its_page)
{ std::uint8_t bytes[RvkRequestWriter::max_request_size] = {};
  RvkRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0x1234567000u), Status::ok);
  CHECK_EQ(writer.length(), DisplayClockRequest::encoded_size);
  CHECK_EQ(type_of(bytes), DisplayClockRequest::type);
  WireReader reader(bytes, writer.length());
  DisplayClockRequest clock;
  CHECK_STATUS(clock.decode(reader), Status::ok);
  CHECK_EQ(clock.address(), 0x1234567000u);
  CHECK_EQ(reader.offset(), DisplayClockRequest::encoded_size); }

TEST(display_clock_request, no_page_is_a_request_to_stop)
{ std::uint8_t bytes[RvkRequestWriter::max_request_size] = {};
  RvkRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0u), Status::ok);
  WireReader reader(bytes, writer.length());
  DisplayClockRequest clock;
  CHECK_STATUS(clock.decode(reader), Status::ok);
  CHECK_EQ(clock.address(), 0u); }

TEST(display_clock_request, an_address_off_a_page_or_a_short_request_is_malformed)
{ std::uint8_t bytes[RvkRequestWriter::max_request_size] = {};
  RvkRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0x5000u), Status::ok);
  WireReader short_reader(bytes, DisplayClockRequest::encoded_size - 1u);
  DisplayClockRequest short_clock;
  CHECK_STATUS(short_clock.decode(short_reader), Status::malformed);
  CHECK_EQ(short_clock.address(), 0u);
  const std::uint64_t off_page[] = {0x5001u, 0x5800u, 0x5FFFu};
  for (const std::uint64_t address : off_page) {
      CHECK_STATUS(writer.display_clock(address), Status::ok);
      WireReader reader(bytes, writer.length());
      DisplayClockRequest clock;
      CHECK_STATUS(clock.decode(reader), Status::malformed);
      CHECK_EQ(clock.address(), 0u);
  }
  bytes[31] = 0x80u;   /* the address's top byte: the whole u64 is read */
  bytes[24] = 0u;
  bytes[25] = 0u;
  WireReader high_reader(bytes, DisplayClockRequest::encoded_size);
  DisplayClockRequest high;
  CHECK_STATUS(high.decode(high_reader), Status::ok);
  CHECK_EQ(high.address() >> 56u, 0x80u); }

TEST(display_timing_request, the_writer_s_request_decodes_to_its_head_and_rate)
{ std::uint8_t bytes[RvkRequestWriter::max_request_size] = {};
  RvkRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_timing(3u, 143998u), Status::ok);
  CHECK_EQ(writer.length(), DisplayTimingRequest::encoded_size);
  CHECK_EQ(type_of(bytes), DisplayTimingRequest::type);
  WireReader reader(bytes, writer.length());
  DisplayTimingRequest timing;
  CHECK_STATUS(timing.decode(reader), Status::ok);
  CHECK_EQ(timing.head(), 3u);
  CHECK_EQ(timing.refresh_millihz(), 143998u);
  CHECK_EQ(reader.offset(), DisplayTimingRequest::encoded_size); }

TEST(display_timing_request, the_rate_s_bounds_are_inclusive_and_past_them_is_malformed)
{ std::uint8_t bytes[RvkRequestWriter::max_request_size] = {};
  RvkRequestWriter writer(bytes, sizeof bytes);
  struct Case {
      std::uint32_t millihz;
      Status        expected;
  };
  const Case cases[] = {{0u, Status::malformed},        {999u, Status::malformed},
                        {1000u, Status::ok},            {60000u, Status::ok},
                        {1000000u, Status::ok},         {1000001u, Status::malformed},
                        {0xFFFFFFFFu, Status::malformed}};
  for (const Case &item : cases) {
      CHECK_STATUS(writer.display_timing(0u, item.millihz), Status::ok);
      WireReader reader(bytes, writer.length());
      DisplayTimingRequest timing;
      CHECK_STATUS(timing.decode(reader), item.expected);
      CHECK_EQ(timing.refresh_millihz(), item.expected == Status::ok ? item.millihz : 0u);
  }
  CHECK_STATUS(writer.display_timing(1u, 60000u), Status::ok);
  WireReader short_reader(bytes, DisplayTimingRequest::encoded_size - 1u);
  DisplayTimingRequest short_timing;
  CHECK_STATUS(short_timing.decode(short_reader), Status::malformed);
  CHECK_EQ(short_timing.head(), 0u); }

TEST(display_clock_request, a_writer_too_small_writes_neither)
{ std::uint8_t bytes[DisplayClockRequest::encoded_size - 1u] = {};
  RvkRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0x1000u), Status::too_large);
  CHECK_EQ(writer.length(), 0u);
  CHECK_STATUS(writer.display_timing(0u, 60000u), Status::too_large);
  CHECK_EQ(writer.length(), 0u); }

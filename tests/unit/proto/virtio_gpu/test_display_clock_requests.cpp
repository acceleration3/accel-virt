#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/page_request.hpp"
#include "proto/virtio_gpu/display_timing_request.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::PageRequest;
using av::DisplayTimingRequest;
using av::AvRequestWriter;
using av::Status;
using av::WireReader;

namespace {

std::uint32_t type_of(const std::uint8_t *bytes)
{
    std::uint32_t type = 0u;
    std::memcpy(&type, bytes, sizeof type);
    return type;
}

}

TEST(display_clock_request, the_writer_s_request_decodes_to_its_page)
{ std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0x1234567000u), Status::ok);
  CHECK_EQ(writer.length(), PageRequest::encoded_size);
  CHECK_EQ(type_of(bytes), PageRequest::display_clock);
  WireReader reader(bytes, writer.length());
  PageRequest clock;
  CHECK_STATUS(clock.decode(reader), Status::ok);
  CHECK_EQ(clock.address(), 0x1234567000u);
  CHECK_EQ(reader.offset(), PageRequest::encoded_size); }

TEST(display_clock_request, the_present_engine_s_page_is_named_under_its_own_type)
{ std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.present_engine(0x7654321000u), Status::ok);
  CHECK_EQ(writer.length(), PageRequest::encoded_size);
  CHECK_EQ(type_of(bytes), 0x02F6u);
  CHECK_EQ(PageRequest::present_engine, 0x02F6u);
  WireReader reader(bytes, writer.length());
  PageRequest page;
  CHECK_STATUS(page.decode(reader), Status::ok);
  CHECK_EQ(page.address(), 0x7654321000u);
  AvRequestWriter small(bytes, PageRequest::encoded_size - 1u);
  CHECK_STATUS(small.present_engine(0x1000u), Status::too_large); }

TEST(display_clock_request, no_page_is_a_request_to_stop)
{ std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0u), Status::ok);
  WireReader reader(bytes, writer.length());
  PageRequest clock;
  CHECK_STATUS(clock.decode(reader), Status::ok);
  CHECK_EQ(clock.address(), 0u); }

TEST(display_clock_request, an_address_off_a_page_or_a_short_request_is_malformed)
{ std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0x5000u), Status::ok);
  WireReader short_reader(bytes, PageRequest::encoded_size - 1u);
  PageRequest short_clock;
  CHECK_STATUS(short_clock.decode(short_reader), Status::malformed);
  CHECK_EQ(short_clock.address(), 0u);
  const std::uint64_t off_page[] = {0x5001u, 0x5800u, 0x5FFFu};
  for (const std::uint64_t address : off_page) {
      CHECK_STATUS(writer.display_clock(address), Status::ok);
      WireReader reader(bytes, writer.length());
      PageRequest clock;
      CHECK_STATUS(clock.decode(reader), Status::malformed);
      CHECK_EQ(clock.address(), 0u);
  }
  bytes[31] = 0x80u;
  bytes[24] = 0u;
  bytes[25] = 0u;
  WireReader high_reader(bytes, PageRequest::encoded_size);
  PageRequest high;
  CHECK_STATUS(high.decode(high_reader), Status::ok);
  CHECK_EQ(high.address() >> 56u, 0x80u); }

TEST(display_timing_request, the_writer_s_request_decodes_to_its_head_and_rate)
{ std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
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
{ std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
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
{ std::uint8_t bytes[PageRequest::encoded_size - 1u] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.display_clock(0x1000u), Status::too_large);
  CHECK_EQ(writer.length(), 0u);
  CHECK_STATUS(writer.display_timing(0u, 60000u), Status::too_large);
  CHECK_EQ(writer.length(), 0u); }

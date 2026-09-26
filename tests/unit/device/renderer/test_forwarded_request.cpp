/* A forwarded control request is read as a virtqueue request is: never past
 * its end, and nothing at all from the end on. */
#include "device/renderer/forwarded_request.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>

using av::ForwardedRequest;

TEST(forwarded_request_read, copies_what_the_request_holds_from_the_offset)
{ const std::uint8_t bytes[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  ForwardedRequest    request(bytes, sizeof bytes);
  std::uint8_t        buffer[16] = {};
  CHECK_EQ(request.read(0u, buffer, sizeof bytes), sizeof bytes);
  CHECK_EQ(buffer[0], 1u);
  CHECK_EQ(buffer[7], 8u);
  CHECK_EQ(buffer[8], 0u);
  for (std::uint8_t &byte : buffer) byte = 0xEEu;
  CHECK_EQ(request.read(2u, buffer, 3u), 3u);
  CHECK_EQ(buffer[0], 3u);
  CHECK_EQ(buffer[2], 5u);
  CHECK_EQ(buffer[3], 0xEEu); }

TEST(forwarded_request_read, stops_at_the_end_and_reads_nothing_past_it)
{ const std::uint8_t bytes[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  ForwardedRequest    request(bytes, sizeof bytes);
  std::uint8_t        buffer[16] = {};
  for (std::uint8_t &byte : buffer) byte = 0xEEu;
  CHECK_EQ(request.read(6u, buffer, sizeof buffer), 2u);
  CHECK_EQ(buffer[0], 7u);
  CHECK_EQ(buffer[1], 8u);
  CHECK_EQ(buffer[2], 0xEEu);
  CHECK_EQ(request.read(7u, buffer, SIZE_MAX), 1u);   /* a size that would wrap */
  CHECK_EQ(buffer[0], 8u);
  CHECK_EQ(request.read(8u, buffer, sizeof buffer), 0u);
  CHECK_EQ(request.read(9u, buffer, sizeof buffer), 0u);
  CHECK_EQ(request.read(SIZE_MAX, buffer, sizeof buffer), 0u);
  CHECK_EQ(request.read(0u, buffer, 0u), 0u);
  CHECK_EQ(buffer[0], 8u);
  CHECK_EQ(buffer[2], 0xEEu);
  ForwardedRequest empty(nullptr, 0u);
  CHECK_EQ(empty.read(0u, buffer, sizeof buffer), 0u); }

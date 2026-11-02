#include "core/log/log_queue.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::LogQueue;

namespace {
const char *drained(LogQueue &queue, std::uint8_t (&out)[64])
{
    std::memset(out, 0, sizeof out);
    queue.take(out, sizeof out - 1u);
    return reinterpret_cast<const char *>(out);
}

}

TEST(log_queue_append, queues_each_line_with_its_newline_in_order)
{ std::uint8_t storage[32];
  std::uint8_t out[64];
  LogQueue queue(storage, sizeof storage);
  CHECK(queue.append("first"));
  CHECK(queue.append("second"));
  CHECK_EQ(queue.queued(), 13u);
  CHECK(std::strcmp(drained(queue, out), "first\nsecond\n") == 0);
  CHECK_EQ(queue.queued(), 0u); }

TEST(log_queue_append, a_line_that_does_not_fit_is_dropped_whole_and_counted)
{ std::uint8_t storage[10];
  std::uint8_t out[64];
  LogQueue queue(storage, sizeof storage);
  CHECK(queue.append("abcd"));
  CHECK(!queue.append("efghi"));
  CHECK(queue.append("efgh"));
  CHECK_EQ(queue.queued(), 10u);
  CHECK(!queue.append(""));
  CHECK_EQ(queue.take_dropped(), 2u);
  CHECK_EQ(queue.take_dropped(), 0u);
  CHECK(std::strcmp(drained(queue, out), "abcd\nefgh\n") == 0); }

TEST(log_queue_take, hands_out_part_of_the_queue_and_keeps_the_rest_in_order)
{ std::uint8_t storage[16];
  std::uint8_t out[64];
  LogQueue queue(storage, sizeof storage);
  CHECK(queue.append("0123456"));
  std::uint8_t part[3] = {};
  CHECK_EQ(queue.take(part, sizeof part), 3u);
  CHECK(std::memcmp(part, "012", 3u) == 0);
  CHECK_EQ(queue.queued(), 5u);
  CHECK(std::strcmp(drained(queue, out), "3456\n") == 0);
  CHECK_EQ(queue.take(part, sizeof part), 0u); }

TEST(log_queue_take, a_line_that_wraps_the_end_comes_out_whole)
{ std::uint8_t storage[8];
  std::uint8_t out[64];
  LogQueue queue(storage, sizeof storage);
  CHECK(queue.append("abcde"));
  std::uint8_t part[4] = {};
  CHECK_EQ(queue.take(part, sizeof part), 4u);
  CHECK(queue.append("wxyz"));
  CHECK_EQ(queue.queued(), 7u);
  CHECK(std::strcmp(drained(queue, out), "e\nwxyz\n") == 0);
  CHECK(!queue.append("toolongx"));
  CHECK_EQ(queue.take_dropped(), 1u); }

TEST(log_queue, without_storage_every_line_is_dropped)
{ std::uint8_t out[4] = {};
  LogQueue queue(nullptr, 64u);
  CHECK(!queue.append("anything"));
  CHECK_EQ(queue.queued(), 0u);
  CHECK_EQ(queue.take(out, sizeof out), 0u);
  CHECK_EQ(queue.take_dropped(), 1u); }

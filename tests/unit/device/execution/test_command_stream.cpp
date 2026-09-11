/* A batch payload split into commands: every header checked against the
 * staged copy before its payload is handed on. */
#include "core/support/status.hpp"
#include "device/execution/command_stream.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::CommandStream;
using av::Status;

namespace {

/* A payload built command by command. */
class Payload {
public:
    Payload &command(std::uint32_t id, std::uint32_t payload_bytes, std::uint8_t flags = 0u)
    {
        static_cast<void>(CommandHeader(id, 8u + payload_bytes, flags).encode(bytes_ + size_, 8u));
        for (std::uint32_t i = 0u; i < payload_bytes; i++) bytes_[size_ + 8u + i] = static_cast<std::uint8_t>(i);
        size_ += 8u + payload_bytes;
        return *this;
    }
    Payload &raw(const std::uint8_t *bytes, std::size_t count)
    {
        std::memcpy(bytes_ + size_, bytes, count);
        size_ += count;
        return *this;
    }
    std::uint8_t *bytes() { return bytes_; }
    std::size_t   size() const { return size_; }

private:
    std::uint8_t bytes_[512] = {};
    std::size_t  size_ = 0u;
};

/* Walks a whole payload: whether it ended cleanly. */
bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    CommandStream stream(bytes, length);
    CommandHeader header;
    const std::uint8_t *payload = nullptr;
    Status status = Status::ok;
    std::size_t walked = 0u;
    while ((status = stream.next(header, payload)) == Status::ok) {
        walked += header.size();
        if (payload != bytes + walked - header.size() + 8u || walked > length || !stream.invariants_hold())
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "a command outside the payload");
    }
    return status != Status::no_space;
}

}  // namespace

TEST(command_stream_next, walks_every_command_to_the_end)
{ Payload payload;
  payload.command(0x10001u, 0u).command(0x10002u, 13u, CommandHeader::generate_reply).command(0x30007u, 3u);
  CommandStream stream(payload.bytes(), payload.size());
  CommandHeader header;
  const std::uint8_t *bytes = nullptr;
  CHECK_STATUS(stream.next(header, bytes), Status::ok);
  CHECK_EQ(header.id(), 0x10001u);
  CHECK(bytes == payload.bytes() + 8u);
  CHECK_STATUS(stream.next(header, bytes), Status::ok);
  CHECK_EQ(header.size(), 21u);
  CHECK(header.generates_reply());
  CHECK_EQ(bytes[12], 12u);
  CHECK_STATUS(stream.next(header, bytes), Status::ok);
  CHECK_EQ(header.api(), 3u);
  CHECK(stream.invariants_hold());
  CHECK_STATUS(stream.next(header, bytes), Status::no_space);
  CHECK_EQ(stream.offset(), payload.size());
  CHECK_STATUS(stream.next(header, bytes), Status::no_space); }

TEST(command_stream_next, an_empty_payload_has_no_commands)
{ CommandStream stream(nullptr, 0u);
  CommandHeader header;
  const std::uint8_t *bytes = nullptr;
  CHECK_STATUS(stream.next(header, bytes), Status::no_space); }

TEST(command_stream_next, a_command_past_the_payload_a_bad_header_or_a_short_tail_is_malformed)
{ struct Case { const char *name; std::uint32_t size_word; std::size_t extra; };
  const Case cases[] = {
      {"past the end", 64u, 0u},
      {"below the header", 4u, 0u},
      {"unknown flag", 0x02000008u, 0u},
  };
  for (const Case &c : cases) {
      Payload payload;
      payload.command(0x10001u, 4u);
      const std::uint8_t header[8] = {1, 0, 1, 0, static_cast<std::uint8_t>(c.size_word),
                                      static_cast<std::uint8_t>(c.size_word >> 8), 0,
                                      static_cast<std::uint8_t>(c.size_word >> 24)};
      payload.raw(header, sizeof header);
      CommandStream stream(payload.bytes(), payload.size());
      CommandHeader decoded;
      const std::uint8_t *bytes = nullptr;
      CHECK_STATUS(stream.next(decoded, bytes), Status::ok);
      if (stream.next(decoded, bytes) != Status::malformed)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK_STATUS(stream.next(decoded, bytes), Status::malformed);   /* stays */
  }
  Payload tail;
  const std::uint8_t short_bytes[5] = {};
  tail.command(0x10001u, 0u).raw(short_bytes, sizeof short_bytes);
  CommandStream stream(tail.bytes(), tail.size());
  CommandHeader decoded;
  const std::uint8_t *bytes = nullptr;
  CHECK_STATUS(stream.next(decoded, bytes), Status::ok);
  CHECK_STATUS(stream.next(decoded, bytes), Status::malformed); }

TEST(command_stream_next, survives_the_hostile_sweep)
{ Payload payload;
  payload.command(0x10001u, 5u).command(0x10002u, 0u, CommandHeader::generate_reply).command(0x10003u, 9u);
  const av::HostileSweep sweep(rejects, payload.bytes(), payload.size());
  sweep.run();
  CHECK(!rejects(payload.bytes(), payload.size())); }

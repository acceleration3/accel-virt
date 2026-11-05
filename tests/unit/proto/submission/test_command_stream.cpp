#include "core/support/status.hpp"
#include "proto/submission/command_stream.hpp"
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

}

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
      CHECK_STATUS(stream.next(decoded, bytes), Status::malformed);
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
  CHECK(!rejects(payload.bytes(), payload.size()));
  const std::uint32_t acceptable = 112u + 96u + 72u + 3u + 3u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

TEST(command_stream_ends_in_piece, is_true_when_the_last_command_is_a_transport_piece_or_the_stream_is_damaged)
{ Payload whole;
  whole.command(0x10001u, 4u).command(CommandHeader::piece_id, 16u);
  CommandStream pieced(whole.bytes(), whole.size());
  CHECK(pieced.ends_in_piece());
  Payload other;
  other.command(CommandHeader::piece_id, 16u).command(0x10001u, 4u);
  CommandStream finished(other.bytes(), other.size());
  CHECK(!finished.ends_in_piece());
  Payload another_api;
  another_api.command(0x10000u | CommandHeader::piece_id, 16u);
  CommandStream foreign(another_api.bytes(), another_api.size());
  CHECK(!foreign.ends_in_piece());
  CommandStream empty(nullptr, 0u);
  CHECK(!empty.ends_in_piece());
  Payload cut;
  cut.command(0x10001u, 4u);
  const std::uint8_t damaged[8] = {1, 0, 1, 0, 4, 0, 0, 0};
  cut.raw(damaged, sizeof damaged);
  CommandStream broken(cut.bytes(), cut.size());
  CHECK(broken.ends_in_piece()); }

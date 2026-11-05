#include "core/support/status.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ContextRequest;
using av::Status;
using av::WireReader;

namespace {

constexpr std::size_t name_at = 32u;

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, 4); }

class Create {
public:
    Create(std::uint32_t id, std::uint32_t length, const char *name, std::size_t bytes)
    {
        put32(bytes_, 16u, id);
        put32(bytes_, 24u, length);
        std::memcpy(bytes_ + name_at, name, bytes);
    }
    Create(std::uint32_t length, const std::uint8_t *raw)
    {
        put32(bytes_, 24u, length);
        std::memcpy(bytes_ + name_at, raw, ContextRequest::name_capacity);
    }
    std::uint8_t *bytes() { return bytes_; }

private:
    std::uint8_t bytes_[ContextRequest::create_size] = {};
};

bool name_is_clean(const ContextRequest &request)
{
    const char *const name = request.name();
    std::size_t       length = 0u;
    while (length <= ContextRequest::name_capacity && name[length] != '\0') {
        if (static_cast<unsigned char>(name[length]) < 0x20u || static_cast<unsigned char>(name[length]) >= 0x7Fu)
            return false;
        length++;
    }
    return length <= ContextRequest::name_capacity;
}

bool accepts_clean(const std::uint8_t *bytes, std::size_t length)
{
    WireReader     reader(bytes, length);
    ContextRequest request;
    if (request.decode_create(reader) != Status::ok) return true;
    if (!name_is_clean(request))
        av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "a name kept a byte it must not");
    return false;
}

}

TEST(context_request_name, is_empty_before_any_decode)
{ const ContextRequest request;
  CHECK_EQ(request.name()[0], 0);
  CHECK_EQ(request.name_length(), 0u); }

TEST(context_request_name, a_printable_name_is_kept_whole)
{ Create create(7u, 12u, "vulkan-probe", 12u);
  WireReader     reader(create.bytes(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK(std::strcmp(request.name(), "vulkan-probe") == 0);
  CHECK_EQ(request.name_length(), 12u); }

TEST(context_request_name, stops_at_the_first_nul_even_when_the_length_claims_more)
{ Create create(1u, 20u, "app\0hidden-tail", 15u);
  WireReader     reader(create.bytes(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK(std::strcmp(request.name(), "app") == 0);
  CHECK_EQ(request.name_length(), 20u); }

TEST(context_request_name, stops_at_the_claimed_length_even_when_more_bytes_follow)
{ Create create(1u, 3u, "application", 11u);
  WireReader     reader(create.bytes(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK(std::strcmp(request.name(), "app") == 0); }

TEST(context_request_name, a_zero_length_gives_an_empty_name)
{ Create create(1u, 0u, "ignored", 7u);
  WireReader     reader(create.bytes(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK_EQ(request.name()[0], 0); }

TEST(context_request_name, a_length_past_the_capacity_is_clamped_with_the_terminator_in_place)
{ std::uint8_t raw[ContextRequest::name_capacity];
  std::memset(raw, 'x', sizeof raw);
  const std::uint32_t lengths[] = {64u, 65u, 1000u, 0x7FFFFFFFu, 0xFFFFFFFFu};
  for (const std::uint32_t length : lengths) {
      Create         create(length, raw);
      WireReader     reader(create.bytes(), ContextRequest::create_size);
      ContextRequest request;
      CHECK_STATUS(request.decode_create(reader), Status::ok);
      CHECK_EQ(std::strlen(request.name()), ContextRequest::name_capacity);
      CHECK_EQ(request.name_length(), length);
  } }

TEST(context_request_name, a_name_with_no_nul_in_its_whole_field_still_terminates)
{ std::uint8_t raw[ContextRequest::name_capacity];
  std::memset(raw, 'a', sizeof raw);
  Create         create(ContextRequest::name_capacity, raw);
  WireReader     reader(create.bytes(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK_EQ(std::strlen(request.name()), 64u);
  CHECK_EQ(request.name()[64], 0); }

TEST(context_request_name, every_non_printable_byte_becomes_a_question_mark)
{ for (unsigned value = 1u; value < 256u; value++) {
      std::uint8_t raw[ContextRequest::name_capacity] = {};
      raw[0] = 'a';
      raw[1] = static_cast<std::uint8_t>(value);
      raw[2] = 'b';
      Create         create(3u, raw);
      WireReader     reader(create.bytes(), ContextRequest::create_size);
      ContextRequest request;
      CHECK_STATUS(request.decode_create(reader), Status::ok);
      const bool printable = value >= 0x20u && value < 0x7Fu;
      CHECK_EQ(request.name()[0], 'a');
      CHECK_EQ(request.name()[1], printable ? static_cast<char>(value) : '?');
      CHECK_EQ(request.name()[2], 'b');
      CHECK(name_is_clean(request));
  } }

TEST(context_request_name, the_boundaries_of_the_printable_range_are_exact)
{ std::uint8_t raw[ContextRequest::name_capacity] = {};
  raw[0] = 0x1Fu;
  raw[1] = 0x20u;
  raw[2] = 0x7Eu;
  raw[3] = 0x7Fu;
  raw[4] = 0x80u;
  raw[5] = 0xFFu;
  Create         create(6u, raw);
  WireReader     reader(create.bytes(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK(std::strcmp(request.name(), "? ~???") == 0); }

TEST(context_request_name, newlines_and_escapes_cannot_reach_a_log_line)
{ Create create(1u, 12u, "a\nb\rc\x1b[31m!", 11u);
  WireReader     reader(create.bytes(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK(std::strchr(request.name(), '\n') == nullptr);
  CHECK(std::strchr(request.name(), '\r') == nullptr);
  CHECK(std::strchr(request.name(), '\x1b') == nullptr);
  CHECK(name_is_clean(request)); }

TEST(context_request_name, a_second_decode_replaces_a_longer_first_name)
{ Create        first(1u, 11u, "application", 11u);
  Create        second(2u, 2u, "ab", 2u);
  ContextRequest request;
  WireReader     one(first.bytes(), ContextRequest::create_size);
  WireReader     two(second.bytes(), ContextRequest::create_size);
  CHECK_STATUS(request.decode_create(one), Status::ok);
  CHECK_STATUS(request.decode_create(two), Status::ok);
  CHECK(std::strcmp(request.name(), "ab") == 0);
  CHECK_EQ(request.context_id(), 2u); }

TEST(context_request_name, a_short_request_keeps_the_previous_name)
{ Create        first(1u, 3u, "app", 3u);
  ContextRequest request;
  WireReader     one(first.bytes(), ContextRequest::create_size);
  CHECK_STATUS(request.decode_create(one), Status::ok);
  for (std::size_t length = 0u; length < ContextRequest::create_size; length++) {
      WireReader shorter(first.bytes(), length);
      CHECK_STATUS(request.decode_create(shorter), Status::malformed);
  }
  CHECK(std::strcmp(request.name(), "app") == 0); }

TEST(context_request_name, a_guarded_request_is_read_only_within_its_bounds)
{ av::GuardedBuffer buffer(ContextRequest::create_size);
  std::memset(buffer.data(), 'z', ContextRequest::create_size);
  put32(buffer.data(), 24u, 0xFFFFFFFFu);
  WireReader     reader(buffer.data(), ContextRequest::create_size);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK_EQ(std::strlen(request.name()), ContextRequest::name_capacity);
  CHECK(buffer.guards_intact()); }

TEST(context_request_name, survives_the_hostile_sweep)
{ Create create(1u, 12u, "vulkan-probe", 12u);
  const av::HostileSweep sweep(accepts_clean, create.bytes(), ContextRequest::create_size);
  sweep.run();
  CHECK(sweep.rejections() > 0u); }

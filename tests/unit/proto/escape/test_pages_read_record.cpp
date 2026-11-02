#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/pages_read_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::PagesReadRecord;
using av::Status;

namespace {

constexpr std::uint32_t allocation = 0x40000007u;
constexpr std::uint32_t wait = 100000u;

class Bytes {
public:
    explicit Bytes(const PagesReadRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *data() { return bytes_; }
    Status        status() const { return status_; }
    void          put(std::size_t at, std::uint8_t width, std::uint64_t value)
    { std::memcpy(bytes_ + at, &value, width); }

private:
    std::uint8_t bytes_[PagesReadRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

}

TEST(pages_read_record, a_begin_round_trips_with_every_field_where_the_contract_puts_it)
{ Bytes bytes{PagesReadRecord(PagesReadRecord::Action::begin, allocation, wait)};
  CHECK_STATUS(bytes.status(), Status::ok);
  const std::uint8_t expected[16] = {'A', 'V', 'B', 0, 1u, 0u, 1u, 0u, 0x07u, 0x00u, 0x00u, 0x40u, 0xA0u, 0x86u, 0x01u, 0x00u};
  CHECK(std::memcmp(bytes.data(), expected, sizeof expected) == 0);
  for (std::size_t at = 16u; at < PagesReadRecord::encoded_size; at++) CHECK_EQ(bytes.data()[at], 0u);
  PagesReadRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), PagesReadRecord::encoded_size), Status::ok);
  CHECK(decoded.action() == PagesReadRecord::Action::begin);
  CHECK_EQ(decoded.allocation(), allocation);
  CHECK_EQ(decoded.wait_microseconds(), wait);
  CHECK_EQ(decoded.writes(), 0u);
  CHECK(!decoded.complete()); }

TEST(pages_read_record, an_end_waits_for_nothing)
{ Bytes bytes{PagesReadRecord(PagesReadRecord::Action::end, allocation, 0u)};
  CHECK_STATUS(bytes.status(), Status::ok);
  CHECK_EQ(bytes.data()[6], 2u);
  PagesReadRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), PagesReadRecord::encoded_size), Status::ok);
  CHECK(decoded.action() == PagesReadRecord::Action::end);
  CHECK_EQ(decoded.wait_microseconds(), 0u); }

TEST(pages_read_record, the_answer_carries_the_writes_and_whether_they_had_moved)
{ const PagesReadRecord asked(PagesReadRecord::Action::begin, allocation, wait);
  Bytes complete{asked.answered(0x1122334455667788ull, true)};
  CHECK_STATUS(complete.status(), Status::ok);
  std::uint64_t at_writes = 0u;
  std::memcpy(&at_writes, complete.data() + 16u, sizeof at_writes);
  CHECK_EQ(at_writes, 0x1122334455667788ull);
  CHECK_EQ(complete.data()[24], 1u);
  PagesReadRecord read;
  CHECK_STATUS(read.read_answer(complete.data(), PagesReadRecord::encoded_size), Status::ok);
  CHECK_EQ(read.writes(), 0x1122334455667788ull);
  CHECK(read.complete());
  CHECK_EQ(read.allocation(), allocation);
  Bytes incomplete{asked.answered(9u, false)};
  CHECK_STATUS(read.read_answer(incomplete.data(), PagesReadRecord::encoded_size), Status::ok);
  CHECK_EQ(read.writes(), 9u);
  CHECK(!read.complete());
  PagesReadRecord request;
  CHECK_STATUS(request.decode(complete.data(), PagesReadRecord::encoded_size), Status::malformed);
  CHECK_STATUS(request.decode(incomplete.data(), PagesReadRecord::encoded_size), Status::malformed); }

TEST(pages_read_record, every_field_is_judged)
{ struct Case { const char *what; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", 0u, 4u, 0x004E5641u},
      {"version 0", 4u, 2u, 0u},
      {"version 2", 4u, 2u, 2u},
      {"action 0", 6u, 2u, 0u},
      {"action 3", 6u, 2u, 3u},
      {"no allocation", 8u, 4u, 0u},
      {"writes filled in", 16u, 8u, 1u},
      {"complete filled in", 24u, 4u, 1u},
      {"reserved", 28u, 4u, 1u},
  };
  for (const Case &one : cases) {
      Bytes bytes{PagesReadRecord(PagesReadRecord::Action::begin, allocation, wait)};
      bytes.put(one.at, one.width, one.value);
      PagesReadRecord decoded(PagesReadRecord::Action::end, 7u, 3u);
      CHECK_STATUS(decoded.decode(bytes.data(), PagesReadRecord::encoded_size), Status::malformed);
      CHECK(decoded.action() == PagesReadRecord::Action::end);
      CHECK_EQ(decoded.allocation(), 7u);
      CHECK_EQ(decoded.wait_microseconds(), 3u);
  }
  const Case answered_cases[] = {
      {"magic", 0u, 4u, 0x004E5641u},
      {"version", 4u, 2u, 2u},
      {"action", 6u, 2u, 3u},
      {"no allocation", 8u, 4u, 0u},
      {"complete 2", 24u, 4u, 2u},
      {"reserved", 28u, 4u, 1u},
  };
  for (const Case &one : answered_cases) {
      Bytes bytes{PagesReadRecord(PagesReadRecord::Action::begin, allocation, wait).answered(5u, true)};
      bytes.put(one.at, one.width, one.value);
      PagesReadRecord read;
      CHECK_STATUS(read.read_answer(bytes.data(), PagesReadRecord::encoded_size), Status::malformed);
      CHECK_EQ(read.writes(), 0u);
  } }

TEST(pages_read_record, a_record_of_any_other_length_is_refused)
{ Bytes bytes{PagesReadRecord(PagesReadRecord::Action::begin, allocation, wait)};
  PagesReadRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), PagesReadRecord::encoded_size - 1u), Status::malformed);
  CHECK_STATUS(decoded.read_answer(bytes.data(), PagesReadRecord::encoded_size - 1u), Status::malformed);
  std::uint8_t longer[PagesReadRecord::encoded_size + 8u] = {};
  std::memcpy(longer, bytes.data(), PagesReadRecord::encoded_size);
  CHECK_STATUS(decoded.decode(longer, sizeof longer), Status::malformed);
  CHECK_STATUS(PagesReadRecord(PagesReadRecord::Action::begin, allocation, wait)
                   .encode(longer, PagesReadRecord::encoded_size - 1u),
               Status::too_large); }

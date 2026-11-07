#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/clock_origin_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClockOriginRecord;
using av::Status;

namespace {

constexpr std::uint64_t counter = 123456789u;
constexpr std::uint64_t ticks = 987654321012ull;
constexpr std::uint64_t frequency = 1000000000u;

class Bytes {
public:
    explicit Bytes(const ClockOriginRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *data() { return bytes_; }
    Status        status() const { return status_; }
    void          put(std::size_t at, std::uint8_t width, std::uint64_t value)
    { std::memcpy(bytes_ + at, &value, width); }

private:
    std::uint8_t bytes_[ClockOriginRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

}

TEST(clock_origin_record, round_trips_with_each_field_where_the_contract_puts_it)
{ Bytes bytes{ClockOriginRecord(counter, ticks, frequency)};
  CHECK_STATUS(bytes.status(), Status::ok);
  const std::uint8_t expected[8] = {'A', 'V', 'C', 0, 1u, 0u, 0u, 0u};
  CHECK(std::memcmp(bytes.data(), expected, sizeof expected) == 0);
  std::uint64_t fields[3] = {};
  std::memcpy(fields, bytes.data() + 8u, sizeof fields);
  CHECK_EQ(fields[0], counter);
  CHECK_EQ(fields[1], ticks);
  CHECK_EQ(fields[2], frequency);
  ClockOriginRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), ClockOriginRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.counter(), counter);
  CHECK_EQ(decoded.ticks(), ticks);
  CHECK_EQ(decoded.frequency(), frequency); }

TEST(clock_origin_record, every_field_is_judged)
{ struct Case { const char *what; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", 0u, 4u, 0x00565641u},
      {"version 0", 4u, 2u, 0u},
      {"version 2", 4u, 2u, 2u},
      {"reserved half", 6u, 2u, 1u},
      {"no counter", 8u, 8u, 0u},
      {"no frequency", 24u, 8u, 0u},
      {"frequency past what the kernel can scale", 24u, 8u, ClockOriginRecord::most_frequency + 1u},
  };
  for (const Case &one : cases) {
      Bytes bytes{ClockOriginRecord(counter, ticks, frequency)};
      bytes.put(one.at, one.width, one.value);
      ClockOriginRecord decoded(7u, 8u, 9u);
      CHECK_STATUS(decoded.decode(bytes.data(), ClockOriginRecord::encoded_size), Status::malformed);
      CHECK_EQ(decoded.counter(), 7u);
      CHECK_EQ(decoded.ticks(), 8u);
      CHECK_EQ(decoded.frequency(), 9u);
  }
  Bytes most{ClockOriginRecord(counter, 0u, ClockOriginRecord::most_frequency)};
  ClockOriginRecord decoded;
  CHECK_STATUS(decoded.decode(most.data(), ClockOriginRecord::encoded_size), Status::ok); }

TEST(clock_origin_record, a_record_of_any_other_length_is_refused)
{ Bytes bytes{ClockOriginRecord(counter, ticks, frequency)};
  ClockOriginRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), ClockOriginRecord::encoded_size - 1u), Status::malformed);
  std::uint8_t longer[ClockOriginRecord::encoded_size + 8u] = {};
  std::memcpy(longer, bytes.data(), ClockOriginRecord::encoded_size);
  CHECK_STATUS(decoded.decode(longer, sizeof longer), Status::malformed);
  CHECK_STATUS(ClockOriginRecord(counter, ticks, frequency).encode(longer, ClockOriginRecord::encoded_size - 1u), Status::too_large); }

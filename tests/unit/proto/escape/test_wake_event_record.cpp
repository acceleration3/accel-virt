/* The escape record a process sends to have the driver set its event whenever
 * the host raises on its av context (docs/protocol.md, "Wake event record").
 * The kernel decodes it from memory the process wrote, so every field is
 * judged as hostile input. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/wake_event_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::Status;
using av::WakeEventRecord;

namespace {

constexpr std::uint64_t handle = 0x1F4u;

class Bytes {
public:
    explicit Bytes(const WakeEventRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *data() { return bytes_; }
    Status        status() const { return status_; }
    void          put(std::size_t at, std::uint8_t width, std::uint64_t value)
    { std::memcpy(bytes_ + at, &value, width); }

private:
    std::uint8_t bytes_[WakeEventRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

}  // namespace

TEST(wake_event_record, round_trips_with_the_handle_where_the_contract_puts_it)
{ Bytes bytes{WakeEventRecord(handle)};
  CHECK_STATUS(bytes.status(), Status::ok);
  const std::uint8_t expected[8] = {'A', 'V', 'E', 0, 1u, 0u, 0u, 0u};
  CHECK(std::memcmp(bytes.data(), expected, sizeof expected) == 0);
  std::uint64_t at_event = 0u;
  std::memcpy(&at_event, bytes.data() + 8u, sizeof at_event);
  CHECK_EQ(at_event, handle);
  for (std::size_t at = 16u; at < WakeEventRecord::encoded_size; at++) CHECK_EQ(bytes.data()[at], 0u);
  WakeEventRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), WakeEventRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.event(), handle); }

TEST(wake_event_record, every_field_is_judged)
{ struct Case { const char *what; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", 0u, 4u, 0x00565641u},   /* AVV, the views' */
      {"version 0", 4u, 2u, 0u},
      {"version 2", 4u, 2u, 2u},
      {"reserved half", 6u, 2u, 1u},
      {"no handle", 8u, 8u, 0u},
      {"reserved", 16u, 8u, 1u},
      {"reserved tail high bit", 24u, 8u, 0x8000000000000000ull},
  };
  for (const Case &one : cases) {
      Bytes bytes{WakeEventRecord(handle)};
      bytes.put(one.at, one.width, one.value);
      WakeEventRecord decoded(7u);
      CHECK_STATUS(decoded.decode(bytes.data(), WakeEventRecord::encoded_size), Status::malformed);
      CHECK_EQ(decoded.event(), 7u);
  } }

TEST(wake_event_record, a_record_of_any_other_length_is_refused)
{ Bytes bytes{WakeEventRecord(handle)};
  WakeEventRecord decoded;
  CHECK_STATUS(decoded.decode(bytes.data(), WakeEventRecord::encoded_size - 1u), Status::malformed);
  std::uint8_t longer[WakeEventRecord::encoded_size + 8u] = {};
  std::memcpy(longer, bytes.data(), WakeEventRecord::encoded_size);
  CHECK_STATUS(decoded.decode(longer, sizeof longer), Status::malformed);
  CHECK_STATUS(WakeEventRecord(handle).encode(longer, WakeEventRecord::encoded_size - 1u), Status::too_large); }

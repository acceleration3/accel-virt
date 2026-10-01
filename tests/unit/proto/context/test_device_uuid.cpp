#include "core/support/status.hpp"
#include "proto/context/device_uuid.hpp"
#include "core/support/text_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::DeviceUuid;
using av::Status;

namespace {

constexpr std::uint8_t nvidia[DeviceUuid::bytes] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};

}  // namespace

TEST(device_uuid_parse, reads_the_canonical_text_in_either_case)
{ DeviceUuid uuid;
  CHECK(!uuid.equals(nvidia));
  CHECK_STATUS(uuid.parse("7b7ad2c5-4219-8c80-c4fd-a64b8775aedb"), Status::ok);
  CHECK(uuid.equals(nvidia));
  DeviceUuid upper;
  CHECK_STATUS(upper.parse("7B7AD2C5-4219-8C80-C4FD-A64B8775AEDB"), Status::ok);
  CHECK(upper.equals(nvidia)); }

TEST(device_uuid_parse, anything_else_is_refused_and_leaves_the_uuid_unchanged)
{ const char *const refused[] = {
      "",
      "auto",
      "7b7ad2c5-4219-8c80-c4fd-a64b8775aed",     /* short */
      "7b7ad2c5-4219-8c80-c4fd-a64b8775aedbb",   /* long */
      "7b7ad2c5x4219-8c80-c4fd-a64b8775aedb",    /* separator */
      "7b7ad2c5-4219-8c80-c4fd-a64b8775aedg",    /* digit */
      "7b7ad2c5-42-198c80-c4fd-a64b8775aedb",    /* grouping */
      "7b7ad2c5-4219-8c80-c4fd-a64b8775ae:b",
      "7b7ad2c5-4219-8c80-c4fd-a64b8775ae/b",
      "7b7ad2c5-4219-8c80-c4fd-a64b8775ae@b",
      "7b7ad2c5-4219-8c80-c4fd-a64b8775ae`b",
      "7b7ad2c5-4219-8c80-c4fd-a64b8775aeGb",
  };
  for (const char *text : refused) {
      DeviceUuid uuid;
      CHECK_STATUS(uuid.parse("7b7ad2c5-4219-8c80-c4fd-a64b8775aedb"), Status::ok);
      CHECK_STATUS(uuid.parse(text), Status::invalid_parameter);
      CHECK(uuid.equals(nvidia));
  }
  DeviceUuid none;
  CHECK_STATUS(none.parse(nullptr), Status::invalid_parameter); }

/* What every host API reported, kept: the same bytes compare equal, others and
 * a null do not, and a null report is the all-zero UUID no GPU has. */
TEST(device_uuid, holds_the_bytes_an_api_reported)
{ const DeviceUuid reported(nvidia);
  CHECK(reported.equals(nvidia));
  CHECK(reported.equals(DeviceUuid(nvidia)));
  CHECK(!reported.equals(DeviceUuid()));
  CHECK(!reported.equals(static_cast<const std::uint8_t *>(nullptr)));
  CHECK(std::memcmp(reported.raw(), nvidia, DeviceUuid::bytes) == 0);
  std::uint8_t other[DeviceUuid::bytes] = {};
  std::memcpy(other, nvidia, sizeof other);
  other[15] ^= 1u;
  CHECK(!reported.equals(other));
  const std::uint8_t zeros[DeviceUuid::bytes] = {};
  CHECK(DeviceUuid(nullptr).equals(zeros)); }

TEST(device_uuid, crosses_the_wire_as_its_sixteen_bytes)
{ std::uint8_t bytes[DeviceUuid::bytes] = {};
  av::WireWriter writer(bytes, sizeof bytes);
  DeviceUuid(nvidia).encode(writer);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK(std::memcmp(bytes, nvidia, sizeof bytes) == 0);
  av::WireReader reader(bytes, sizeof bytes);
  DeviceUuid read;
  read.decode(reader);
  CHECK_STATUS(reader.status(), Status::ok);
  CHECK(read.equals(nvidia));
  /* A short read latches in the reader. */
  av::WireReader short_reader(bytes, sizeof bytes - 1u);
  DeviceUuid partial;
  partial.decode(short_reader);
  CHECK(short_reader.status() != Status::ok); }

TEST(device_uuid, writes_its_thirty_two_hex_digits)
{ char           line[40] = {};
  av::TextBuffer text(line, sizeof line);
  DeviceUuid(nvidia).append_hex(text);
  CHECK(std::strcmp(text.text(), "7B7AD2C542198C80C4FDA64B8775AEDB") == 0); }

#include "core/support/status.hpp"
#include "device/gpu/device_uuid.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

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

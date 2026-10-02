#include "miniport/adapter/adapter_description.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/gpu_identity.hpp"

#include <cstdint>

using av::AdapterDescription;
using av::GpuIdentity;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::uint8_t uuid[GpuIdentity::uuid_bytes] = {3};
constexpr std::uint64_t sixteen_gib = 16ull << 30;

int pdo_storage;
void *const pdo = &pdo_storage;

const GpuIdentity radeon(0x1002u, 0x7550u, GpuIdentity::discrete_gpu, uuid, sixteen_gib, 0u,
                         "AMD Radeon RX 9070 XT");

}  // namespace

TEST(adapter_description, names_the_device_and_fills_what_wmi_reads)
{ MockPlatform platform;
  AdapterDescription description(platform);
  CHECK_EQ(description.published(), 0u);
  CHECK_STATUS(description.publish(pdo, radeon), Status::ok);
  CHECK_EQ(description.published(), 1u);
  CHECK(platform.described_device() == pdo);
  CHECK(platform.device_description_is("AMD Radeon RX 9070 XT"));
  CHECK(platform.driver_text_is("HardwareInformation.AdapterString", "AMD Radeon RX 9070 XT"));
  CHECK(platform.driver_text_is("HardwareInformation.ChipType", "AMD Radeon RX 9070 XT"));
  CHECK(platform.driver_text_is("HardwareInformation.DacType", "Internal"));
  CHECK_EQ(platform.driver_value_type("HardwareInformation.qwMemorySize"), 11u);
  CHECK_EQ(platform.driver_number("HardwareInformation.qwMemorySize"), sixteen_gib);
  /* Sixteen GiB does not fit the 32-bit value; it saturates rather than wraps to 0. */
  CHECK_EQ(platform.driver_value_type("HardwareInformation.MemorySize"), 4u);
  CHECK_EQ(platform.driver_number("HardwareInformation.MemorySize"), 0xFFFFFFFFu);
  CHECK_EQ(platform.device_writes(), 6u); }

TEST(adapter_description, a_size_that_fits_is_written_exactly_in_both)
{ MockPlatform platform;
  AdapterDescription description(platform);
  const GpuIdentity small(0x8086u, 0x46A6u, GpuIdentity::integrated_gpu, uuid, 0xFFFFFFFFu, 0u, "Intel(R) Graphics");
  CHECK_STATUS(description.publish(pdo, small), Status::ok);
  CHECK_EQ(platform.driver_number("HardwareInformation.MemorySize"), 0xFFFFFFFFu);
  CHECK_EQ(platform.driver_number("HardwareInformation.qwMemorySize"), 0xFFFFFFFFu);
  const GpuIdentity smaller(0x8086u, 0x46A6u, GpuIdentity::integrated_gpu, uuid, 256u << 20, 0u, "x");
  CHECK_STATUS(description.publish(pdo, smaller), Status::ok);
  CHECK_EQ(platform.driver_number("HardwareInformation.MemorySize"), 256u << 20);
  CHECK(platform.device_description_is("x"));
  CHECK_EQ(description.published(), 2u); }

/* A GPU that names nothing leaves the INF's name standing: an empty
 * description would be worse than a generic one. */
TEST(adapter_description, a_gpu_with_no_name_leaves_the_description_alone)
{ MockPlatform platform;
  AdapterDescription description(platform);
  const GpuIdentity unnamed(0x1AF4u, 0x1050u, GpuIdentity::other_gpu, uuid, 0u, 0u, "");
  CHECK_STATUS(description.publish(pdo, unnamed), Status::ok);
  CHECK(platform.described_device() == nullptr);
  CHECK_EQ(platform.driver_value_type("HardwareInformation.AdapterString"), 0u);
  CHECK(platform.driver_text_is("HardwareInformation.DacType", "Internal"));
  CHECK_EQ(platform.driver_number("HardwareInformation.MemorySize"), 0u);
  CHECK_EQ(platform.driver_value_type("HardwareInformation.MemorySize"), 4u);
  CHECK_EQ(platform.device_writes(), 3u); }

/* Each value is its own write: one refused does not keep the rest from
 * Windows, and the first refusal is the one reported. */
TEST(adapter_description, a_refused_write_is_reported_and_the_rest_still_written)
{ for (std::uint32_t refused = 1u; refused <= 6u; refused++) {
    MockPlatform platform;
    AdapterDescription description(platform);
    platform.refuse_device_write_at(refused);
    CHECK_STATUS(description.publish(pdo, radeon), Status::not_supported);
    CHECK_EQ(platform.device_writes(), 5u);
    CHECK_EQ(description.published(), 0u); }
  MockPlatform platform;
  AdapterDescription description(platform);
  platform.refuse_device_write_at(2u);
  platform.refuse_device_write_at(0u);   /* taken back */
  CHECK_STATUS(description.publish(pdo, radeon), Status::ok); }

TEST(adapter_description, publishes_only_at_passive)
{ MockPlatform platform;
  AdapterDescription description(platform);
  platform.set_irql(av::Irql::dispatch);
  static_cast<void>(description.publish(pdo, radeon));
  platform.set_irql(av::Irql::passive);
  /* The description's own ceiling, then each write's. */
  std::uint32_t refusals = 0u;
  while (platform.claim(av::Violation::irql_ceiling)) refusals++;
  CHECK_EQ(refusals, 7u); }

/* The device an adapter started on, told apart from the one it wakes up to.
 * A hibernated guest is restored onto whatever QEMU is running now (feature
 * 040, FR-005). */
#include "core/platform/platform.hpp"
#include "core/rvk/device_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>

using av::CapsetInfo;
using av::DeviceIdentity;
using av::GpuIdentity;
using av::Irql;
using av::MockPlatform;
using av::ProtocolVersion;
using av::SubApi;
using av::Violation;

namespace {

constexpr std::uint8_t first_gpu[GpuIdentity::uuid_bytes] = {5};
constexpr std::uint8_t other_gpu[GpuIdentity::uuid_bytes] = {6};

CapsetInfo capset(const std::uint8_t *uuid = first_gpu, std::uint16_t major = 1u, std::uint16_t minor = 3u)
{
    return CapsetInfo(ProtocolVersion(major, minor), static_cast<std::uint32_t>(SubApi::vulkan), 64u, 12u, 24u, 1u,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 1u, "the host's"));
}

}  // namespace

TEST(device_identity, knows_nothing_until_it_is_told)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  CHECK(!identity.known());
  CHECK(!identity.changed());
  CHECK(identity.invariants_hold()); }

TEST(device_identity, the_same_device_is_accepted)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(identity.known());
  CHECK(identity.accept(capset()));
  CHECK(!identity.changed());
  CHECK_EQ(platform.logged(), 0u); }

/* Another host GPU behind the same PCI function is another device, whatever
 * the guest still holds for it. */
TEST(device_identity, another_host_gpu_is_another_device)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(!identity.accept(capset(other_gpu)));
  CHECK(identity.changed());
  CHECK(platform.logged() != 0u); }

/* A device that speaks an older protocol than the one this adapter agreed on
 * cannot be asked to finish what it never understood. */
TEST(device_identity, an_older_protocol_is_another_device)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset(first_gpu, 1u, 3u));
  CHECK(!identity.accept(capset(first_gpu, 1u, 2u)));
  CHECK(identity.changed()); }

TEST(device_identity, another_major_is_another_device)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset(first_gpu, 1u, 3u));
  CHECK(!identity.accept(capset(first_gpu, 2u, 3u)));
  CHECK(identity.changed()); }

/* A newer minor of the same major is the device growing, not changing. */
TEST(device_identity, a_newer_minor_of_the_same_device_is_accepted)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset(first_gpu, 1u, 3u));
  CHECK(identity.accept(capset(first_gpu, 1u, 9u)));
  CHECK(!identity.changed()); }

/* Nothing was remembered, so nothing can be judged: a device whose capset was
 * never read is not called changed. */
TEST(device_identity, an_unknown_device_accepts_whatever_it_finds)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  CHECK(identity.accept(capset(other_gpu)));
  CHECK(!identity.changed()); }

/* A change is one line, not one per question asked (FR-007). */
TEST(device_identity, a_change_is_logged_once)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(!identity.accept(capset(other_gpu)));
  const std::uint32_t logged = platform.logged();
  CHECK(!identity.accept(capset(other_gpu)));
  CHECK_EQ(platform.logged(), logged); }

/* StopDevice: the next start is a device of its own. */
TEST(device_identity, forgetting_leaves_nothing_to_compare)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(!identity.accept(capset(other_gpu)));
  identity.forget();
  CHECK(!identity.known());
  CHECK(!identity.changed());
  CHECK(identity.accept(capset(other_gpu)));
  CHECK(!identity.changed()); }

TEST(device_identity, every_call_keeps_its_irql_ceiling)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  platform.set_irql(Irql::dispatch);
  identity.remember(capset());
  CHECK(platform.claim(Violation::irql_ceiling));
  static_cast<void>(identity.accept(capset()));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

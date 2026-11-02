#include "core/platform/platform.hpp"
#include "miniport/adapter/device_identity.hpp"
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

namespace av {

struct DeviceIdentityProbe {
    static void change_without_knowing(DeviceIdentity &identity)
    {
        identity.known_ = false;
        identity.changed_ = true;
    }
};

}

namespace {

constexpr std::uint8_t first_gpu[GpuIdentity::uuid_bytes] = {5};
constexpr std::uint8_t other_gpu[GpuIdentity::uuid_bytes] = {6};

CapsetInfo capset(const std::uint8_t *uuid = first_gpu, std::uint16_t major = 1u, std::uint16_t minor = 3u)
{
    return CapsetInfo(ProtocolVersion(major, minor), static_cast<std::uint32_t>(SubApi::vulkan), 64u, 12u, 24u, 1u,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 1u, "the host's"));
}

}

TEST(device_identity, knows_nothing_until_it_is_told)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  CHECK(!identity.known());
  CHECK(!identity.changed());
  CHECK(!identity.offers(1u));
  CHECK(identity.invariants_hold()); }

TEST(device_identity, offers_what_the_capset_it_started_on_said)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(identity.offers(CapsetInfo::feature_host_visible_memory));
  CHECK(!identity.offers(CapsetInfo::feature_present_engine));
  identity.forget();
  CHECK(!identity.offers(CapsetInfo::feature_host_visible_memory)); }

TEST(device_identity, the_same_device_is_accepted)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(identity.known());
  CHECK(identity.accept(capset()));
  CHECK(!identity.changed());
  CHECK_EQ(platform.logged(), 0u); }

TEST(device_identity, another_host_gpu_is_another_device)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(!identity.accept(capset(other_gpu)));
  CHECK(identity.changed());
  CHECK(platform.logged() != 0u); }

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

TEST(device_identity, a_newer_minor_of_the_same_device_is_accepted)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset(first_gpu, 1u, 3u));
  CHECK(identity.accept(capset(first_gpu, 1u, 9u)));
  CHECK(!identity.changed()); }

TEST(device_identity, an_unknown_device_accepts_whatever_it_finds)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  CHECK(identity.accept(capset(other_gpu)));
  CHECK(!identity.changed()); }

TEST(device_identity, a_change_is_logged_once)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  identity.remember(capset());
  CHECK(!identity.accept(capset(other_gpu)));
  const std::uint32_t logged = platform.logged();
  CHECK(!identity.accept(capset(other_gpu)));
  CHECK_EQ(platform.logged(), logged); }

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

TEST(device_identity, a_change_without_a_device_to_compare_is_caught)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  CHECK(identity.invariants_hold());
  identity.remember(capset());
  CHECK(!identity.accept(capset(other_gpu)));
  CHECK(identity.invariants_hold());
  av::DeviceIdentityProbe::change_without_knowing(identity);
  CHECK(!identity.invariants_hold()); }

TEST(device_identity, every_call_keeps_its_irql_ceiling)
{ MockPlatform platform;
  DeviceIdentity identity(platform);
  platform.set_irql(Irql::dispatch);
  identity.remember(capset());
  CHECK(platform.claim(Violation::irql_ceiling));
  static_cast<void>(identity.accept(capset()));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

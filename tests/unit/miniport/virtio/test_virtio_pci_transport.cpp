#include "core/support/status.hpp"
#include "miniport/virtio/virtio_pci_transport.hpp"
#include "harness/device/fake_common_config.hpp"
#include "harness/device/fake_isr_register.hpp"
#include "harness/device/fake_register_bytes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

namespace av {

struct VirtioPciTransportProbe {
    explicit VirtioPciTransportProbe(VirtioPciTransport &target) : transport(target) {}
    void set_features(std::uint64_t features) { transport.features_ = features; }
    void set_multiplier(std::uint32_t multiplier) { transport.notify_multiplier_ = multiplier; }
    VirtioPciTransport &transport;
};

}  // namespace av

using av::FakeCommonConfig;
using av::VirtioPciTransportProbe;
using av::FakeIsrRegister;
using av::FakeRegisterBytes;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;
using av::VirtioPciTransport;

namespace {

constexpr std::uint64_t version_1 = 1ull << 32;
constexpr std::uint64_t edid = 1ull << 1;
constexpr std::uint64_t blob = 1ull << 3;
constexpr std::uint64_t context_init = 1ull << 4;

class Device {
public:
    Device() : notify_(0x1000u), device_(16u),
               transport_(platform_, common_, notify_, isr_, device_, 4u)
    {
        common_.offer_features(version_1 | edid | blob | context_init);
        common_.set_queues(2u, 256u);
        device_.set_u32(8u, 1u);    /* num_scanouts */
        device_.set_u32(12u, 2u);   /* num_capsets */
    }

    MockPlatform       &platform() { return platform_; }
    FakeCommonConfig   &common() { return common_; }
    FakeIsrRegister    &isr() { return isr_; }
    VirtioPciTransport &transport() { return transport_; }

private:
    MockPlatform       platform_;
    FakeCommonConfig   common_;
    FakeRegisterBytes  notify_;
    FakeIsrRegister    isr_;
    FakeRegisterBytes  device_;
    VirtioPciTransport transport_;
};

}  // namespace

TEST(virtio_pci_transport_negotiate, follows_the_initialization_sequence)
{ Device device;
  CHECK_STATUS(device.transport().negotiate(version_1 | edid | blob, version_1), Status::ok);
  CHECK(device.transport().invariants_hold());
  const FakeCommonConfig &common = device.common();
  CHECK_EQ(common.status_writes(), 4u);
  CHECK_EQ(common.status_written(0u), 0u);      /* reset */
  CHECK_EQ(common.status_written(1u), 1u);      /* ACKNOWLEDGE */
  CHECK_EQ(common.status_written(2u), 3u);      /* | DRIVER */
  CHECK_EQ(common.status_written(3u), 0xBu);    /* | FEATURES_OK */
  CHECK(device.transport().features() == (version_1 | edid | blob));
  CHECK(common.driver_features() == (version_1 | edid | blob)); }

TEST(virtio_pci_transport_negotiate, takes_the_intersection_with_what_is_offered)
{ Device device;
  const std::uint64_t unoffered = 1ull << 40;
  CHECK_STATUS(device.transport().negotiate(version_1 | edid | unoffered, version_1), Status::ok);
  CHECK(device.transport().features() == (version_1 | edid)); }

TEST(virtio_pci_transport_negotiate, a_missing_required_feature_fails_the_device)
{ Device device;
  device.common().offer_features(edid);
  CHECK_STATUS(device.transport().negotiate(version_1 | edid, version_1), Status::not_supported);
  CHECK((device.common().status() & 0x80u) != 0u);
  CHECK(device.transport().invariants_hold());
  CHECK(device.transport().features() == 0u); }

TEST(virtio_pci_transport_negotiate, a_device_refusing_features_ok_is_failed)
{ Device device;
  device.common().refuse_features_ok();
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::not_supported);
  CHECK((device.common().status() & 0x80u) != 0u); }

TEST(virtio_pci_transport_negotiate, waits_for_a_slow_reset)
{ Device device;
  device.common().delay_reset(5u);
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok); }

TEST(virtio_pci_transport_negotiate, gives_up_on_a_reset_that_never_completes)
{ Device device;
  device.common().delay_reset(100000u);
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::invalid_state);
  CHECK_EQ(device.common().status_writes(), 1u); }

TEST(virtio_pci_transport_negotiate, is_passive_only)
{ Device device;
  device.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK(device.platform().claim(Violation::irql_ceiling));
  CHECK(device.platform().claim(Violation::irql_ceiling));   /* and its reset */
  device.platform().set_irql(Irql::passive); }

TEST(virtio_pci_transport_start, sets_driver_ok_after_negotiation)
{ Device device;
  CHECK_STATUS(device.transport().start(), Status::invalid_state);
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK_STATUS(device.transport().start(), Status::ok);
  CHECK_EQ(device.common().status(), 0xFu);
  CHECK(device.transport().invariants_hold());
  CHECK_STATUS(device.transport().start(), Status::invalid_state); }

TEST(virtio_pci_transport_reset, returns_a_running_device_to_unconfigured)
{ Device device;
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK_STATUS(device.transport().start(), Status::ok);
  CHECK_STATUS(device.transport().reset(), Status::ok);
  CHECK(device.transport().invariants_hold());
  CHECK_EQ(device.common().status(), 0u);
  CHECK(device.transport().features() == 0u);
  CHECK_STATUS(device.transport().start(), Status::invalid_state); }

TEST(virtio_pci_transport_reset, a_reset_that_never_completes_is_invalid_state_and_unconfigured)
{ Device device;
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  device.common().delay_reset(100000u);
  CHECK_STATUS(device.transport().reset(), Status::invalid_state);
  CHECK(device.transport().features() == 0u);
  CHECK_STATUS(device.transport().start(), Status::invalid_state); }

TEST(virtio_pci_transport_abandon, writes_reset_once_from_any_irql_without_waiting)
{ Device device;
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK_STATUS(device.transport().start(), Status::ok);
  device.common().delay_reset(100000u);
  const std::uint32_t writes = device.common().status_writes();
  const std::uint64_t before = device.platform().interrupt_time();
  device.platform().set_irql(Irql::isr);
  device.transport().abandon();
  device.platform().set_irql(Irql::passive);
  CHECK_EQ(device.common().status_writes(), writes + 1u);
  CHECK_EQ(device.common().status_written(writes), 0u);
  CHECK(device.platform().interrupt_time() == before);
  CHECK(device.transport().features() == 0u);
  CHECK(device.transport().invariants_hold());
  CHECK_EQ(device.platform().unclaimed_violations(), 0u); }

TEST(virtio_pci_transport_config_vector, is_assigned_and_read_back)
{ Device device;
  CHECK_STATUS(device.transport().assign_config_vector(0u), Status::invalid_state);
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK_STATUS(device.transport().assign_config_vector(0u), Status::ok);
  CHECK_EQ(device.common().config_vector(), 0u); }

TEST(virtio_pci_transport_config_vector, a_refused_vector_is_no_space)
{ Device device;
  device.common().refuse_vectors();
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK_STATUS(device.transport().assign_config_vector(1u), Status::no_space); }

TEST(virtio_pci_transport, reports_a_device_that_needs_reset)
{ Device device;
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK(!device.transport().needs_reset());
  device.common().need_reset();
  CHECK(device.transport().needs_reset()); }

TEST(virtio_pci_transport, queue_count_is_the_devices)
{ Device device;
  CHECK_EQ(device.transport().queue_count(), 2u); }

TEST(virtio_pci_transport_read_isr, returns_and_clears_the_status)
{ Device device;
  device.isr().raise(0x3u);
  device.platform().set_irql(Irql::isr);
  CHECK_EQ(device.transport().read_isr(), 3u);
  CHECK_EQ(device.transport().read_isr(), 0u);
  device.platform().set_irql(Irql::passive); }

TEST(virtio_pci_transport_device_config, reads_within_the_region)
{ Device device;
  std::uint32_t value = 0u;
  CHECK_STATUS(device.transport().read_device_u32(8u, value), Status::ok);
  CHECK_EQ(value, 1u);
  CHECK_STATUS(device.transport().read_device_u32(12u, value), Status::ok);
  CHECK_EQ(value, 2u); }

TEST(virtio_pci_transport_device_config, refuses_reads_past_the_region)
{ Device device;
  std::uint32_t value = 7u;
  CHECK_STATUS(device.transport().read_device_u32(13u, value), Status::invalid_parameter);
  CHECK_STATUS(device.transport().read_device_u32(~std::uint64_t{0} - 1u, value), Status::invalid_parameter);
  CHECK_EQ(value, 7u); }

TEST(virtio_pci_transport_fail, marks_the_device_failed)
{ Device device;
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  device.transport().fail();
  CHECK_EQ(device.common().status(), 0x8Bu);
  CHECK(device.transport().invariants_hold());
  CHECK_STATUS(device.transport().start(), Status::invalid_state); }

TEST(virtio_pci_transport_invariants, features_without_negotiation_are_detected)
{ Device device;
  VirtioPciTransportProbe probe(device.transport());
  probe.set_features(version_1);
  CHECK(!device.transport().invariants_hold());
  probe.set_features(0u);
  probe.set_multiplier(0x2000u);
  CHECK(!device.transport().invariants_hold()); }

TEST(virtio_pci_transport_device_config, a_region_shorter_than_a_field_refuses_every_read)
{ MockPlatform platform;
  FakeCommonConfig common;
  FakeRegisterBytes notify(0x1000u);
  FakeIsrRegister isr;
  FakeRegisterBytes tiny(2u);
  VirtioPciTransport transport(platform, common, notify, isr, tiny, 4u);
  std::uint32_t value = 0u;
  CHECK_STATUS(transport.read_device_u32(0u, value), Status::invalid_parameter); }

/* Feature 043: a context's fence queue gets its vector when the context is set
 * up, long after DRIVER_OK, and is never enabled. */
TEST(virtio_pci_transport_queue_vector, is_assigned_to_a_queue_the_device_has_after_driver_ok)
{ Device device;
  device.common().set_queues(66u, 256u);
  CHECK_STATUS(device.transport().assign_queue_vector(2u, 3u), Status::invalid_state);
  CHECK_STATUS(device.transport().negotiate(version_1, version_1), Status::ok);
  CHECK_STATUS(device.transport().assign_queue_vector(65u, 4u), Status::ok);
  CHECK_STATUS(device.transport().start(), Status::ok);
  CHECK_STATUS(device.transport().assign_queue_vector(2u, 3u), Status::ok);
  CHECK_EQ(device.common().queue(2u).vector, 3u);
  CHECK_EQ(device.common().queue(65u).vector, 4u);
  CHECK_EQ(device.common().queue(2u).enable, 0u);
  CHECK_STATUS(device.transport().assign_queue_vector(66u, 3u), Status::not_found);
  device.common().refuse_vectors();
  CHECK_STATUS(device.transport().assign_queue_vector(3u, 3u), Status::no_space);
  device.transport().fail();
  CHECK_STATUS(device.transport().assign_queue_vector(3u, 3u), Status::invalid_state); }

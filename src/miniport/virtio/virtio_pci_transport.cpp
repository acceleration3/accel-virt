#include "miniport/virtio/virtio_pci_transport.hpp"

#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/virtio/virt_queue.hpp"

namespace av {

namespace {

/* virtio 1.2 4.1.4.3 common configuration offsets. */
enum : uint64_t {
    device_feature_select = 0x00u,
    device_feature        = 0x04u,
    driver_feature_select = 0x08u,
    driver_feature        = 0x0Cu,
    config_msix_vector    = 0x10u,
    num_queues            = 0x12u,
    device_status         = 0x14u,
    queue_select          = 0x16u,
    queue_size            = 0x18u,
    queue_msix_vector     = 0x1Au,
    queue_enable          = 0x1Cu,
    queue_notify_off      = 0x1Eu,
    queue_desc            = 0x20u,
    queue_driver          = 0x28u,
    queue_device          = 0x30u,
};

/* 2.1 device status bits. */
enum : uint8_t {
    acknowledge        = 1u,
    driver             = 2u,
    driver_ok          = 4u,
    features_ok        = 8u,
    device_needs_reset = 0x40u,
    failed             = 0x80u,
};

}  // namespace

VirtioPciTransport::VirtioPciTransport(Platform &platform, RegisterWindow &common,
                                       RegisterWindow &notify, RegisterWindow &isr,
                                       RegisterWindow &device, uint32_t notify_multiplier)
    : platform_(platform), common_(common), notify_(notify), isr_(isr), device_(device),
      notify_multiplier_(notify_multiplier), features_(0u), stage_(Stage::unconfigured)
      {
          AV_SCOPE("VirtioPciTransport::VirtioPciTransport");
      }

Status VirtioPciTransport::reset()
{
    AV_SCOPE("VirtioPciTransport::reset");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::reset");
    features_ = 0u;
    stage_ = Stage::unconfigured;
    common_.write_u8(device_status, 0u);
    for (uint32_t poll = 0u; poll < reset_polls; poll++) {
        if (common_.read_u8(device_status) == 0u) return Status::ok;
        platform_.stall(reset_poll_us);
    }
    platform_.log("virtio: device did not complete reset");
    return Status::invalid_state;
}

void VirtioPciTransport::abandon()
{
    AV_SCOPE("VirtioPciTransport::abandon");
    platform_.require_irql_at_most(Irql::isr, "VirtioPciTransport::abandon");
    features_ = 0u;
    stage_ = Stage::unconfigured;
    common_.write_u8(device_status, 0u);
}

void VirtioPciTransport::add_status(uint8_t bits)
{
    AV_SCOPE("VirtioPciTransport::add_status");
    common_.write_u8(device_status, static_cast<uint8_t>(common_.read_u8(device_status) | bits));
}

uint64_t VirtioPciTransport::read_device_features()
{
    AV_SCOPE("VirtioPciTransport::read_device_features");
    common_.write_u32(device_feature_select, 0u);
    uint64_t features = common_.read_u32(device_feature);
    common_.write_u32(device_feature_select, 1u);
    features |= uint64_t{common_.read_u32(device_feature)} << 32u;
    return features;
}

void VirtioPciTransport::write_driver_features(uint64_t features)
{
    AV_SCOPE("VirtioPciTransport::write_driver_features");
    common_.write_u32(driver_feature_select, 0u);
    common_.write_u32(driver_feature, static_cast<uint32_t>(features));
    common_.write_u32(driver_feature_select, 1u);
    common_.write_u32(driver_feature, static_cast<uint32_t>(features >> 32u));
}

Status VirtioPciTransport::negotiate(uint64_t wanted, uint64_t required)
{
    AV_SCOPE("VirtioPciTransport::negotiate");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::negotiate");
    InvariantScope<VirtioPciTransport> guard(platform_, *this, "negotiate");
    const Status reset_status = reset();
    if (reset_status != Status::ok) return reset_status;
    add_status(acknowledge);
    add_status(driver);

    const uint64_t offered = read_device_features();
    if ((offered & required) != required) {
        platform_.log("virtio: device lacks a required feature");
        fail();
        return Status::not_supported;
    }
    const uint64_t negotiated = offered & (wanted | required);
    write_driver_features(negotiated);
    add_status(features_ok);
    if ((common_.read_u8(device_status) & features_ok) == 0u) {
        platform_.log("virtio: device refused FEATURES_OK");
        fail();
        return Status::not_supported;
    }
    features_ = negotiated;
    stage_ = Stage::negotiated;
    return Status::ok;
}

Status VirtioPciTransport::start()
{
    AV_SCOPE("VirtioPciTransport::start");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::start");
    InvariantScope<VirtioPciTransport> guard(platform_, *this, "start");
    if (stage_ != Stage::negotiated) return Status::invalid_state;
    add_status(driver_ok);
    stage_ = Stage::running;
    return Status::ok;
}

Status VirtioPciTransport::assign_config_vector(uint16_t vector)
{
    AV_SCOPE("VirtioPciTransport::assign_config_vector");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::assign_config_vector");
    if (stage_ != Stage::negotiated) return Status::invalid_state;
    common_.write_u16(config_msix_vector, vector);
    return common_.read_u16(config_msix_vector) == vector ? Status::ok : Status::no_space;
}

Status VirtioPciTransport::activate_queue(VirtQueue &queue, uint16_t vector)
{
    AV_SCOPE("VirtioPciTransport::activate_queue");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::activate_queue");
    InvariantScope<VirtioPciTransport> guard(platform_, *this, "activate_queue");
    if (stage_ != Stage::negotiated) return Status::invalid_state;
    if (queue.index() >= common_.read_u16(num_queues)) return Status::not_found;
    common_.write_u16(queue_select, queue.index());
    const uint16_t device_size = common_.read_u16(queue_size);
    if (device_size == 0u) return Status::not_found;
    const Status allocated = queue.initialize(device_size);
    if (allocated != Status::ok) return allocated;

    common_.write_u16(queue_size, queue.size());
    common_.write_u16(queue_msix_vector, vector);
    if (common_.read_u16(queue_msix_vector) != vector) return Status::no_space;
    common_.write_u64(queue_desc, queue.descriptor_area());
    common_.write_u64(queue_driver, queue.driver_area());
    common_.write_u64(queue_device, queue.device_area());
    const uint64_t offset = uint64_t{common_.read_u16(queue_notify_off)} * notify_multiplier_;
    if (notify_.length() < 2u || offset > notify_.length() - 2u) return Status::malformed;
    queue.bind_notify_offset(offset);
    common_.write_u16(queue_enable, 1u);
    return common_.read_u16(queue_enable) == 1u ? Status::ok : Status::invalid_state;
}

Status VirtioPciTransport::assign_queue_vector(uint16_t index, uint16_t vector)
{
    AV_SCOPE("VirtioPciTransport::assign_queue_vector");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::assign_queue_vector");
    if (stage_ != Stage::negotiated && stage_ != Stage::running) return Status::invalid_state;
    if (index >= common_.read_u16(num_queues)) return Status::not_found;
    common_.write_u16(queue_select, index);
    common_.write_u16(queue_msix_vector, vector);
    return common_.read_u16(queue_msix_vector) == vector ? Status::ok : Status::no_space;
}

void VirtioPciTransport::notify(const VirtQueue &queue)
{
    AV_SCOPE("VirtioPciTransport::notify");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioPciTransport::notify");
    notify_.write_u16(queue.notify_offset(), queue.index());
}

void VirtioPciTransport::fail()
{
    AV_SCOPE("VirtioPciTransport::fail");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioPciTransport::fail");
    add_status(failed);
    features_ = 0u;
    stage_ = Stage::failed;
}

uint64_t VirtioPciTransport::features() const
{
    AV_SCOPE("VirtioPciTransport::features");
    return features_;
}

uint16_t VirtioPciTransport::queue_count()
{
    AV_SCOPE("VirtioPciTransport::queue_count");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::queue_count");
    return common_.read_u16(num_queues);
}

bool VirtioPciTransport::needs_reset()
{
    AV_SCOPE("VirtioPciTransport::needs_reset");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioPciTransport::needs_reset");
    return (common_.read_u8(device_status) & device_needs_reset) != 0u;
}

uint8_t VirtioPciTransport::read_isr()
{
    AV_SCOPE("VirtioPciTransport::read_isr");
    platform_.require_irql_at_most(Irql::isr, "VirtioPciTransport::read_isr");
    return isr_.read_u8(0u);
}

Status VirtioPciTransport::read_device_u32(uint64_t offset, uint32_t &value)
{
    AV_SCOPE("VirtioPciTransport::read_device_u32");
    platform_.require_irql_at_most(Irql::passive, "VirtioPciTransport::read_device_u32");
    const uint64_t length = device_.length();
    if (length < 4u || offset > length - 4u) return Status::invalid_parameter;
    value = device_.read_u32(offset);
    return Status::ok;
}

Status VirtioPciTransport::write_device_u32(uint64_t offset, uint32_t value)
{
    AV_SCOPE("VirtioPciTransport::write_device_u32");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioPciTransport::write_device_u32");
    const uint64_t length = device_.length();
    if (length < 4u || offset > length - 4u) return Status::invalid_parameter;
    device_.write_u32(offset, value);
    return Status::ok;
}

bool VirtioPciTransport::invariants_hold() const
{
    AV_SCOPE("VirtioPciTransport::invariants_hold");
    const bool configured = stage_ == Stage::negotiated || stage_ == Stage::running;
    return (configured || features_ == 0u) && notify_multiplier_ <= notify_.length();
}

}  // namespace av

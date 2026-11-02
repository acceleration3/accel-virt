#include "harness/dxgkrnl/fake_dxgkrnl_fdo.hpp"

#include "miniport/ddi/dxgk_child_descriptor_record.hpp"
#include "miniport/ddi/dxgk_child_status_record.hpp"
#include "miniport/ddi/dxgk_device_descriptor_record.hpp"
#include "miniport/ddi/linked_device_record.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstring>

namespace av {

namespace {

using AddDevice = std::int32_t (*)(void *, void **);
using StartDevice = std::int32_t (*)(void *, void *, const DxgkrnlInterfaceRecord *, std::uint32_t *, std::uint32_t *);
using ContextOnly = std::int32_t (*)(void *);
using InterruptRoutine = std::uint8_t (*)(void *, std::uint32_t);
using VoidContext = void (*)(void *);
using QueryChildRelations = std::int32_t (*)(void *, DxgkChildDescriptorRecord *, std::uint32_t);
using QueryChildStatus = std::int32_t (*)(void *, DxgkChildStatusRecord *, std::uint8_t);
using QueryDeviceDescriptor = std::int32_t (*)(void *, std::uint32_t, DxgkDeviceDescriptorRecord *);
using SetPowerState = std::int32_t (*)(void *, std::uint32_t, std::int32_t, std::int32_t);
using LinkDevice = std::int32_t (*)(void *, void *, LinkedDeviceRecord *);

constexpr std::int32_t  status_quota_exceeded = static_cast<std::int32_t>(0xC0000044u);
constexpr std::int32_t  status_invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
constexpr std::int32_t  status_connection = 1;
constexpr std::uint32_t adapter_uid = 0xFFFFFFFFu;
constexpr std::int32_t  power_action_none = 0;

struct StartInfo {
    std::uint8_t bytes[32];
};

}

FakeDxgkrnlFdo::FakeDxgkrnlFdo(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlAdapter &adapter)
    : platform_(platform), table_(table), adapter_(adapter), dxgadapter_(platform, table), context_(nullptr), started_(false), sources_(0u),
      children_(0u), connected_(0u) {}

std::int32_t FakeDxgkrnlFdo::add_device()
{
    platform_.set_irql(Irql::passive);
    const auto add = reinterpret_cast<AddDevice>(table_.routine(DdiSlot::DxgkDdiAddDevice));
    std::int32_t status = add(&adapter_, &context_);
    if (status < 0) return status;
    const auto link = reinterpret_cast<LinkDevice>(table_.routine(DdiSlot::DxgkDdiLinkDevice));
    if (link == nullptr) return status;
    LinkedDeviceRecord linked = {};
    status = link(&adapter_, context_, &linked);
    if (status >= 0 && linked.NumberOfLinksInChain > max_links) status = status_quota_exceeded;
    if (status >= 0 && linked.NumberOfLinksInChain != 0u && linked.LeadLink == 0u) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "adapter reported itself a secondary link");
    }
    if (status < 0) {
        reinterpret_cast<ContextOnly>(table_.routine(DdiSlot::DxgkDdiRemoveDevice))(context_);
        context_ = nullptr;
    }
    return status;
}

std::int32_t FakeDxgkrnlFdo::start_device()
{
    platform_.set_irql(Irql::passive);
    StartInfo start_info = {};
    const auto start = reinterpret_cast<StartDevice>(table_.routine(DdiSlot::DxgkDdiStartDevice));
    std::int32_t status = start(context_, &start_info, &adapter_.record(), &sources_, &children_);
    if (status >= 0 && children_ > max_children) status = status_invalid_parameter;
    if (status >= 0) {
        DxgkChildDescriptorRecord relations[max_children + 1u];
        std::memset(relations, 0, sizeof relations);
        const auto query = reinterpret_cast<QueryChildRelations>(table_.routine(DdiSlot::DxgkDdiQueryChildRelations));
        status = query(context_, relations, (children_ + 1u) * sizeof relations[0]);
    }
    if (status >= 0) status = dxgadapter_.initialize(context_, children_);
    if (status < 0) {
        reinterpret_cast<ContextOnly>(table_.routine(DdiSlot::DxgkDdiStopDevice))(context_);
        return status;
    }
    started_ = true;
    return status;
}

std::int32_t FakeDxgkrnlFdo::enumerate_children()
{
    platform_.set_irql(Irql::passive);
    connected_ = 0u;
    for (std::uint32_t child = 0u; child < children_; child++) {
        DxgkChildStatusRecord child_status = {};
        child_status.Type = status_connection;
        child_status.ChildUid = child;
        const auto query_status = reinterpret_cast<QueryChildStatus>(table_.routine(DdiSlot::DxgkDdiQueryChildStatus));
        std::int32_t status = query_status(context_, &child_status, 1u);
        if (status < 0) return status;
        connected_ += child_status.HotPlugConnected != 0u ? 1u : 0u;
        std::uint8_t edid[128] = {};
        DxgkDeviceDescriptorRecord descriptor = {0u, sizeof edid, edid};
        const auto query_descriptor =
            reinterpret_cast<QueryDeviceDescriptor>(table_.routine(DdiSlot::DxgkDdiQueryDeviceDescriptor));
        status = query_descriptor(context_, child, &descriptor);
        if (status < 0) return status;
    }
    return 0;
}

std::uint8_t FakeDxgkrnlFdo::interrupt(std::uint32_t message)
{
    const std::uint32_t queued = adapter_.dpcs_queued();
    platform_.set_irql(Irql::isr);
    const std::uint8_t claimed =
        reinterpret_cast<InterruptRoutine>(table_.routine(DdiSlot::DxgkDdiInterruptRoutine))(context_, message);
    platform_.set_irql(Irql::passive);
    if (adapter_.dpcs_queued() == queued) return claimed;
    platform_.set_irql(Irql::dispatch);
    reinterpret_cast<VoidContext>(table_.routine(DdiSlot::DxgkDdiDpcRoutine))(context_);
    platform_.set_irql(Irql::passive);
    return claimed;
}

void FakeDxgkrnlFdo::dpc()
{
    platform_.set_irql(Irql::dispatch);
    reinterpret_cast<VoidContext>(table_.routine(DdiSlot::DxgkDdiDpcRoutine))(context_);
    platform_.set_irql(Irql::passive);
}

std::int32_t FakeDxgkrnlFdo::set_power_state(std::int32_t device_power_state)
{
    platform_.set_irql(Irql::passive);
    return reinterpret_cast<SetPowerState>(table_.routine(DdiSlot::DxgkDdiSetPowerState))(
        context_, adapter_uid, device_power_state, power_action_none);
}

void FakeDxgkrnlFdo::reset_device()
{
    platform_.set_irql(Irql::isr);
    reinterpret_cast<VoidContext>(table_.routine(DdiSlot::DxgkDdiResetDevice))(context_);
    platform_.set_irql(Irql::passive);
}

void FakeDxgkrnlFdo::on_stopping(Stopping stopping, void *context)
{
    stopping_ = stopping;
    stopping_context_ = context;
}

std::int32_t FakeDxgkrnlFdo::stop_device()
{
    platform_.set_irql(Irql::passive);
    if (stopping_ != nullptr) stopping_(stopping_context_);
    started_ = false;
    return reinterpret_cast<ContextOnly>(table_.routine(DdiSlot::DxgkDdiStopDevice))(context_);
}

std::int32_t FakeDxgkrnlFdo::surprise_removal()
{
    constexpr std::int32_t pnp_notify = 1;
    platform_.set_irql(Irql::passive);
    const std::int32_t status = reinterpret_cast<std::int32_t (*)(void *, std::int32_t)>(
        table_.routine(DdiSlot::DxgkDdiNotifySurpriseRemoval))(context_, pnp_notify);
    if (status < 0 || !started_) return status;
    return stop_device();
}

std::int32_t FakeDxgkrnlFdo::remove_device()
{
    platform_.set_irql(Irql::passive);
    if (started_) static_cast<void>(stop_device());
    const std::int32_t status = reinterpret_cast<ContextOnly>(table_.routine(DdiSlot::DxgkDdiRemoveDevice))(context_);
    context_ = nullptr;
    return status;
}

FakeDxgadapter &FakeDxgkrnlFdo::dxgadapter() { return dxgadapter_; }
void         *FakeDxgkrnlFdo::context() const { return context_; }
bool          FakeDxgkrnlFdo::started() const { return started_; }
std::uint32_t FakeDxgkrnlFdo::sources() const { return sources_; }
std::uint32_t FakeDxgkrnlFdo::children() const { return children_; }
std::uint32_t FakeDxgkrnlFdo::connected_children() const { return connected_; }

}

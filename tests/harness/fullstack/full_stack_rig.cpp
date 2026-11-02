#include "harness/fullstack/full_stack_rig.hpp"

namespace av {

namespace {

constexpr std::int32_t not_added = -1;

void *window_memory(void *context, std::uint64_t physical, std::uint64_t)
{
    auto *const host = static_cast<MockHost *>(context);
    if (physical < FullStackRig::hostmem_address) return nullptr;
    return host->placed_at(physical - FullStackRig::hostmem_address);
}

}

FullStackRig::FullStackRig()
    : driver_(), function_(), dxgkrnl_(driver_.platform(), function_), fdo_(driver_.platform(), driver_.table(), dxgkrnl_),
      device_(driver_.platform()), bridge_(driver_.platform(), function_.common(), device_),
      pump_(fdo_, dxgkrnl_, function_.common(), device_.host(), bridge_),
      kmt_(driver_.platform(), driver_.table(), fdo_, dxgkrnl_, function_, pump_, device_.host()), added_(not_added),
      started_(not_added)
{
    start();
}

FullStackRig::FullStackRig(const AvExecutorOps &executor, const std::uint8_t *gpu)
    : driver_(), function_(), dxgkrnl_(driver_.platform(), function_), fdo_(driver_.platform(), driver_.table(), dxgkrnl_),
      device_(driver_.platform(), executor, gpu), bridge_(driver_.platform(), function_.common(), device_),
      pump_(fdo_, dxgkrnl_, function_.common(), device_.host(), bridge_),
      kmt_(driver_.platform(), driver_.table(), fdo_, dxgkrnl_, function_, pump_, device_.host()), added_(not_added),
      started_(not_added)
{
    device_.host().advance_clock_per_read(worker_clock_step_ns);
    start();
}

void FullStackRig::start()
{
    driver_.platform().resolve_process_views(&window_memory, &device_.host());
    function_.set_message_interrupts(message_interrupts);
    function_.add_shared_memory_region(hostmem_region, hostmem_bar, 0u, hostmem_bytes, hostmem_address);
    function_.add_doorbell_region();
    function_.set_subsystem_id(av_subsystem);
    function_.present_host_gpu(host_vendor, host_device, host_revision);
    function_.serve_notify_with(bridge_);
    function_.serve_doorbells_with(device_.doorbells());
    added_ = fdo_.add_device();
    if (added_ < 0) return;
    started_ = fdo_.start_device();
    if (started_ >= 0) static_cast<void>(fdo_.enumerate_children());
    pump_.deliver();
}

FullStackRig::~FullStackRig()
{
    kmt_.end_process();
    if (added_ >= 0) static_cast<void>(fdo_.remove_device());
    driver_.platform().resolve_process_views(nullptr, nullptr);
}

std::int32_t           FullStackRig::added() const { return added_; }
std::int32_t           FullStackRig::started() const { return started_; }
RegisteredDriver      &FullStackRig::driver() { return driver_; }
MockPlatform          &FullStackRig::platform() { return driver_.platform(); }
FakeVirtioGpuFunction &FullStackRig::function() { return function_; }
FakeDxgkrnlAdapter    &FullStackRig::dxgkrnl() { return dxgkrnl_; }
FakeDxgkrnlFdo        &FullStackRig::fdo() { return fdo_; }
LoopbackDevice        &FullStackRig::device() { return device_; }
VirtioBridge          &FullStackRig::bridge() { return bridge_; }
InterruptPump         &FullStackRig::pump() { return pump_; }
DdiKmt                &FullStackRig::kmt() { return kmt_; }

}

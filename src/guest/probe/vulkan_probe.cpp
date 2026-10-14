#include "guest/probe/vulkan_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "guest/probe/vulkan_entry.hpp"
#include "guest/probe/window_frame.hpp"

namespace av {

namespace {

constexpr uint32_t low_part_bits = 32u;

uint32_t low_part(uint64_t luid)
{
    AV_SCOPE("low_part");
    return static_cast<uint32_t>(luid & ((uint64_t{1} << low_part_bits) - 1u));
}

}  // namespace

VulkanProbe::VulkanProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan, ImageSink &images, WindowFrame &windows,
                         void *module, void *window)
    : kmt_(kmt), clock_(kmt), vulkan_(vulkan), images_(images), windows_(windows), module_(module), window_(window),
      steps_(kmt, report, "vulkan"), adapter_luid_(0u), elapsed_(0u)
{
    AV_SCOPE("VulkanProbe::VulkanProbe");
}

void VulkanProbe::run()
{
    AV_SCOPE("VulkanProbe::run");
    {
        ProbeDevice device(vulkan_);
        if (steps_.check_result("instance", device.make_instance()) != Status::ok) return;
        if (adapter_step() != Status::ok || physical_device_step(device) != Status::ok) return;
        if (device_step(device) != Status::ok) return;
        {
            ComputeWork  compute(device);
            ProbeOutcome computed;
            compute.run(computed);
            computed.report(steps_, "compute");
        }
        {
            DrawWork     draw(device, images_);
            ProbeOutcome drawn;
            draw.run(drawn);
            drawn.report(steps_, "draw");
        }
        lost_step(device);
    }
    recreated_step();
}

/* A window of one's own, a surface on it, and the frames the driver puts
 * there. They run on the recreated device, which outlives them: everything a
 * swapchain holds goes with the device it was made on. */
void VulkanProbe::window_steps(ProbeDevice &device)
{
    AV_SCOPE("VulkanProbe::window_steps");
    if (window_ == nullptr || device.device() == VK_NULL_HANDLE) return;
    PresentWork present(device, windows_, images_, module_, window_);
    if (!surface_step(present)) return;
    if (!present_step(present)) return;
    pacing_step();
    resized_step(present);
}

bool VulkanProbe::surface_step(PresentWork &present)
{
    AV_SCOPE("VulkanProbe::surface_step");
    ProbeOutcome outcome;
    const bool   offered = present.make_surface(outcome);
    if (offered) outcome.measure(present.width() * present.height(), extent * extent);
    outcome.report(steps_, "surface");
    return offered;
}

/* Sixty FIFO frames, and then the window itself: every pixel of it must be the
 * frame's, whatever the driver says it did. */
bool VulkanProbe::present_step(PresentWork &present)
{
    AV_SCOPE("VulkanProbe::present_step");
    ProbeOutcome outcome;
    if (!present.make_swapchain(VK_PRESENT_MODE_FIFO_KHR, outcome)) {
        outcome.report(steps_, "present");
        return false;
    }
    const uint64_t started = clock_.now();
    const bool     presented = outcome.accept(present.present_frames(frames, outcome));
    elapsed_ = clock_.now() - started;
    if (!presented) {
        outcome.report(steps_, "present");
        return false;
    }
    const uint32_t pixels = present.width() * present.height();
    const uint32_t drawn = present.drawn_on_the_window("vulkan-present", outcome);
    outcome.measure(drawn, pixels);
    outcome.report(steps_, "present");
    return drawn == pixels;
}

/* FIFO holds each frame to a vertical blank: sixty frames cannot have taken
 * less time than sixty blanks, less the one a first frame need not wait for. */
void VulkanProbe::pacing_step()
{
    AV_SCOPE("VulkanProbe::pacing_step");
    const uint64_t period = clock_.period();
    if (period == 0u) {
        static_cast<void>(steps_.check_measured("pacing", Status::not_found, 0u));
        return;
    }
    const uint64_t least = period * (frames - 1u);
    static_cast<void>(steps_.check_measured("pacing", elapsed_ >= least ? Status::ok : Status::conflict,
                                            static_cast<uint32_t>(elapsed_ / period)));
}

/* The window changes size under the swapchain: what it hands out is out of
 * date until the application makes another, which presents at the new size -
 * the background past the triangle shows that it is a frame of that size, and
 * not the last one before the resize still on the window. */
void VulkanProbe::resized_step(PresentWork &present)
{
    AV_SCOPE("VulkanProbe::resized_step");
    const uint32_t width = present.width();
    const uint32_t height = present.height();
    if (!windows_.resize(window_, width + resize_by, height + resize_by)) {
        static_cast<void>(steps_.check_measured("resized", Status::not_found, 0u));
        return;
    }
    ProbeOutcome   outcome;
    const VkResult stale = present.present_frames(1u, outcome);
    if (stale != VK_ERROR_OUT_OF_DATE_KHR) {
        static_cast<void>(steps_.check_measured("resized", Status::conflict, static_cast<uint32_t>(stale)));
        return;
    }
    ProbeOutcome again;
    if (!present.make_swapchain(VK_PRESENT_MODE_IMMEDIATE_KHR, again) ||
        !again.accept(present.present_frames(1u, again))) {
        again.report(steps_, "resized");
        return;
    }
    const uint32_t pixels = present.width() * present.height();
    again.measure(present.drawn_on_the_window("vulkan-resized", again), pixels);
    again.report(steps_, "resized");
}

/* One vertical blank, timed on an adapter and device of this step's own. */

Status VulkanProbe::adapter_step()
{
    AV_SCOPE("VulkanProbe::adapter_step");
    AdapterSearch search(kmt_);
    Status        found = search.find_avgpu(0u);
    if (found == Status::ok) found = kmt_.adapter_luid(search.adapter(), adapter_luid_);
    return steps_.check_measured("adapter_luid", found, low_part(adapter_luid_));
}

Status VulkanProbe::physical_device_step(ProbeDevice &device)
{
    AV_SCOPE("VulkanProbe::physical_device_step");
    const VkResult listed = device.list_physical_devices();
    if (listed != VK_SUCCESS) return steps_.check_result("physical_device", listed);
    if (!device.choose(adapter_luid_))
        return steps_.check_measured("physical_device", Status::not_found, device.listed());
    return steps_.check_measured("physical_device", Status::ok, low_part(adapter_luid_));
}

Status VulkanProbe::device_step(ProbeDevice &device)
{
    AV_SCOPE("VulkanProbe::device_step");
    if (!device.choose_family()) return steps_.check_measured("device", Status::not_found, 0u);
    return steps_.check_result("device", device.make_device());
}

/* The fault is the first call; the device's next call must see the loss too,
 * without the probe telling it anything. */
void VulkanProbe::lost_step(ProbeDevice &device)
{
    AV_SCOPE("VulkanProbe::lost_step");
    const VkResult faulted = vulkan_.force_context_fault(device.device());
    VkResult       unexpected = faulted;
    if (faulted == VK_ERROR_DEVICE_LOST) unexpected = vulkan_.device_wait_idle(device.device());
    if (unexpected == VK_ERROR_DEVICE_LOST) {
        static_cast<void>(steps_.check("lost", Status::ok));
        return;
    }
    static_cast<void>(steps_.check_measured("lost", Status::conflict, static_cast<uint32_t>(unexpected)));
}

void VulkanProbe::recreated_step()
{
    AV_SCOPE("VulkanProbe::recreated_step");
    ProbeDevice  device(vulkan_);
    ProbeOutcome outcome;
    const bool   made = outcome.accept(device.make_instance()) && outcome.accept(device.list_physical_devices());
    if (!made) {
        outcome.report(steps_, "recreated");
        return;
    }
    if (!device.choose(adapter_luid_) || !device.choose_family()) {
        outcome.missing();
        outcome.report(steps_, "recreated");
        return;
    }
    if (outcome.accept(device.make_device())) {
        ComputeWork compute(device);
        compute.run(outcome);
    }
    outcome.report(steps_, "recreated");
    window_steps(device);
}

}  // namespace av

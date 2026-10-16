#include "icd/vulkan/swapchain.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/slot_format.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/common/window_extent.hpp"
#include "icd/common/window_surface.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

namespace {

constexpr uint32_t pixel_bytes = 4u;
/* An acquire waiting for the present thread looks again this often. */
constexpr uint32_t acquire_milliseconds = 1u;
constexpr uint64_t nanoseconds_per_millisecond = 1000000u;

}  // namespace

Swapchain::Swapchain(WindowSurface &windows, WindowChains &chains, EntryLookup &entries, VkDevice device,
                     uint64_t host_device, VkPhysicalDevice physical, Kmt &kmt, DriverLog &log,
                     Connection &connection, VkPresentQueue &presents)
    : windows_(windows), presents_(presents), device_(entries, device, physical),
      chained_(entries, device, host_device, chains, connection.transport()),
      lines_(log), pacing_(), luid_(connection.luid()),
      app_device_(device), window_(nullptr), extent_{0u, 0u}, format_(static_cast<int32_t>(VK_FORMAT_UNDEFINED)),
      holders_{}, slot_images_{}, count_(0u), next_(0u),
      through_chain_(false), retired_(false), presented_(0u), window_refused_(), chain_refused_()
{
    AV_SCOPE("Swapchain::Swapchain");
    /* Fresh, so the bind cannot be refused. */
    static_cast<void>(pacing_.bind(kmt, lines_, connection.adapter(), connection.device()));
}

Swapchain::~Swapchain()
{
    AV_SCOPE("Swapchain::~Swapchain");
    release();
}

VkResult Swapchain::initialize(const VkSwapchainCreateInfoKHR &info, void *window, SurfaceProperties &surface)
{
    AV_SCOPE("Swapchain::initialize");
    if (count_ != 0u) return VK_ERROR_INITIALIZATION_FAILED;
    const VkResult judged = judge(info, surface);
    if (judged != VK_SUCCESS) return judged;
    format_ = foreign_enum(info.imageFormat);
    /* judge() has held the format to one this driver presents, so the
     * application's value is a format the host knows. */
    const auto format = static_cast<VkFormat>(format_);
    VkResult   made = make_destination(window, format, info.imageExtent);
    for (uint32_t index = 0u; made == VK_SUCCESS && index < info.minImageCount; index++) {
        VkImage image = VK_NULL_HANDLE;
        made = device_.make_image(format, info.imageExtent, info.imageUsage, image);
    }
    if (made != VK_SUCCESS) {
        lines_.write("a swapchain's host objects could not be made");
        release();
        return made;
    }
    window_ = window;
    extent_ = info.imageExtent;
    /* judge() has held the mode to FIFO or IMMEDIATE: a blank a present, or
     * none. */
    static_cast<void>(pacing_.ask(foreign_enum(info.presentMode) == static_cast<int32_t>(VK_PRESENT_MODE_FIFO_KHR) ? 1 : 0));
    count_ = info.minImageCount;
    return VK_SUCCESS;
}

VkResult Swapchain::make_destination(void *window, VkFormat format, VkExtent2D extent)
{
    AV_SCOPE("Swapchain::make_destination");
    through_chain_ = chained_.open(window, luid_, format, extent);
    if (through_chain_) return device_.make_commands();
    return device_.make_readback(uint64_t{extent.width} * extent.height * pixel_bytes);
}

VkResult Swapchain::judge(const VkSwapchainCreateInfoKHR &info, SurfaceProperties &surface) const
{
    AV_SCOPE("Swapchain::judge");
    VkSurfaceCapabilitiesKHR capabilities = {};
    const VkResult described = surface.capabilities(capabilities);
    if (described != VK_SUCCESS) return described;
    if (capabilities.currentExtent.width == 0u || capabilities.currentExtent.height == 0u)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (info.imageExtent.width != capabilities.currentExtent.width ||
        info.imageExtent.height != capabilities.currentExtent.height) {
        refused("width, the window's", info.imageExtent.width, capabilities.currentExtent.width);
        refused("height, the window's", info.imageExtent.height, capabilities.currentExtent.height);
        return VK_ERROR_OUT_OF_DATE_KHR;
    }
    /* Each check is named in the log, so a refusal says which; an application
     * reports only that the swapchain failed (Portal: "Failed to create swap
     * chain"). */
    struct Check {
        bool        holds;
        const char *what;
        uint64_t    asked;
        uint64_t    offered;
    };
    const Check checks[] = {
        {foreign_enum(info.sType) == static_cast<int32_t>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR), "sType",
         static_cast<uint32_t>(foreign_enum(info.sType)), VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR},
        {info.minImageCount >= capabilities.minImageCount && info.minImageCount <= capabilities.maxImageCount,
         "image count, the most", info.minImageCount, capabilities.maxImageCount},
        {info.imageArrayLayers == 1u, "array layers", info.imageArrayLayers, 1u},
        {(info.imageUsage & ~capabilities.supportedUsageFlags) == 0u, "usage", info.imageUsage,
         capabilities.supportedUsageFlags},
        {foreign_enum(info.preTransform) == static_cast<int32_t>(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR),
         "transform", static_cast<uint32_t>(foreign_enum(info.preTransform)), VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR},
        {foreign_enum(info.compositeAlpha) == static_cast<int32_t>(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR), "alpha",
         static_cast<uint32_t>(foreign_enum(info.compositeAlpha)), VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR},
        {surface.offers_format(foreign_enum(info.imageFormat), foreign_enum(info.imageColorSpace)),
         "format, in colour space", static_cast<uint32_t>(foreign_enum(info.imageFormat)),
         static_cast<uint32_t>(foreign_enum(info.imageColorSpace))},
        {surface.offers_mode(foreign_enum(info.presentMode)), "present mode",
         static_cast<uint32_t>(foreign_enum(info.presentMode)), 0u}};
    bool fits = true;
    for (const Check &check : checks) {
        if (check.holds) continue;
        refused(check.what, check.asked, check.offered);
        fits = false;
    }
    return fits ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

void Swapchain::refused(const char *what, uint64_t asked, uint64_t beside) const
{
    AV_SCOPE("Swapchain::refused");
    LogLine(lines_).text("a swapchain is refused on its ").text(what).text(": ").number(asked).text(" / ").number(beside).write();
}

VkResult Swapchain::images(uint32_t &count, VkImage *images) const
{
    AV_SCOPE("Swapchain::images");
    if (images == nullptr) {
        count = count_;
        return VK_SUCCESS;
    }
    const uint32_t written = count < count_ ? count : count_;
    for (uint32_t index = 0u; index < written; index++) images[index] = device_.made(index).image_;
    count = written;
    return written < count_ ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult Swapchain::acquire(uint64_t timeout, uint32_t &index)
{
    AV_SCOPE("Swapchain::acquire");
    if (retired_) return VK_ERROR_OUT_OF_DATE_KHR;
    const VkResult state = window_state();
    if (state != VK_SUCCESS) return state;
    for (uint64_t waited = 0u;; waited += acquire_milliseconds * nanoseconds_per_millisecond) {
        bool presenting = false;
        bool found = false;
        presents_.acquire();
        for (uint32_t step = 0u; step < count_ && !found; step++) {
            const uint32_t candidate = (next_ + step) % count_;
            presenting = presenting || holders_[candidate] == Holder::presents;
            if (holders_[candidate] != Holder::none) continue;
            holders_[candidate] = Holder::application;
            next_ = (candidate + 1u) % count_;
            index = candidate;
            found = true;
        }
        presents_.release();
        if (found) return VK_SUCCESS;
        if (timeout == 0u) return VK_NOT_READY;
        if (!presenting || waited >= timeout) return VK_TIMEOUT;
        presents_.await_shown(acquire_milliseconds);
    }
}

VkResult Swapchain::present(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("Swapchain::present");
    if (!holds(index)) return VK_ERROR_UNKNOWN;
    const VkResult state = window_state();
    if (state != VK_SUCCESS) {
        give(index, Holder::none);
        return state;
    }
    if (!pacing_.admits()) {
        give(index, Holder::none);
        return device_.skip(queue, wait_count, waits);
    }
    /* A retired swapchain gave its slot back, and the window's chain shows
     * its successor's frames: what it presents is consumed and shown
     * nowhere. */
    if (through_chain_ && !chained_.opened()) {
        give(index, Holder::none);
        const VkResult skipped = device_.skip(queue, wait_count, waits);
        return skipped != VK_SUCCESS ? skipped : VK_ERROR_OUT_OF_DATE_KHR;
    }
    const VkResult shown = presents_.start() ? hand_over(queue, index, wait_count, waits)
                                             : present_now(queue, index, wait_count, waits);
    if (shown != VK_SUCCESS) return shown;
    presented_++;
    pacing_.after_present();
    return VK_SUCCESS;
}

/* A chain has one slot, so its frames go one at a time; read-back frames
 * take turns in two. A thread that freed no slot in time drops the frame,
 * whose semaphores are consumed all the same. */
VkResult Swapchain::hand_over(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("Swapchain::hand_over");
    const uint32_t slots = through_chain_ ? 1u : PresentDevice::slots;
    const uint32_t slot = presents_.free_slot(*this, slots, slots);
    if (slot == slots) {
        give(index, Holder::none);
        return device_.skip(queue, wait_count, waits);
    }
    const VkResult copied = copy(queue, index, wait_count, waits, slot);
    if (copied != VK_SUCCESS) {
        give(index, Holder::none);
        return copied;
    }
    presents_.acquire();
    holders_[index] = Holder::presents;
    slot_images_[slot] = index;
    presents_.release();
    if (presents_.hand(frame(slot))) return VK_SUCCESS;
    /* No row for it: the copy is seen through here, and the frame dropped. */
    const VkResult finished = device_.finish(slot);
    give(index, Holder::none);
    return finished;
}

VkResult Swapchain::present_now(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("Swapchain::present_now");
    VkResult copied = copy(queue, index, wait_count, waits, 0u);
    if (copied == VK_SUCCESS) copied = device_.finish(0u);
    give(index, Holder::none);
    if (copied != VK_SUCCESS) return copied;
    if (presents_.show_now(frame(0u))) return VK_SUCCESS;
    if (window_refused_.first()) lines_.write("a presented frame was refused by its window; later ones are not logged");
    const VkResult state = window_state();
    return state != VK_SUCCESS ? state : VK_ERROR_OUT_OF_DATE_KHR;
}

VkResult Swapchain::copy(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits,
                         uint32_t slot)
{
    AV_SCOPE("Swapchain::copy");
    const VkImage image = device_.made(index).image_;
    if (through_chain_) return device_.copy_into(queue, image, chained_.slot(), extent_, wait_count, waits, slot);
    return device_.copy(queue, image, extent_, wait_count, waits, slot);
}

VkFrame Swapchain::frame(uint32_t slot)
{
    AV_SCOPE("Swapchain::frame");
    const VkFrame::Rows rows = shows_bgra() ? VkFrame::Rows::bgra_top_down : VkFrame::Rows::rgba_top_down;
    return VkFrame(*this, slot, through_chain_ ? nullptr : device_.readback(slot), rows, extent_.width,
                   extent_.height, window_, FenceReady::Mark{app_device_, device_.fence(slot)});
}

void Swapchain::done(const VkFrame &frame, bool drawn)
{
    AV_SCOPE("Swapchain::done");
    const uint32_t slot = frame.slot();
    if (slot >= PresentDevice::slots) return;
    if (slot_images_[slot] < PresentDevice::max_images) holders_[slot_images_[slot]] = Holder::none;
    if (!drawn) device_.doubt(slot);
}

bool Swapchain::holds(uint32_t index)
{
    AV_SCOPE("Swapchain::holds");
    if (index >= count_) return false;
    presents_.acquire();
    const bool held = holders_[index] == Holder::application;
    presents_.release();
    return held;
}

void Swapchain::give(uint32_t index, Holder holder)
{
    AV_SCOPE("Swapchain::give");
    presents_.acquire();
    holders_[index] = holder;
    presents_.release();
}

/* The window as it is now: gone, a different size, or the swapchain's. */
VkResult Swapchain::window_state() const
{
    AV_SCOPE("Swapchain::window_state");
    WindowExtent window(windows_, window_, extent_.width, extent_.height);
    switch (window.look()) {
    case WindowExtent::Look::gone: return VK_ERROR_SURFACE_LOST_KHR;
    case WindowExtent::Look::same: return VK_SUCCESS;
    default: return VK_ERROR_OUT_OF_DATE_KHR;
    }
}

bool Swapchain::shows_bgra() const
{
    AV_SCOPE("Swapchain::shows_bgra");
    SlotFormat slot(SlotLayout::rgba_top_down);
    return slot.take_vk(static_cast<uint32_t>(format_)) && slot.bgra();
}

/* What the thread holds of this swapchain is shown first: it may be
 * presenting the slot that goes back. */
void Swapchain::retire()
{
    AV_SCOPE("Swapchain::retire");
    retired_ = true;
    presents_.settle(*this);
    chained_.release();
}

VkDevice Swapchain::device() const
{
    AV_SCOPE("Swapchain::device");
    return app_device_;
}

uint64_t Swapchain::presented() const
{
    AV_SCOPE("Swapchain::presented");
    return presented_;
}

PresentPacing &Swapchain::pacing()
{
    AV_SCOPE("Swapchain::pacing");
    return pacing_;
}

/* A retired swapchain's window is its successor's, chain and all. */
void Swapchain::release()
{
    AV_SCOPE("Swapchain::release");
    presents_.settle(*this);
    if (retired_) chained_.release();
    else chained_.close();
    device_.release();
    through_chain_ = false;
    count_ = 0u;
    for (Holder &holder : holders_) holder = Holder::none;
}

/* A made swapchain has its images and the device's; one that goes through a
 * chain has its slot until it is retired; it holds only images it has; an
 * unmade one holds nothing. */
bool Swapchain::invariants_hold() const
{
    AV_SCOPE("Swapchain::invariants_hold");
    if (!device_.invariants_hold() || !pacing_.invariants_hold() || !chained_.invariants_hold()) return false;
    if (count_ > PresentDevice::max_images) return false;
    if (count_ != 0u && device_.images() != count_) return false;
    if (count_ != 0u && through_chain_ && !retired_ && !chained_.opened()) return false;
    /* Every counted image is the device's own invariant; what is this one's is
     * that nothing past the count is held. */
    for (uint32_t index = count_; index < PresentDevice::max_images; index++)
        if (holders_[index] != Holder::none) return false;
    return next_ < (count_ != 0u ? count_ : 1u);
}

}  // namespace av::icd

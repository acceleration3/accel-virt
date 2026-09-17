#include "icd/vulkan/swapchain.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/common/window_surface.hpp"
#include "icd/vulkan/foreign_enum.hpp"
#include "icd/vulkan/surface_properties.hpp"

namespace av::icd {

namespace {

constexpr uint32_t pixel_bytes = 4u;

}  // namespace

Swapchain::Swapchain(BlockSource &memory, WindowSurface &windows, EntryLookup &entries, VkDevice device,
                     VkPhysicalDevice physical, Kmt &kmt, LogSink &log, uint32_t adapter, uint32_t kmt_device)
    : memory_(memory), windows_(windows), device_(entries, device, physical), pacer_(kmt, log, adapter, kmt_device),
      log_(log), app_device_(device), window_(nullptr), extent_{0u, 0u},
      format_(static_cast<int32_t>(VK_FORMAT_UNDEFINED)), mode_(static_cast<int32_t>(VK_PRESENT_MODE_FIFO_KHR)),
      held_{}, count_(0u), next_(0u), frame_(nullptr), retired_(false),
      presented_(0u)
{
    AV_SCOPE("Swapchain::Swapchain");
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
    const uint64_t bytes = uint64_t{info.imageExtent.width} * info.imageExtent.height * pixel_bytes;
    frame_ = static_cast<uint8_t *>(memory_.allocate(static_cast<size_t>(bytes), Pool::paged));
    if (frame_ == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkResult made = device_.make_readback(bytes);
    /* judge() has held the format to one this driver presents, so the
     * application's value is a format the host knows. */
    const auto format = static_cast<VkFormat>(foreign_enum(info.imageFormat));
    for (uint32_t index = 0u; made == VK_SUCCESS && index < info.minImageCount; index++) {
        VkImage image = VK_NULL_HANDLE;
        made = device_.make_image(format, info.imageExtent, info.imageUsage, image);
    }
    if (made != VK_SUCCESS) {
        log_.write("rvkvk: a swapchain's host objects could not be made");
        release();
        return made;
    }
    window_ = window;
    extent_ = info.imageExtent;
    format_ = foreign_enum(info.imageFormat);
    mode_ = foreign_enum(info.presentMode);
    count_ = info.minImageCount;
    return VK_SUCCESS;
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
        info.imageExtent.height != capabilities.currentExtent.height)
        return VK_ERROR_OUT_OF_DATE_KHR;
    const bool fits = foreign_enum(info.sType) == static_cast<int32_t>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR) &&
                      info.minImageCount >= capabilities.minImageCount &&
                      info.minImageCount <= capabilities.maxImageCount && info.imageArrayLayers == 1u &&
                      (info.imageUsage & ~capabilities.supportedUsageFlags) == 0u &&
                      foreign_enum(info.preTransform) ==
                          static_cast<int32_t>(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) &&
                      foreign_enum(info.compositeAlpha) ==
                          static_cast<int32_t>(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) &&
                      surface.offers_format(foreign_enum(info.imageFormat), foreign_enum(info.imageColorSpace)) &&
                      surface.offers_mode(foreign_enum(info.presentMode));
    return fits ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
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
    for (uint32_t step = 0u; step < count_; step++) {
        const uint32_t candidate = (next_ + step) % count_;
        if (held_[candidate]) continue;
        held_[candidate] = true;
        next_ = (candidate + 1u) % count_;
        index = candidate;
        return VK_SUCCESS;
    }
    return timeout == 0u ? VK_NOT_READY : VK_TIMEOUT;
}

VkResult Swapchain::present(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("Swapchain::present");
    if (index >= count_ || !held_[index]) return VK_ERROR_UNKNOWN;
    held_[index] = false;
    const VkResult state = window_state();
    if (state != VK_SUCCESS) return state;
    const VkResult copied = device_.copy(queue, device_.made(index).image_, extent_, wait_count, waits);
    if (copied != VK_SUCCESS) return copied;
    to_window_order(device_.readback());
    if (!windows_.blit_window(window_, extent_.width, extent_.height, frame_)) {
        log_.write("rvkvk: a presented frame was refused by its window");
        return window_state();
    }
    presented_++;
    if (mode_ == static_cast<int32_t>(VK_PRESENT_MODE_FIFO_KHR)) pacer_.wait();
    return VK_SUCCESS;
}

/* The window as it is now: gone, a different size, or the swapchain's. */
VkResult Swapchain::window_state() const
{
    AV_SCOPE("Swapchain::window_state");
    uint32_t width = 0u;
    uint32_t height = 0u;
    if (!windows_.client_size(window_, width, height)) return VK_ERROR_SURFACE_LOST_KHR;
    if (width != extent_.width || height != extent_.height) return VK_ERROR_OUT_OF_DATE_KHR;
    return VK_SUCCESS;
}

/* The copy's top row first becomes the window's bottom row first, and an RGBA
 * image's red and blue trade places, since a DIB is BGRA. */
void Swapchain::to_window_order(const uint8_t *rows) const
{
    AV_SCOPE("Swapchain::to_window_order");
    const bool rgba = format_ == static_cast<int32_t>(VK_FORMAT_R8G8B8A8_UNORM) ||
                      format_ == static_cast<int32_t>(VK_FORMAT_R8G8B8A8_SRGB);
    const size_t row_bytes = size_t{extent_.width} * pixel_bytes;
    for (uint32_t row = 0u; row < extent_.height; row++) {
        const uint8_t *const from = rows + size_t{extent_.height - 1u - row} * row_bytes;
        uint8_t *const       to = frame_ + size_t{row} * row_bytes;
        for (size_t at = 0u; at < row_bytes; at += pixel_bytes) {
            to[at] = from[at + (rgba ? 2u : 0u)];
            to[at + 1u] = from[at + 1u];
            to[at + 2u] = from[at + (rgba ? 0u : 2u)];
            to[at + 3u] = from[at + 3u];
        }
    }
}

void Swapchain::retire()
{
    AV_SCOPE("Swapchain::retire");
    retired_ = true;
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

FramePacer &Swapchain::pacer()
{
    AV_SCOPE("Swapchain::pacer");
    return pacer_;
}

void Swapchain::release()
{
    AV_SCOPE("Swapchain::release");
    device_.release();
    if (frame_ != nullptr) memory_.release(frame_);
    frame_ = nullptr;
    count_ = 0u;
    for (bool &held : held_) held = false;
}

/* A made swapchain has its frame buffer, its images and the device's, and
 * holds only images it has; an unmade one holds nothing. */
bool Swapchain::invariants_hold() const
{
    AV_SCOPE("Swapchain::invariants_hold");
    if (!device_.invariants_hold() || !pacer_.invariants_hold()) return false;
    if (count_ > PresentDevice::max_images) return false;
    if (count_ != 0u && (frame_ == nullptr || device_.images() != count_)) return false;
    /* Every counted image is the device's own invariant; what is this one's is
     * that nothing past the count is held. */
    for (uint32_t index = count_; index < PresentDevice::max_images; index++)
        if (held_[index]) return false;
    return next_ < (count_ != 0u ? count_ : 1u);
}

}  // namespace av::icd

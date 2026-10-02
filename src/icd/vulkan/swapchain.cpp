#include "icd/vulkan/swapchain.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/support/log_sink.hpp"
#include "icd/common/window_surface.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

namespace {

constexpr uint32_t pixel_bytes = 4u;

}  // namespace

Swapchain::Swapchain(BlockSource &memory, WindowSurface &windows, WindowChains &chains, EntryLookup &entries,
                     VkDevice device, VkPhysicalDevice physical, Kmt &kmt, LogSink &log, Connection &connection)
    : memory_(memory), windows_(windows), device_(entries, device, physical),
      chained_(entries, device, chains, connection.transport()),
      pacer_(kmt, log, connection.adapter(), connection.device()), log_(log), luid_(connection.luid()),
      app_device_(device), window_(nullptr), extent_{0u, 0u}, format_(static_cast<int32_t>(VK_FORMAT_UNDEFINED)),
      mode_(static_cast<int32_t>(VK_PRESENT_MODE_FIFO_KHR)), held_{}, count_(0u), next_(0u), frame_(nullptr),
      through_chain_(false), retired_(false), presented_(0u), window_refused_(), chain_refused_()
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
        log_.write("avvk: a swapchain's host objects could not be made");
        release();
        return made;
    }
    window_ = window;
    extent_ = info.imageExtent;
    mode_ = foreign_enum(info.presentMode);
    count_ = info.minImageCount;
    return VK_SUCCESS;
}

VkResult Swapchain::make_destination(void *window, VkFormat format, VkExtent2D extent)
{
    AV_SCOPE("Swapchain::make_destination");
    through_chain_ = chained_.open(window, luid_, format, extent);
    if (through_chain_) return device_.make_commands();
    const uint64_t bytes = uint64_t{extent.width} * extent.height * pixel_bytes;
    /* A BGRA image is a DIB's own order, so its read-back is drawn as it is;
     * only an RGBA one needs its red and blue traded somewhere first. */
    if (!shows_bgra()) {
        frame_ = static_cast<uint8_t *>(memory_.allocate(static_cast<size_t>(bytes), Pool::paged));
        if (frame_ == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return device_.make_readback(bytes);
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
    DriverLog lines(log_, "avvk: ");
    LogLine(lines).text("a swapchain is refused on its ").text(what).text(": ").number(asked).text(" / ").number(beside).write();
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
    if (mode_ == static_cast<int32_t>(VK_PRESENT_MODE_IMMEDIATE_KHR) && !pacer_.admits())
        return device_.skip(queue, wait_count, waits);
    if (through_chain_) return present_chained(queue, index, wait_count, waits);
    const VkResult copied = device_.copy(queue, device_.made(index).image_, extent_, wait_count, waits);
    if (copied != VK_SUCCESS) return copied;
    const uint8_t *rows = device_.readback();
    if (frame_ != nullptr) {
        to_bgra(rows);
        rows = frame_;
    }
    if (!windows_.blit_window_top_down(window_, extent_.width, extent_.height, rows)) {
        if (window_refused_.first()) log_.write("avvk: a presented frame was refused by its window; later ones are not logged");
        return window_state();
    }
    presented_++;
    if (mode_ == static_cast<int32_t>(VK_PRESENT_MODE_FIFO_KHR)) pacer_.wait();
    return VK_SUCCESS;
}

/* A retired swapchain gave its slot back, and the window's chain shows its
 * successor's frames: what it presents is consumed and shown nowhere. */
VkResult Swapchain::present_chained(VkQueue queue, uint32_t index, uint32_t wait_count, const VkSemaphore *waits)
{
    AV_SCOPE("Swapchain::present_chained");
    if (!chained_.opened()) {
        const VkResult skipped = device_.skip(queue, wait_count, waits);
        return skipped != VK_SUCCESS ? skipped : VK_ERROR_OUT_OF_DATE_KHR;
    }
    const VkResult copied =
        device_.copy_into(queue, device_.made(index).image_, chained_.slot(), extent_, wait_count, waits);
    if (copied != VK_SUCCESS) return copied;
    if (!chained_.show()) {
        if (chain_refused_.first())
            log_.write("avvk: a presented frame was refused by its window's chain; later ones are not logged");
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

/* An RGBA image's red and blue trade places, since a DIB is BGRA; the rows
 * keep their order, which a top-down DIB draws as they are. A pixel at a
 * time: a byte at a time over 14 MB a frame was a fifth of a game's render
 * thread. */
void Swapchain::to_bgra(const uint8_t *rows) const
{
    AV_SCOPE("Swapchain::to_bgra");
    const size_t pixels = size_t{extent_.width} * extent_.height;
    for (size_t at = 0u; at < pixels; at++) {
        uint32_t pixel = 0u;
        __builtin_memcpy(&pixel, rows + at * pixel_bytes, pixel_bytes);
        pixel = (pixel & 0xFF00FF00u) | ((pixel >> 16u) & 0xFFu) | ((pixel & 0xFFu) << 16u);
        __builtin_memcpy(frame_ + at * pixel_bytes, &pixel, pixel_bytes);
    }
}

bool Swapchain::shows_bgra() const
{
    AV_SCOPE("Swapchain::shows_bgra");
    return format_ == static_cast<int32_t>(VK_FORMAT_B8G8R8A8_UNORM) ||
           format_ == static_cast<int32_t>(VK_FORMAT_B8G8R8A8_SRGB);
}

void Swapchain::retire()
{
    AV_SCOPE("Swapchain::retire");
    retired_ = true;
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

FramePacer &Swapchain::pacer()
{
    AV_SCOPE("Swapchain::pacer");
    return pacer_;
}

/* A retired swapchain's window is its successor's, chain and all. */
void Swapchain::release()
{
    AV_SCOPE("Swapchain::release");
    if (retired_) chained_.release();
    else chained_.close();
    device_.release();
    if (frame_ != nullptr) memory_.release(frame_);
    frame_ = nullptr;
    through_chain_ = false;
    count_ = 0u;
    for (bool &held : held_) held = false;
}

/* A made swapchain has its images and the device's, and a frame buffer
 * exactly when it reads back a format that is not BGRA; one that goes through
 * a chain has its slot until it is retired; it holds only images it has; an
 * unmade one holds nothing. */
bool Swapchain::invariants_hold() const
{
    AV_SCOPE("Swapchain::invariants_hold");
    if (!device_.invariants_hold() || !pacer_.invariants_hold() || !chained_.invariants_hold()) return false;
    if (count_ > PresentDevice::max_images) return false;
    const bool reads_rgba = !through_chain_ && !shows_bgra();
    if (count_ != 0u && ((frame_ != nullptr) != reads_rgba || device_.images() != count_)) return false;
    if (count_ != 0u && through_chain_ && !retired_ && !chained_.opened()) return false;
    /* Every counted image is the device's own invariant; what is this one's is
     * that nothing past the count is held. */
    for (uint32_t index = count_; index < PresentDevice::max_images; index++)
        if (held_[index]) return false;
    return next_ < (count_ != 0u ? count_ : 1u);
}

}  // namespace av::icd

#include "icd/vulkan/presentation.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/log_sink.hpp"
#include "proto/wire/foreign_enum.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/surface_properties.hpp"

namespace av::icd {

Presentation::Presentation(BlockSource &memory, Kmt &kmt, WindowSurface &windows, LogSink &log, IcdRoot &root,
                           EntryLookup &entries)
    : memory_(memory), kmt_(kmt), windows_(windows), log_(log), root_(root), entries_(entries), surfaces_(), slots_{}
{
    AV_SCOPE("Presentation::Presentation");
}

Presentation::~Presentation()
{
    AV_SCOPE("Presentation::~Presentation");
    for (SwapchainSlot &slot : slots_) destroy(slot);
}

VkResult Presentation::create_surface(VkInstance instance, const Win32SurfaceCreateRecord *info,
                                      VkSurfaceKHR *surface)
{
    AV_SCOPE("Presentation::create_surface");
    if (info == nullptr || foreign_enum(info->type_) != static_cast<int32_t>(win32_surface_create_info)) {
        *surface = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return surfaces_.create(instance, info->window_, *surface);
}

void Presentation::destroy_surface(VkInstance instance, VkSurfaceKHR surface)
{
    AV_SCOPE("Presentation::destroy_surface");
    surfaces_.destroy(instance, surface);
}

VkResult Presentation::surface_support(VkSurfaceKHR surface, VkBool32 *supported)
{
    AV_SCOPE("Presentation::surface_support");
    *supported = VK_FALSE;
    if (surfaces_.window_of(surface) == nullptr) return VK_ERROR_SURFACE_LOST_KHR;
    /* Every queue presents: the copy runs on the queue it is handed. */
    *supported = VK_TRUE;
    return VK_SUCCESS;
}

VkResult Presentation::capabilities(VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities)
{
    AV_SCOPE("Presentation::capabilities");
    SurfaceProperties properties(windows_, surfaces_.window_of(surface));
    return properties.capabilities(*capabilities);
}

VkResult Presentation::capabilities2(const VkPhysicalDeviceSurfaceInfo2KHR *info,
                                     VkSurfaceCapabilities2KHR *capabilities)
{
    AV_SCOPE("Presentation::capabilities2");
    SurfaceProperties properties(windows_, surfaces_.window_of(info->surface));
    return properties.capabilities(capabilities->surfaceCapabilities);
}

VkResult Presentation::formats(VkSurfaceKHR surface, uint32_t *count, VkSurfaceFormatKHR *formats)
{
    AV_SCOPE("Presentation::formats");
    if (surfaces_.window_of(surface) == nullptr) return VK_ERROR_SURFACE_LOST_KHR;
    SurfaceProperties properties(windows_, surfaces_.window_of(surface));
    return properties.formats(*count, formats);
}

VkResult Presentation::formats2(const VkPhysicalDeviceSurfaceInfo2KHR *info, uint32_t *count,
                                VkSurfaceFormat2KHR *formats)
{
    AV_SCOPE("Presentation::formats2");
    if (surfaces_.window_of(info->surface) == nullptr) return VK_ERROR_SURFACE_LOST_KHR;
    SurfaceProperties properties(windows_, surfaces_.window_of(info->surface));
    return properties.formats2(*count, formats);
}

VkResult Presentation::present_modes(VkSurfaceKHR surface, uint32_t *count, VkPresentModeKHR *modes)
{
    AV_SCOPE("Presentation::present_modes");
    if (surfaces_.window_of(surface) == nullptr) return VK_ERROR_SURFACE_LOST_KHR;
    SurfaceProperties properties(windows_, surfaces_.window_of(surface));
    return properties.present_modes(*count, modes);
}

VkResult Presentation::create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                                        VkSwapchainKHR *swapchain)
{
    AV_SCOPE("Presentation::create_swapchain");
    *swapchain = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t         adapter = 0u;
    uint32_t         kmt_device = 0u;
    if (info == nullptr || !root_.present_context(device, physical, adapter, kmt_device))
        return VK_ERROR_INITIALIZATION_FAILED;
    void *const window = surfaces_.window_of(info->surface);
    if (window == nullptr) return VK_ERROR_SURFACE_LOST_KHR;
    for (SwapchainSlot &slot : slots_) {
        if (slot.chain_.constructed()) continue;
        static_cast<void>(
            slot.chain_.construct(memory_, windows_, entries_, device, physical, kmt_, log_, adapter, kmt_device));
        SurfaceProperties properties(windows_, window);
        const VkResult made = slot.chain_.object().initialize(*info, window, properties);
        if (made != VK_SUCCESS) {
            destroy(slot);
            return made;
        }
        if (Swapchain *const old = find(device, info->oldSwapchain); old != nullptr) old->retire();
        *swapchain = reinterpret_cast<VkSwapchainKHR>(&slot);
        return VK_SUCCESS;
    }
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

void Presentation::destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain)
{
    AV_SCOPE("Presentation::destroy_swapchain");
    for (SwapchainSlot &slot : slots_)
        if (reinterpret_cast<VkSwapchainKHR>(&slot) == swapchain && find(device, swapchain) != nullptr) destroy(slot);
}

VkResult Presentation::swapchain_images(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count,
                                        VkImage *images)
{
    AV_SCOPE("Presentation::swapchain_images");
    const Swapchain *const chain = find(device, swapchain);
    if (chain == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    return chain->images(*count, images);
}

VkResult Presentation::acquire(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, uint32_t *index)
{
    AV_SCOPE("Presentation::acquire");
    Swapchain *const chain = find(device, swapchain);
    if (chain == nullptr) return VK_ERROR_OUT_OF_DATE_KHR;
    return chain->acquire(timeout, *index);
}

VkResult Presentation::present(VkQueue queue, const VkPresentInfoKHR *info)
{
    AV_SCOPE("Presentation::present");
    VkResult answer = VK_SUCCESS;
    for (uint32_t index = 0u; index < info->swapchainCount; index++) {
        Swapchain *const chain = find(VK_NULL_HANDLE, info->pSwapchains[index]);
        /* The semaphores are waited on once, by the first copy. */
        const uint32_t   waits = index == 0u ? info->waitSemaphoreCount : 0u;
        const VkResult   result = chain != nullptr
                                      ? chain->present(queue, info->pImageIndices[index], waits, info->pWaitSemaphores)
                                      : VK_ERROR_OUT_OF_DATE_KHR;
        if (info->pResults != nullptr) info->pResults[index] = result;
        answer = worse(answer, result);
    }
    return answer;
}

void Presentation::forget_instance(VkInstance instance)
{
    AV_SCOPE("Presentation::forget_instance");
    surfaces_.forget(instance);
}

void Presentation::forget_device(VkDevice device)
{
    AV_SCOPE("Presentation::forget_device");
    for (SwapchainSlot &slot : slots_)
        if (slot.chain_.constructed() && slot.chain_.object().device() == device) destroy(slot);
}

uint32_t Presentation::surfaces() const
{
    AV_SCOPE("Presentation::surfaces");
    return surfaces_.live();
}

uint32_t Presentation::swapchains() const
{
    AV_SCOPE("Presentation::swapchains");
    uint32_t count = 0u;
    for (const SwapchainSlot &slot : slots_) count += slot.chain_.constructed() ? 1u : 0u;
    return count;
}

void Presentation::report(StateWriter &writer) const
{
    AV_SCOPE("Presentation::report");
    writer.open("presentation", 0u);
    writer.number("surfaces", surfaces());
    writer.number("swapchains", swapchains());
    uint64_t number = 0u;
    for (const SwapchainSlot &slot : slots_) {
        if (!slot.chain_.constructed()) continue;
        writer.open("swapchain", number++);
        writer.number("presented", slot.chain_.object().presented());
        writer.close();
    }
    writer.close();
}

/* The surfaces hold, and every swapchain a slot has holds. */
bool Presentation::invariants_hold() const
{
    AV_SCOPE("Presentation::invariants_hold");
    if (!surfaces_.invariants_hold()) return false;
    for (const SwapchainSlot &slot : slots_) {
        if (slot.chain_.constructed() && !slot.chain_.object().invariants_hold()) return false;
    }
    return true;
}

/* A swapchain this table made, of `device` unless no device is named. */
Swapchain *Presentation::find(VkDevice device, VkSwapchainKHR swapchain)
{
    AV_SCOPE("Presentation::find");
    for (SwapchainSlot &slot : slots_) {
        if (reinterpret_cast<VkSwapchainKHR>(&slot) != swapchain || !slot.chain_.constructed()) continue;
        Swapchain &chain = slot.chain_.object();
        return device == VK_NULL_HANDLE || chain.device() == device ? &chain : nullptr;
    }
    return nullptr;
}

void Presentation::destroy(SwapchainSlot &slot)
{
    AV_SCOPE("Presentation::destroy");
    slot.chain_.destroy();
}

/* An error outranks everything, and the first error is the one kept; a present
 * of this driver answers no other non-zero code. */
VkResult Presentation::worse(VkResult kept, VkResult result) const
{
    AV_SCOPE("Presentation::worse");
    if (kept < 0) return kept;
    return result < 0 ? result : VK_SUCCESS;
}

}  // namespace av::icd

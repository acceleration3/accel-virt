#include "icd/vulkan/presentation.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/driver_log.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/wire/foreign_enum.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/surface_properties.hpp"

namespace av::icd {

Presentation::Presentation(BlockSource &memory, Kmt &kmt, WindowSurface &windows, WindowChains &chains,
                           DriverLog &log, IcdRoot &root, EntryLookup &entries, ServiceThread &present_thread,
                           ThreadWake &handed, ThreadWake &shown, ThreadLock &present_lock)
    : memory_(memory), kmt_(kmt), windows_(windows), chains_(chains), log_(log), root_(root), entries_(entries),
      ready_(entries), presents_(memory, present_thread, handed, shown, present_lock, ready_, windows, chains, log),
      surfaces_(memory), chains_made_(memory), swapchains_(memory, Pool::paged)
{
    AV_SCOPE("Presentation::Presentation");
}

Presentation::~Presentation()
{
    AV_SCOPE("Presentation::~Presentation");
    swapchains_.erase_if([this](uint64_t, Swapchain *&chain) {
        static_cast<void>(chains_made_.destroy(chain));
        return true;
    });
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
    uint64_t         host_device = 0u;
    Connection      *connection = nullptr;
    if (info == nullptr || !root_.present_context(device, physical, host_device, connection)) {
        log_.write("a swapchain is refused: its device has no adapter to present on");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    void *const window = surfaces_.window_of(info->surface);
    if (window == nullptr) {
        log_.write("a swapchain is refused: its surface names no window");
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    if (Swapchain *const old = find(device, info->oldSwapchain); old != nullptr) old->retire();
    Swapchain *chain = nullptr;
    if (swapchains_.reserve(swapchains_.count() + 1u) != Status::ok ||
        chains_made_.create(chain, windows_, chains_, entries_, device, host_device, physical, kmt_, log_, *connection,
                            presents_) !=
            Status::ok)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    SurfaceProperties properties(windows_, window);
    const VkResult    made = chain->initialize(*info, window, properties);
    if (made != VK_SUCCESS) {
        static_cast<void>(chains_made_.destroy(chain));
        return made;
    }
    uint32_t slot = 0u;
    static_cast<void>(swapchains_.insert(HandleBits(chain).value(), chain, slot));
    *swapchain = HandleBits(chain).as<VkSwapchainKHR>();
    return VK_SUCCESS;
}

void Presentation::destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain)
{
    AV_SCOPE("Presentation::destroy_swapchain");
    Swapchain *const chain = find(device, swapchain);
    if (chain != nullptr) destroy(chain);
}

VkResult Presentation::swapchain_images(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count,
                                        VkImage *images)
{
    AV_SCOPE("Presentation::swapchain_images");
    const Swapchain *const chain = find(device, swapchain);
    if (chain == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    return chain->images(*count, images);
}

VkResult Presentation::acquire(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore,
                               VkFence fence, uint32_t *index)
{
    AV_SCOPE("Presentation::acquire");
    Swapchain *const chain = find(device, swapchain);
    if (chain == nullptr) return VK_ERROR_OUT_OF_DATE_KHR;
    const VkResult acquired = chain->acquire(timeout, *index);
    if (acquired != VK_SUCCESS) return acquired;
    return root_.signal_acquire(device, semaphore, fence);
}

VkResult Presentation::present(VkQueue queue, const VkPresentInfoKHR *info)
{
    AV_SCOPE("Presentation::present");
    VkResult answer = VK_SUCCESS;
    for (uint32_t index = 0u; index < info->swapchainCount; index++) {
        Swapchain *const chain = find(VK_NULL_HANDLE, info->pSwapchains[index]);
        const uint32_t   waits = index == 0u ? info->waitSemaphoreCount : 0u;
        const VkResult   result = chain != nullptr
                                      ? chain->present(queue, info->pImageIndices[index], waits, info->pWaitSemaphores)
                                      : VK_ERROR_OUT_OF_DATE_KHR;
        if (info->pResults != nullptr) info->pResults[index] = result;
        answer = worse(answer, result);
    }
    return answer;
}

void Presentation::refuse_presents()
{
    AV_SCOPE("Presentation::refuse_presents");
    presents_.refuse();
}

void Presentation::forget_instance(VkInstance instance)
{
    AV_SCOPE("Presentation::forget_instance");
    surfaces_.forget(instance);
}

void Presentation::forget_device(VkDevice device)
{
    AV_SCOPE("Presentation::forget_device");
    swapchains_.erase_if([this, device](uint64_t, Swapchain *&chain) {
        if (chain->device() != device) return false;
        static_cast<void>(chains_made_.destroy(chain));
        return true;
    });
}

uint32_t Presentation::surfaces() const
{
    AV_SCOPE("Presentation::surfaces");
    return surfaces_.live();
}

uint32_t Presentation::swapchains() const
{
    AV_SCOPE("Presentation::swapchains");
    return swapchains_.count();
}

void Presentation::report(StateWriter &writer) const
{
    AV_SCOPE("Presentation::report");
    writer.open("presentation", 0u);
    writer.number("surfaces", surfaces());
    writer.number("swapchains", swapchains());
    uint64_t number = 0u;
    swapchains_.for_each([&writer, &number](uint64_t, Swapchain *const &chain) {
        writer.open("swapchain", number++);
        writer.number("presented", chain->presented());
        writer.close();
    });
    presents_.report(writer);
    writer.close();
}

bool Presentation::invariants_hold() const
{
    AV_SCOPE("Presentation::invariants_hold");
    if (!surfaces_.invariants_hold() || swapchains_.count() != chains_made_.live()) return false;
    bool whole = true;
    swapchains_.for_each([&whole](uint64_t handle, Swapchain *const &chain) {
        whole = whole && HandleBits(chain).value() == handle && chain->invariants_hold();
    });
    return whole && swapchains_.invariants_hold();
}

Swapchain *Presentation::find(VkDevice device, VkSwapchainKHR swapchain)
{
    AV_SCOPE("Presentation::find");
    Swapchain *const *const chain = swapchains_.find(HandleBits(swapchain).value());
    if (chain == nullptr) return nullptr;
    return device == VK_NULL_HANDLE || (*chain)->device() == device ? *chain : nullptr;
}

void Presentation::destroy(Swapchain *chain)
{
    AV_SCOPE("Presentation::destroy");
    static_cast<void>(swapchains_.erase(HandleBits(chain).value()));
    static_cast<void>(chains_made_.destroy(chain));
}

VkResult Presentation::worse(VkResult kept, VkResult result) const
{
    AV_SCOPE("Presentation::worse");
    if (kept < 0) return kept;
    return result < 0 ? result : VK_SUCCESS;
}

}

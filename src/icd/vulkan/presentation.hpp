/* The ICD's WSI (feature 046, contracts/wsi.md): the surfaces and swapchains of
 * the process, and the entries that make, query, present and destroy them. */
#ifndef AV_PRESENTATION_HPP
#define AV_PRESENTATION_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/vulkan/entry_lookup.hpp"
#include "icd/vulkan/fence_ready.hpp"
#include "icd/vulkan/surface_table.hpp"
#include "icd/vulkan/swapchain.hpp"
#include "icd/vulkan/win32_surface_create_record.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class DriverLog;
}  // namespace av

namespace av {
class BlockSource;
class Kmt;
class StateWriter;
class ThreadLock;
}  // namespace av

namespace av::icd {

class IcdRoot;
class ServiceThread;
class ThreadWake;
class WindowChains;
class WindowSurface;
struct PresentationProbe;

class Presentation {
public:
    /* The present thread is `present_thread`, woken by `handed`, waking a
     * present that waits for a slot by `shown`, its tables under
     * `present_lock` (WindowPresentQueue). */
    Presentation(BlockSource &memory, Kmt &kmt, WindowSurface &windows, WindowChains &chains, DriverLog &log,
                 IcdRoot &root, EntryLookup &entries, ServiceThread &present_thread, ThreadWake &handed,
                 ThreadWake &shown, ThreadLock &present_lock);
    ~Presentation();
    Presentation(const Presentation &) = delete;
    Presentation &operator=(const Presentation &) = delete;

    VkResult create_surface(VkInstance instance, const Win32SurfaceCreateRecord *info, VkSurfaceKHR *surface);
    void     destroy_surface(VkInstance instance, VkSurfaceKHR surface);
    VkResult surface_support(VkSurfaceKHR surface, VkBool32 *supported);
    VkResult capabilities(VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities);
    VkResult capabilities2(const VkPhysicalDeviceSurfaceInfo2KHR *info, VkSurfaceCapabilities2KHR *capabilities);
    VkResult formats(VkSurfaceKHR surface, uint32_t *count, VkSurfaceFormatKHR *formats);
    VkResult formats2(const VkPhysicalDeviceSurfaceInfo2KHR *info, uint32_t *count, VkSurfaceFormat2KHR *formats);
    VkResult present_modes(VkSurfaceKHR surface, uint32_t *count, VkPresentModeKHR *modes);

    VkResult create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info, VkSwapchainKHR *swapchain);
    void     destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain);
    VkResult swapchain_images(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count, VkImage *images);
    /* The next image, with `semaphore` and `fence` signalled for it. */
    VkResult acquire(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore,
                     VkFence fence, uint32_t *index);
    /* Every swapchain of `info` in turn: each result into pResults, the worst
     * as the answer. */
    VkResult present(VkQueue queue, const VkPresentInfoKHR *info);

    /* The shim could not make what the present thread needs: presents show
     * their own frames. */
    void refuse_presents();

    /* What goes with an instance or a device. */
    void forget_instance(VkInstance instance);
    void forget_device(VkDevice device);

    uint32_t surfaces() const;
    uint32_t swapchains() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    Swapchain *find(VkDevice device, VkSwapchainKHR swapchain);
    void       destroy(Swapchain *chain);
    VkResult   worse(VkResult kept, VkResult result) const;

    BlockSource   &memory_;
    Kmt           &kmt_;
    WindowSurface &windows_;
    WindowChains  &chains_;
    DriverLog     &log_;
    IcdRoot       &root_;
    EntryLookup   &entries_;
    FenceReady     ready_;
    /* Before the swapchains, which are settled on it as they go. */
    VkPresentQueue presents_;
    SurfaceTable   surfaces_;
    /* As many swapchains as the application makes, a swapchain's handle its
     * address, found through its handle. */
    ObjectPool<Swapchain>              chains_made_;
    KeyedSlots<uint64_t, Swapchain *>  swapchains_;

    friend struct PresentationProbe;
};

}  // namespace av::icd

#endif  // AV_PRESENTATION_HPP

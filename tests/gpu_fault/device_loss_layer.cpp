#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>
#include <pthread.h>

#define AV_LOSS_EXPORT extern "C" __attribute__((visibility("default")))

namespace {

constexpr std::uint32_t max_instances = 64u;
constexpr std::uint32_t max_devices = 256u;
constexpr std::uint32_t max_submits = 64u;

void *key_of(const void *handle)
{
    return *static_cast<void *const *>(handle);
}

struct Instance {
    void                     *key;
    VkInstance                instance;
    PFN_vkGetInstanceProcAddr next;
    PFN_vkDestroyInstance     destroy;
};

struct Device {
    void                       *key;
    VkDevice                    device;
    PFN_vkGetDeviceProcAddr     next;
    PFN_vkDestroyDevice         destroy;
    PFN_vkQueueSubmit           submit;
    PFN_vkQueueSubmit2          submit2;
    PFN_vkQueueSubmit2          submit2_khr;
    PFN_vkQueueWaitIdle         queue_wait_idle;
    PFN_vkDeviceWaitIdle        device_wait_idle;
    PFN_vkWaitForFences         wait_for_fences;
    PFN_vkWaitSemaphores        wait_semaphores;
    PFN_vkWaitSemaphores        wait_semaphores_khr;
    PFN_vkGetFenceStatus        fence_status;
    PFN_vkGetEventStatus        event_status;
    PFN_vkGetSemaphoreCounterValue counter_value;
    PFN_vkGetSemaphoreCounterValue counter_value_khr;
    PFN_vkGetQueryPoolResults   query_results;
    PFN_vkQueueBindSparse       bind_sparse;
    PFN_vkQueuePresentKHR       present;
    PFN_vkAcquireNextImageKHR   acquire;
    bool                        lost;
};

class Registry {
public:
    Registry() : mutex_(PTHREAD_MUTEX_INITIALIZER), instances_{}, devices_{} {}

    void add(const Instance &instance)
    {
        pthread_mutex_lock(&mutex_);
        for (Instance &slot : instances_)
            if (slot.key == nullptr) {
                slot = instance;
                break;
            }
        pthread_mutex_unlock(&mutex_);
    }

    bool instance(const void *key, Instance &found)
    {
        pthread_mutex_lock(&mutex_);
        bool known = false;
        for (const Instance &slot : instances_)
            if (key != nullptr && slot.key == key) {
                found = slot;
                known = true;
            }
        pthread_mutex_unlock(&mutex_);
        return known;
    }

    void remove_instance(const void *key)
    {
        pthread_mutex_lock(&mutex_);
        for (Instance &slot : instances_)
            if (slot.key == key) slot = Instance{};
        pthread_mutex_unlock(&mutex_);
    }

    void add(const Device &device)
    {
        pthread_mutex_lock(&mutex_);
        for (Device &slot : devices_)
            if (slot.key == nullptr) {
                slot = device;
                break;
            }
        pthread_mutex_unlock(&mutex_);
    }

    bool device(const void *key, Device &found)
    {
        pthread_mutex_lock(&mutex_);
        bool known = false;
        for (const Device &slot : devices_)
            if (key != nullptr && slot.key == key) {
                found = slot;
                known = true;
            }
        pthread_mutex_unlock(&mutex_);
        return known;
    }

    void remove_device(const void *key)
    {
        pthread_mutex_lock(&mutex_);
        for (Device &slot : devices_)
            if (slot.key == key) slot = Device{};
        pthread_mutex_unlock(&mutex_);
    }

    void lose(const void *key)
    {
        pthread_mutex_lock(&mutex_);
        for (Device &slot : devices_)
            if (key != nullptr && slot.key == key) slot.lost = true;
        pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_;
    Instance        instances_[max_instances];
    Device          devices_[max_devices];
};

Registry registry;

template <typename Entry>
Entry device_entry(PFN_vkGetDeviceProcAddr next, VkDevice device, const char *name)
{
    return reinterpret_cast<Entry>(next(device, name));
}

Device device_of(const void *handle)
{
    Device found{};
    static_cast<void>(registry.device(key_of(handle), found));
    return found;
}

VKAPI_ATTR void VKAPI_CALL lose_device(VkDevice device)
{
    registry.lose(key_of(device));
}

VKAPI_ATTR VkResult VKAPI_CALL queue_submit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
    const Device device = device_of(queue);
    if (!device.lost) return device.submit(queue, count, submits, fence);
    if (count > max_submits) return VK_ERROR_DEVICE_LOST;
    VkSubmitInfo empty[max_submits];
    for (uint32_t index = 0u; index < count; index++) {
        empty[index] = submits[index];
        empty[index].commandBufferCount = 0u;
        empty[index].pCommandBuffers = nullptr;
    }
    static_cast<void>(device.submit(queue, count, empty, fence));
    return VK_ERROR_DEVICE_LOST;
}

VkResult submit2_lost(PFN_vkQueueSubmit2 submit, VkQueue queue, uint32_t count, const VkSubmitInfo2 *submits,
                      VkFence fence)
{
    if (count > max_submits) return VK_ERROR_DEVICE_LOST;
    VkSubmitInfo2 empty[max_submits];
    for (uint32_t index = 0u; index < count; index++) {
        empty[index] = submits[index];
        empty[index].commandBufferInfoCount = 0u;
        empty[index].pCommandBufferInfos = nullptr;
    }
    static_cast<void>(submit(queue, count, empty, fence));
    return VK_ERROR_DEVICE_LOST;
}

VKAPI_ATTR VkResult VKAPI_CALL queue_submit2(VkQueue queue, uint32_t count, const VkSubmitInfo2 *submits,
                                             VkFence fence)
{
    const Device device = device_of(queue);
    if (!device.lost) return device.submit2(queue, count, submits, fence);
    return submit2_lost(device.submit2, queue, count, submits, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL queue_submit2_khr(VkQueue queue, uint32_t count, const VkSubmitInfo2 *submits,
                                                 VkFence fence)
{
    const Device device = device_of(queue);
    if (!device.lost) return device.submit2_khr(queue, count, submits, fence);
    return submit2_lost(device.submit2_khr, queue, count, submits, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL queue_wait_idle(VkQueue queue)
{
    const Device   device = device_of(queue);
    const VkResult waited = device.queue_wait_idle(queue);
    return device.lost ? VK_ERROR_DEVICE_LOST : waited;
}

VKAPI_ATTR VkResult VKAPI_CALL device_wait_idle(VkDevice handle)
{
    const Device   device = device_of(handle);
    const VkResult waited = device.device_wait_idle(handle);
    return device.lost ? VK_ERROR_DEVICE_LOST : waited;
}

VKAPI_ATTR VkResult VKAPI_CALL wait_for_fences(VkDevice handle, uint32_t count, const VkFence *fences, VkBool32 all,
                                               uint64_t timeout)
{
    const Device   device = device_of(handle);
    const VkResult waited = device.wait_for_fences(handle, count, fences, all, timeout);
    return device.lost ? VK_ERROR_DEVICE_LOST : waited;
}

VKAPI_ATTR VkResult VKAPI_CALL wait_semaphores(VkDevice handle, const VkSemaphoreWaitInfo *info, uint64_t timeout)
{
    const Device   device = device_of(handle);
    const VkResult waited = device.wait_semaphores(handle, info, timeout);
    return device.lost ? VK_ERROR_DEVICE_LOST : waited;
}

VKAPI_ATTR VkResult VKAPI_CALL wait_semaphores_khr(VkDevice handle, const VkSemaphoreWaitInfo *info, uint64_t timeout)
{
    const Device   device = device_of(handle);
    const VkResult waited = device.wait_semaphores_khr(handle, info, timeout);
    return device.lost ? VK_ERROR_DEVICE_LOST : waited;
}

VKAPI_ATTR VkResult VKAPI_CALL fence_status(VkDevice handle, VkFence fence)
{
    const Device device = device_of(handle);
    return device.lost ? VK_ERROR_DEVICE_LOST : device.fence_status(handle, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL event_status(VkDevice handle, VkEvent event)
{
    const Device device = device_of(handle);
    return device.lost ? VK_ERROR_DEVICE_LOST : device.event_status(handle, event);
}

VKAPI_ATTR VkResult VKAPI_CALL counter_value(VkDevice handle, VkSemaphore semaphore, uint64_t *value)
{
    const Device device = device_of(handle);
    return device.lost ? VK_ERROR_DEVICE_LOST : device.counter_value(handle, semaphore, value);
}

VKAPI_ATTR VkResult VKAPI_CALL counter_value_khr(VkDevice handle, VkSemaphore semaphore, uint64_t *value)
{
    const Device device = device_of(handle);
    return device.lost ? VK_ERROR_DEVICE_LOST : device.counter_value_khr(handle, semaphore, value);
}

VKAPI_ATTR VkResult VKAPI_CALL query_results(VkDevice handle, VkQueryPool pool, uint32_t first, uint32_t count,
                                             size_t size, void *data, VkDeviceSize stride, VkQueryResultFlags flags)
{
    const Device device = device_of(handle);
    return device.lost ? VK_ERROR_DEVICE_LOST
                       : device.query_results(handle, pool, first, count, size, data, stride, flags);
}

VKAPI_ATTR VkResult VKAPI_CALL bind_sparse(VkQueue queue, uint32_t count, const VkBindSparseInfo *infos, VkFence fence)
{
    const Device device = device_of(queue);
    return device.lost ? VK_ERROR_DEVICE_LOST : device.bind_sparse(queue, count, infos, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL queue_present(VkQueue queue, const VkPresentInfoKHR *info)
{
    const Device device = device_of(queue);
    return device.lost ? VK_ERROR_DEVICE_LOST : device.present(queue, info);
}

VKAPI_ATTR VkResult VKAPI_CALL acquire_next_image(VkDevice handle, VkSwapchainKHR swapchain, uint64_t timeout,
                                                  VkSemaphore semaphore, VkFence fence, uint32_t *index)
{
    const Device device = device_of(handle);
    return device.lost ? VK_ERROR_DEVICE_LOST : device.acquire(handle, swapchain, timeout, semaphore, fence, index);
}

VKAPI_ATTR void VKAPI_CALL destroy_device(VkDevice handle, const VkAllocationCallbacks *allocator)
{
    const Device device = device_of(handle);
    registry.remove_device(key_of(handle));
    if (device.destroy != nullptr) device.destroy(handle, allocator);
}

PFN_vkVoidFunction device_proc(VkDevice device, const char *name);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get_device_proc_addr(VkDevice device, const char *name)
{
    return device_proc(device, name);
}

PFN_vkVoidFunction device_proc(VkDevice handle, const char *name)
{
    struct Wrapped {
        const char        *name;
        PFN_vkVoidFunction ours;
    };
    static const Wrapped wrapped[] = {
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(&queue_submit)},
        {"vkQueueSubmit2", reinterpret_cast<PFN_vkVoidFunction>(&queue_submit2)},
        {"vkQueueSubmit2KHR", reinterpret_cast<PFN_vkVoidFunction>(&queue_submit2_khr)},
        {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&queue_wait_idle)},
        {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&device_wait_idle)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(&wait_for_fences)},
        {"vkWaitSemaphores", reinterpret_cast<PFN_vkVoidFunction>(&wait_semaphores)},
        {"vkWaitSemaphoresKHR", reinterpret_cast<PFN_vkVoidFunction>(&wait_semaphores_khr)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(&fence_status)},
        {"vkGetEventStatus", reinterpret_cast<PFN_vkVoidFunction>(&event_status)},
        {"vkGetSemaphoreCounterValue", reinterpret_cast<PFN_vkVoidFunction>(&counter_value)},
        {"vkGetSemaphoreCounterValueKHR", reinterpret_cast<PFN_vkVoidFunction>(&counter_value_khr)},
        {"vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(&query_results)},
        {"vkQueueBindSparse", reinterpret_cast<PFN_vkVoidFunction>(&bind_sparse)},
        {"vkQueuePresentKHR", reinterpret_cast<PFN_vkVoidFunction>(&queue_present)},
        {"vkAcquireNextImageKHR", reinterpret_cast<PFN_vkVoidFunction>(&acquire_next_image)},
        {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(&destroy_device)},
    };
    if (std::strcmp(name, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&get_device_proc_addr);
    if (std::strcmp(name, "vkAvLoseDeviceAV") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&lose_device);
    Device device{};
    if (handle == VK_NULL_HANDLE || !registry.device(key_of(handle), device)) return nullptr;
    const PFN_vkVoidFunction next = device.next(handle, name);
    if (next == nullptr) return nullptr;
    for (const Wrapped &entry : wrapped)
        if (std::strcmp(name, entry.name) == 0) return entry.ours;
    return next;
}

template <typename Info, typename Link>
Link *chain_link(const Info *info, VkStructureType type)
{
    const VkBaseInStructure *next = static_cast<const VkBaseInStructure *>(info->pNext);
    for (; next != nullptr; next = next->pNext) {
        if (next->sType != type) continue;
        Link *link = const_cast<Link *>(reinterpret_cast<const Link *>(next));
        if (link->function == VK_LAYER_LINK_INFO) return link;
    }
    return nullptr;
}

VKAPI_ATTR VkResult VKAPI_CALL create_device(VkPhysicalDevice physical, const VkDeviceCreateInfo *info,
                                             const VkAllocationCallbacks *allocator, VkDevice *made)
{
    VkLayerDeviceCreateInfo *const link =
        chain_link<VkDeviceCreateInfo, VkLayerDeviceCreateInfo>(info, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO);
    if (link == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    const PFN_vkGetInstanceProcAddr next_instance = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    const PFN_vkGetDeviceProcAddr   next_device = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    Instance owner{};
    const VkInstance instance = registry.instance(key_of(physical), owner) ? owner.instance : VK_NULL_HANDLE;
    const auto create = reinterpret_cast<PFN_vkCreateDevice>(next_instance(instance, "vkCreateDevice"));
    if (create == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    const VkResult result = create(physical, info, allocator, made);
    if (result != VK_SUCCESS) return result;
    const VkDevice device = *made;
    Device kept{};
    kept.key = key_of(device);
    kept.device = device;
    kept.next = next_device;
    kept.destroy = device_entry<PFN_vkDestroyDevice>(next_device, device, "vkDestroyDevice");
    kept.submit = device_entry<PFN_vkQueueSubmit>(next_device, device, "vkQueueSubmit");
    kept.submit2 = device_entry<PFN_vkQueueSubmit2>(next_device, device, "vkQueueSubmit2");
    kept.submit2_khr = device_entry<PFN_vkQueueSubmit2>(next_device, device, "vkQueueSubmit2KHR");
    kept.queue_wait_idle = device_entry<PFN_vkQueueWaitIdle>(next_device, device, "vkQueueWaitIdle");
    kept.device_wait_idle = device_entry<PFN_vkDeviceWaitIdle>(next_device, device, "vkDeviceWaitIdle");
    kept.wait_for_fences = device_entry<PFN_vkWaitForFences>(next_device, device, "vkWaitForFences");
    kept.wait_semaphores = device_entry<PFN_vkWaitSemaphores>(next_device, device, "vkWaitSemaphores");
    kept.wait_semaphores_khr = device_entry<PFN_vkWaitSemaphores>(next_device, device, "vkWaitSemaphoresKHR");
    kept.fence_status = device_entry<PFN_vkGetFenceStatus>(next_device, device, "vkGetFenceStatus");
    kept.event_status = device_entry<PFN_vkGetEventStatus>(next_device, device, "vkGetEventStatus");
    kept.counter_value = device_entry<PFN_vkGetSemaphoreCounterValue>(next_device, device, "vkGetSemaphoreCounterValue");
    kept.counter_value_khr =
        device_entry<PFN_vkGetSemaphoreCounterValue>(next_device, device, "vkGetSemaphoreCounterValueKHR");
    kept.query_results = device_entry<PFN_vkGetQueryPoolResults>(next_device, device, "vkGetQueryPoolResults");
    kept.bind_sparse = device_entry<PFN_vkQueueBindSparse>(next_device, device, "vkQueueBindSparse");
    kept.present = device_entry<PFN_vkQueuePresentKHR>(next_device, device, "vkQueuePresentKHR");
    kept.acquire = device_entry<PFN_vkAcquireNextImageKHR>(next_device, device, "vkAcquireNextImageKHR");
    kept.lost = false;
    registry.add(kept);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroy_instance(VkInstance handle, const VkAllocationCallbacks *allocator)
{
    Instance instance{};
    const bool known = registry.instance(key_of(handle), instance);
    registry.remove_instance(key_of(handle));
    if (known && instance.destroy != nullptr) instance.destroy(handle, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL create_instance(const VkInstanceCreateInfo *info, const VkAllocationCallbacks *allocator,
                                               VkInstance *made)
{
    VkLayerInstanceCreateInfo *const link = chain_link<VkInstanceCreateInfo, VkLayerInstanceCreateInfo>(
        info, VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO);
    if (link == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    const PFN_vkGetInstanceProcAddr next = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    const auto create = reinterpret_cast<PFN_vkCreateInstance>(next(VK_NULL_HANDLE, "vkCreateInstance"));
    if (create == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    const VkResult result = create(info, allocator, made);
    if (result != VK_SUCCESS) return result;
    Instance kept{};
    kept.key = key_of(*made);
    kept.instance = *made;
    kept.next = next;
    kept.destroy = reinterpret_cast<PFN_vkDestroyInstance>(next(*made, "vkDestroyInstance"));
    registry.add(kept);
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get_instance_proc_addr(VkInstance instance, const char *name)
{
    if (std::strcmp(name, "vkGetInstanceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&get_instance_proc_addr);
    if (std::strcmp(name, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&create_instance);
    if (std::strcmp(name, "vkDestroyInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&destroy_instance);
    if (std::strcmp(name, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&create_device);
    if (std::strcmp(name, "vkGetDeviceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&get_device_proc_addr);
    Instance kept{};
    if (instance == VK_NULL_HANDLE || !registry.instance(key_of(instance), kept)) return nullptr;
    return kept.next(instance, name);
}

}

AV_LOSS_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *version)
{
    if (version == nullptr || version->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) return VK_ERROR_INITIALIZATION_FAILED;
    if (version->loaderLayerInterfaceVersion > 2u) version->loaderLayerInterfaceVersion = 2u;
    version->pfnGetInstanceProcAddr = &get_instance_proc_addr;
    version->pfnGetDeviceProcAddr = &get_device_proc_addr;
    version->pfnGetPhysicalDeviceProcAddr = nullptr;
    return VK_SUCCESS;
}

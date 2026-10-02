#include "executor/shim/dxvk_vulkan_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/dxvk_interop.hpp"

namespace av {

DxvkVulkanDevice::DxvkVulkanDevice()
    : interop_(nullptr), physical_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), create_image_(nullptr),
      destroy_image_(nullptr), requirements_(nullptr), allocate_(nullptr), free_(nullptr), bind_(nullptr),
      layout_(nullptr), memory_fd_(nullptr), create_semaphore_(nullptr), destroy_semaphore_(nullptr),
      import_semaphore_(nullptr), submit_(nullptr), queue_idle_(nullptr)
{
    AV_SCOPE("DxvkVulkanDevice::DxvkVulkanDevice");
}

DxvkVulkanDevice::~DxvkVulkanDevice()
{
    AV_SCOPE("DxvkVulkanDevice::~DxvkVulkanDevice");
    release();
}

bool DxvkVulkanDevice::connect(ID3D11Device5 *device)
{
    AV_SCOPE("DxvkVulkanDevice::connect");
    if (interop_ != nullptr) return true;
    IDXGIVkInteropDevice1 *interop = nullptr;
    if (device == nullptr ||
        FAILED(device->QueryInterface(__uuidof(IDXGIVkInteropDevice1), reinterpret_cast<void **>(&interop))))
        return false;
    VkInstance instance = VK_NULL_HANDLE;
    interop->GetVulkanHandles(&instance, &physical_, &device_);
    const auto load = [this](const char *name) { return vkGetDeviceProcAddr(device_, name); };
    create_image_ = reinterpret_cast<PFN_vkCreateImage>(load("vkCreateImage"));
    destroy_image_ = reinterpret_cast<PFN_vkDestroyImage>(load("vkDestroyImage"));
    requirements_ = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(load("vkGetImageMemoryRequirements"));
    allocate_ = reinterpret_cast<PFN_vkAllocateMemory>(load("vkAllocateMemory"));
    free_ = reinterpret_cast<PFN_vkFreeMemory>(load("vkFreeMemory"));
    bind_ = reinterpret_cast<PFN_vkBindImageMemory>(load("vkBindImageMemory"));
    layout_ = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(load("vkGetImageSubresourceLayout"));
    memory_fd_ = reinterpret_cast<PFN_vkGetMemoryFdKHR>(load("vkGetMemoryFdKHR"));
    create_semaphore_ = reinterpret_cast<PFN_vkCreateSemaphore>(load("vkCreateSemaphore"));
    destroy_semaphore_ = reinterpret_cast<PFN_vkDestroySemaphore>(load("vkDestroySemaphore"));
    import_semaphore_ = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(load("vkImportSemaphoreFdKHR"));
    submit_ = reinterpret_cast<PFN_vkQueueSubmit>(load("vkQueueSubmit"));
    queue_idle_ = reinterpret_cast<PFN_vkQueueWaitIdle>(load("vkQueueWaitIdle"));
    interop_ = interop;
    return memory_fd_ != nullptr;
}

bool DxvkVulkanDevice::connected() const
{
    AV_SCOPE("DxvkVulkanDevice::connected");
    return interop_ != nullptr && memory_fd_ != nullptr;
}

void DxvkVulkanDevice::release()
{
    AV_SCOPE("DxvkVulkanDevice::release");
    if (interop_ != nullptr) static_cast<IDXGIVkInteropDevice1 *>(interop_)->Release();
    interop_ = nullptr;
    device_ = VK_NULL_HANDLE;
    memory_fd_ = nullptr;
}

bool DxvkVulkanDevice::create_image(const VkImageCreateInfo &info, VkImage &image) const
{
    AV_SCOPE("DxvkVulkanDevice::create_image");
    image = VK_NULL_HANDLE;
    return create_image_(device_, &info, nullptr, &image) == VK_SUCCESS;
}

void DxvkVulkanDevice::destroy_image(VkImage image) const
{
    AV_SCOPE("DxvkVulkanDevice::destroy_image");
    if (image != VK_NULL_HANDLE) destroy_image_(device_, image, nullptr);
}

VkMemoryRequirements DxvkVulkanDevice::requirements(VkImage image) const
{
    AV_SCOPE("DxvkVulkanDevice::requirements");
    VkMemoryRequirements needs = {};
    requirements_(device_, image, &needs);
    return needs;
}

uint32_t DxvkVulkanDevice::memory_type(uint32_t allowed) const
{
    AV_SCOPE("DxvkVulkanDevice::memory_type");
    VkPhysicalDeviceMemoryProperties memory = {};
    vkGetPhysicalDeviceMemoryProperties(physical_, &memory);
    uint32_t any = VK_MAX_MEMORY_TYPES;
    for (uint32_t index = 0u; index < memory.memoryTypeCount; index++) {
        if ((allowed & (1u << index)) == 0u) continue;
        if ((memory.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u) return index;
        if (any == VK_MAX_MEMORY_TYPES) any = index;
    }
    return any;
}

bool DxvkVulkanDevice::bind_memory(VkImage image, const void *chain, VkDeviceSize size, uint32_t type,
                                   VkDeviceMemory &memory) const
{
    AV_SCOPE("DxvkVulkanDevice::bind_memory");
    memory = VK_NULL_HANDLE;
    VkMemoryDedicatedAllocateInfo dedicated = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, chain, image,
                                               VK_NULL_HANDLE};
    VkMemoryAllocateInfo allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &dedicated, size, type};
    if (allocate_(device_, &allocation, nullptr, &memory) != VK_SUCCESS) {
        memory = VK_NULL_HANDLE;
        return false;
    }
    return bind_(device_, image, memory, 0u) == VK_SUCCESS;
}

void DxvkVulkanDevice::free_memory(VkDeviceMemory memory) const
{
    AV_SCOPE("DxvkVulkanDevice::free_memory");
    if (memory != VK_NULL_HANDLE) free_(device_, memory, nullptr);
}

bool DxvkVulkanDevice::memory_fd(VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagBits type, int &fd) const
{
    AV_SCOPE("DxvkVulkanDevice::memory_fd");
    const VkMemoryGetFdInfoKHR asked = {VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, nullptr, memory, type};
    return memory_fd_(device_, &asked, &fd) == VK_SUCCESS;
}

VkSubresourceLayout DxvkVulkanDevice::row_layout(VkImage image) const
{
    AV_SCOPE("DxvkVulkanDevice::row_layout");
    const VkImageSubresource plane = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u};
    VkSubresourceLayout laid = {};
    layout_(device_, image, &plane, &laid);
    return laid;
}

bool DxvkVulkanDevice::wrap(const D3D11_TEXTURE2D_DESC1 &desc, VkImage image, ID3D11Texture2D *&texture) const
{
    AV_SCOPE("DxvkVulkanDevice::wrap");
    texture = nullptr;
    auto *const interop = static_cast<IDXGIVkInteropDevice1 *>(interop_);
    return SUCCEEDED(interop->CreateTexture2DFromVkImage(&desc, image, &texture));
}

bool DxvkVulkanDevice::import_timeline(int fd, VkSemaphore &semaphore) const
{
    AV_SCOPE("DxvkVulkanDevice::import_timeline");
    semaphore = VK_NULL_HANDLE;
    if (import_semaphore_ == nullptr) return false;
    VkSemaphoreTypeCreateInfo timeline = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
                                          VK_SEMAPHORE_TYPE_TIMELINE, 0u};
    const VkSemaphoreCreateInfo made = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &timeline, 0u};
    if (create_semaphore_(device_, &made, nullptr, &semaphore) != VK_SUCCESS) {
        semaphore = VK_NULL_HANDLE;
        return false;
    }
    const VkImportSemaphoreFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR, nullptr, semaphore, 0u,
                                                 VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT, fd};
    if (import_semaphore_(device_, &imported) == VK_SUCCESS) return true;
    destroy_semaphore_(device_, semaphore, nullptr);
    semaphore = VK_NULL_HANDLE;
    return false;
}

void DxvkVulkanDevice::destroy_semaphore(VkSemaphore semaphore) const
{
    AV_SCOPE("DxvkVulkanDevice::destroy_semaphore");
    if (semaphore != VK_NULL_HANDLE) destroy_semaphore_(device_, semaphore, nullptr);
}

/* DXVK submits from a thread of its own; holding its submission lock puts
 * this between two of its submissions, so everything it submits after waits. */
bool DxvkVulkanDevice::wait(VkSemaphore semaphore, uint64_t value) const
{
    AV_SCOPE("DxvkVulkanDevice::wait");
    auto *const interop = static_cast<IDXGIVkInteropDevice1 *>(interop_);
    VkQueue  queue = VK_NULL_HANDLE;
    uint32_t family = 0u;
    interop->GetSubmissionQueue(&queue, &family);
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkTimelineSemaphoreSubmitInfo values = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr, 1u, &value, 0u,
                                            nullptr};
    const VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO, &values, 1u, &semaphore, &stage, 0u, nullptr, 0u,
                                 nullptr};
    interop->LockSubmissionQueue();
    const VkResult submitted = submit_(queue, 1u, &submit, VK_NULL_HANDLE);
    interop->ReleaseSubmissionQueue();
    return submitted == VK_SUCCESS;
}

void DxvkVulkanDevice::idle() const
{
    AV_SCOPE("DxvkVulkanDevice::idle");
    if (interop_ == nullptr) return;
    auto *const interop = static_cast<IDXGIVkInteropDevice1 *>(interop_);
    VkQueue  queue = VK_NULL_HANDLE;
    uint32_t family = 0u;
    interop->GetSubmissionQueue(&queue, &family);
    interop->FlushRenderingCommands();
    interop->LockSubmissionQueue();
    static_cast<void>(queue_idle_(queue));
    interop->ReleaseSubmissionQueue();
}

}  // namespace av

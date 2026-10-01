// vk_window.exe: a Vulkan window as an application makes one - a Win32
// surface, a swapchain on it, an image acquired, filled and presented each
// frame - for a few seconds. The top half is blue and the bottom half red, so a
// screenshot shows whether the frame reached the screen the right way up and in
// the right colours, and a white bar crosses it, so one shows whether frames
// keep coming. With "resize", the window grows by a third two seconds in and
// the swapchain is made again with the old one named, as a game's is. "rgba"
// asks for R8G8B8A8 images rather than B8G8R8A8, "immediate" for IMMEDIATE
// rather than FIFO. The loader is loaded at run time: vulkan-1.dll is the
// system's. Prints the presents it made, the swapchains, and any failure.
//
//   vk_window.exe [seconds] [width] [height] [resize] [rgba] [immediate]
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <windows.h>
#include <vulkan/vulkan.h>

namespace {

PFN_vkGetInstanceProcAddr get_instance = nullptr;
VkInstance                instance = VK_NULL_HANDLE;
VkDevice                  device = VK_NULL_HANDLE;

template <typename F>
F instance_entry(const char *name) { return reinterpret_cast<F>(get_instance(instance, name)); }

#define ENTRY(name) PFN_##name name = nullptr
ENTRY(vkCreateInstance);
ENTRY(vkEnumeratePhysicalDevices);
ENTRY(vkCreateWin32SurfaceKHR);
ENTRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
ENTRY(vkGetPhysicalDeviceMemoryProperties);
ENTRY(vkCreateDevice);
ENTRY(vkGetDeviceProcAddr);
ENTRY(vkGetDeviceQueue);
ENTRY(vkCreateSwapchainKHR);
ENTRY(vkDestroySwapchainKHR);
ENTRY(vkGetSwapchainImagesKHR);
ENTRY(vkAcquireNextImageKHR);
ENTRY(vkQueuePresentKHR);
ENTRY(vkCreateSemaphore);
ENTRY(vkCreateFence);
ENTRY(vkWaitForFences);
ENTRY(vkResetFences);
ENTRY(vkCreateCommandPool);
ENTRY(vkAllocateCommandBuffers);
ENTRY(vkBeginCommandBuffer);
ENTRY(vkEndCommandBuffer);
ENTRY(vkCmdPipelineBarrier);
ENTRY(vkCmdCopyBufferToImage);
ENTRY(vkQueueSubmit);
ENTRY(vkDeviceWaitIdle);
ENTRY(vkCreateBuffer);
ENTRY(vkDestroyBuffer);
ENTRY(vkGetBufferMemoryRequirements);
ENTRY(vkAllocateMemory);
ENTRY(vkFreeMemory);
ENTRY(vkBindBufferMemory);
ENTRY(vkMapMemory);

bool                closing = false;
VkPhysicalDevice    physical = VK_NULL_HANDLE;
VkSurfaceKHR        surface = VK_NULL_HANDLE;
VkSwapchainKHR      swapchain = VK_NULL_HANDLE;
VkImage             images[8] = {};
uint32_t            image_count = 0;
VkExtent2D          extent = {};
VkBuffer            staging = VK_NULL_HANDLE;
VkDeviceMemory      staging_memory = VK_NULL_HANDLE;
VkFormat            format = VK_FORMAT_B8G8R8A8_UNORM;
VkPresentModeKHR    mode = VK_PRESENT_MODE_FIFO_KHR;

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_CLOSE) {
        closing = true;
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}

// One frame in the swapchain's channel order: blue over red, and a white
// column after it, which a present copies at a moving offset.
bool fill_staging()
{
    if (staging != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
    }
    const VkDeviceSize frame = VkDeviceSize{extent.width} * extent.height * 4u;
    VkBufferCreateInfo created = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    created.size = frame + VkDeviceSize{extent.height} * 16u * 4u;
    created.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(device, &created, nullptr, &staging) != VK_SUCCESS) return false;
    VkMemoryRequirements needs = {};
    vkGetBufferMemoryRequirements(device, staging, &needs);
    VkPhysicalDeviceMemoryProperties memory = {};
    vkGetPhysicalDeviceMemoryProperties(physical, &memory);
    uint32_t type = memory.memoryTypeCount;
    const VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t index = 0; index < memory.memoryTypeCount && type == memory.memoryTypeCount; index++)
        if ((needs.memoryTypeBits & (1u << index)) && (memory.memoryTypes[index].propertyFlags & wanted) == wanted)
            type = index;
    VkMemoryAllocateInfo allocate = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = needs.size;
    allocate.memoryTypeIndex = type;
    void *mapped = nullptr;
    if (vkAllocateMemory(device, &allocate, nullptr, &staging_memory) != VK_SUCCESS ||
        vkBindBufferMemory(device, staging, staging_memory, 0) != VK_SUCCESS ||
        vkMapMemory(device, staging_memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS)
        return false;
    const bool    rgba = format == VK_FORMAT_R8G8B8A8_UNORM;
    const uint8_t blue[4] = {static_cast<uint8_t>(rgba ? 0 : 255), 0, static_cast<uint8_t>(rgba ? 255 : 0), 255};
    const uint8_t red[4] = {static_cast<uint8_t>(rgba ? 255 : 0), 0, static_cast<uint8_t>(rgba ? 0 : 255), 255};
    uint8_t *bytes = static_cast<uint8_t *>(mapped);
    for (uint32_t y = 0; y < extent.height; y++)
        for (uint32_t x = 0; x < extent.width; x++)
            std::memcpy(bytes + (VkDeviceSize{y} * extent.width + x) * 4u, y < extent.height / 2 ? blue : red, 4);
    std::memset(bytes + frame, 255, VkDeviceSize{extent.height} * 16u * 4u);
    return true;
}

// The swapchain at the window's size, with the last one named and destroyed
// once the new one is made.
bool make_swapchain(int &made)
{
    vkDeviceWaitIdle(device);
    VkSurfaceCapabilitiesKHR capabilities = {};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &capabilities) != VK_SUCCESS) return false;
    extent = capabilities.currentExtent;
    if (extent.width == 0 || extent.height == 0) return false;
    VkSwapchainCreateInfoKHR info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface;
    info.minImageCount = capabilities.minImageCount + 1 <= capabilities.maxImageCount ? capabilities.minImageCount + 1
                                                                                      : capabilities.minImageCount;
    info.imageFormat = format;
    info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = swapchain;
    VkSwapchainKHR next = VK_NULL_HANDLE;
    const VkResult created = vkCreateSwapchainKHR(device, &info, nullptr, &next);
    if (swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = next;
    if (created != VK_SUCCESS) {
        wprintf(L"vkCreateSwapchainKHR %d\n", created);
        return false;
    }
    image_count = 8;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, images);
    made++;
    wprintf(L"swapchain %d: %ux%u, %u images\n", made, extent.width, extent.height, image_count);
    return fill_staging();
}

void barrier(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to)
{
    VkImageMemoryBarrier changed = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    changed.dstAccessMask = to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
    changed.srcAccessMask = from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
    changed.oldLayout = from;
    changed.newLayout = to;
    changed.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    changed.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    changed.image = image;
    changed.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &changed);
}

void record(VkCommandBuffer commands, VkImage image, int frame)
{
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commands, &begin);
    barrier(commands, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy regions[2] = {};
    regions[0].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    regions[0].imageExtent = {extent.width, extent.height, 1};
    regions[1].bufferOffset = VkDeviceSize{extent.width} * extent.height * 4u;
    regions[1].bufferRowLength = 16;
    regions[1].imageSubresource = regions[0].imageSubresource;
    regions[1].imageOffset = {static_cast<int32_t>((frame * 4u) % (extent.width - 16u)), 0, 0};
    regions[1].imageExtent = {16, extent.height, 1};
    vkCmdCopyBufferToImage(commands, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, regions);
    barrier(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkEndCommandBuffer(commands);
}

}  // namespace

int wmain(int argc, wchar_t **argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int  seconds = argc > 1 ? _wtoi(argv[1]) : 5;
    const int  width = argc > 2 ? _wtoi(argv[2]) : 640;
    const int  height = argc > 3 ? _wtoi(argv[3]) : 360;
    bool       resize = false;
    for (int index = 4; index < argc; index++) {
        if (!wcscmp(argv[index], L"resize")) resize = true;
        if (!wcscmp(argv[index], L"rgba")) format = VK_FORMAT_R8G8B8A8_UNORM;
        if (!wcscmp(argv[index], L"immediate")) mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    HMODULE loader = LoadLibraryW(L"vulkan-1.dll");
    if (loader == nullptr) {
        wprintf(L"no vulkan-1.dll\n");
        return 1;
    }
    get_instance = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"));
    vkCreateInstance = instance_entry<PFN_vkCreateInstance>("vkCreateInstance");
    const char *const instance_extensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
    VkApplicationInfo application = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instance_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &application;
    instance_info.enabledExtensionCount = 2;
    instance_info.ppEnabledExtensionNames = instance_extensions;
    VkResult step = vkCreateInstance(&instance_info, nullptr, &instance);
    if (step != VK_SUCCESS) {
        wprintf(L"vkCreateInstance %d\n", step);
        return 2;
    }
#define LOAD(name) name = instance_entry<PFN_##name>(#name)
    LOAD(vkEnumeratePhysicalDevices); LOAD(vkCreateWin32SurfaceKHR); LOAD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD(vkGetPhysicalDeviceMemoryProperties); LOAD(vkCreateDevice); LOAD(vkGetDeviceProcAddr);
    uint32_t count = 1;
    vkEnumeratePhysicalDevices(instance, &count, &physical);
    if (count == 0) {
        wprintf(L"no physical device\n");
        return 3;
    }
    const float             priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    const char *const device_extensions[] = {"VK_KHR_swapchain"};
    VkDeviceCreateInfo device_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_extensions;
    step = vkCreateDevice(physical, &device_info, nullptr, &device);
    if (step != VK_SUCCESS) {
        wprintf(L"vkCreateDevice %d\n", step);
        return 4;
    }
#define DEVICE(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name))
    DEVICE(vkGetDeviceQueue); DEVICE(vkCreateSwapchainKHR); DEVICE(vkDestroySwapchainKHR);
    DEVICE(vkGetSwapchainImagesKHR); DEVICE(vkAcquireNextImageKHR); DEVICE(vkQueuePresentKHR);
    DEVICE(vkCreateSemaphore); DEVICE(vkCreateFence); DEVICE(vkWaitForFences); DEVICE(vkResetFences);
    DEVICE(vkCreateCommandPool); DEVICE(vkAllocateCommandBuffers); DEVICE(vkBeginCommandBuffer);
    DEVICE(vkEndCommandBuffer); DEVICE(vkCmdPipelineBarrier); DEVICE(vkCmdCopyBufferToImage); DEVICE(vkQueueSubmit);
    DEVICE(vkDeviceWaitIdle); DEVICE(vkCreateBuffer); DEVICE(vkDestroyBuffer); DEVICE(vkGetBufferMemoryRequirements);
    DEVICE(vkAllocateMemory); DEVICE(vkFreeMemory); DEVICE(vkBindBufferMemory); DEVICE(vkMapMemory);
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, 0, 0, &queue);

    WNDCLASSW type = {};
    type.lpfnWndProc = procedure;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"vk_window";
    RegisterClassW(&type);
    RECT area = {0, 0, width, height};
    AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowW(L"vk_window", L"vk_window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100,
                                area.right - area.left, area.bottom - area.top, nullptr, nullptr, type.hInstance,
                                nullptr);
    VkWin32SurfaceCreateInfoKHR surface_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surface_info.hinstance = type.hInstance;
    surface_info.hwnd = window;
    step = vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface);
    if (step != VK_SUCCESS) {
        wprintf(L"vkCreateWin32SurfaceKHR %d\n", step);
        return 5;
    }
    int swapchains = 0;
    if (!make_swapchain(swapchains)) return 6;

    VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore acquired = VK_NULL_HANDLE, drawn = VK_NULL_HANDLE;
    vkCreateSemaphore(device, &semaphore_info, nullptr, &acquired);
    vkCreateSemaphore(device, &semaphore_info, nullptr, &drawn);
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkFence done = VK_NULL_HANDLE;
    vkCreateFence(device, &fence_info, nullptr, &done);
    VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(device, &pool_info, nullptr, &pool);
    VkCommandBufferAllocateInfo commands_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commands_info.commandPool = pool;
    commands_info.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &commands_info, &commands);

    LARGE_INTEGER frequency = {}, start = {}, now = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    int  presents = 0, out_of_date = 0, failures = 0;
    bool resized = false;
    while (!closing) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        QueryPerformanceCounter(&now);
        const double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (elapsed >= seconds) break;
        if (resize && !resized && elapsed >= 2.0) {
            resized = true;
            RECT grown = {0, 0, width * 4 / 3, height * 4 / 3};
            AdjustWindowRect(&grown, WS_OVERLAPPEDWINDOW, FALSE);
            SetWindowPos(window, nullptr, 0, 0, grown.right - grown.left, grown.bottom - grown.top,
                         SWP_NOMOVE | SWP_NOZORDER);
            wprintf(L"resized after %d presents\n", presents);
        }
        vkWaitForFences(device, 1, &done, VK_TRUE, UINT64_MAX);
        uint32_t index = 0;
        step = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &index);
        if (step == VK_ERROR_OUT_OF_DATE_KHR) {
            out_of_date++;
            if (!make_swapchain(swapchains)) Sleep(10);
            continue;
        }
        if (step != VK_SUCCESS && step != VK_SUBOPTIMAL_KHR) {
            if (failures++ < 4) wprintf(L"vkAcquireNextImageKHR %d\n", step);
            Sleep(10);
            continue;
        }
        vkResetFences(device, 1, &done);
        record(commands, images[index], presents);
        const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &acquired;
        submit.pWaitDstStageMask = &stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commands;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &drawn;
        vkQueueSubmit(queue, 1, &submit, done);
        VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &drawn;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &index;
        step = vkQueuePresentKHR(queue, &present);
        if (step == VK_SUCCESS) presents++;
        else if (step == VK_ERROR_OUT_OF_DATE_KHR) {
            out_of_date++;
            make_swapchain(swapchains);
        } else if (failures++ < 4) wprintf(L"vkQueuePresentKHR %d\n", step);
    }
    QueryPerformanceCounter(&now);
    const double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / frequency.QuadPart;
    wprintf(L"presents %d in %.2f s: %.1f a second; swapchains %d, out of date %d, failures %d\n", presents, elapsed,
            presents / elapsed, swapchains, out_of_date, failures);
    vkDeviceWaitIdle(device);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    DestroyWindow(window);
    return 0;
}

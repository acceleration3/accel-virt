#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

static uint32_t ask(VkDevice d, PFN_vkGetMemoryHostPointerPropertiesEXT fn, void *p, const char *what)
{
    VkMemoryHostPointerPropertiesEXT h = {VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT, 0, 0};
    VkResult r = fn(d, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, p, &h);
    printf("  %-28s result=%d bits=0x%x\n", what, (int)r, h.memoryTypeBits);
    return r == VK_SUCCESS ? h.memoryTypeBits : 0u;
}

int main(void)
{
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance inst;
    if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS) { puts("no instance"); return 1; }
    uint32_t n = 8; VkPhysicalDevice ds[8];
    vkEnumeratePhysicalDevices(inst, &n, ds);
    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(ds[i], &props);
        printf("GPU %u: %s\n", i, props.deviceName);
        const char *ext[] = {"VK_KHR_external_memory", "VK_EXT_external_memory_host"};
        float pri = 1.0f;
        VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueCount = 1; q.pQueuePriorities = &pri;
        VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &q;
        dci.enabledExtensionCount = 2; dci.ppEnabledExtensionNames = ext;
        VkDevice dev;
        if (vkCreateDevice(ds[i], &dci, NULL, &dev) != VK_SUCCESS) { puts("  no device"); continue; }
        PFN_vkGetMemoryHostPointerPropertiesEXT fn =
            (PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(dev, "vkGetMemoryHostPointerPropertiesEXT");
        if (!fn) { puts("  no entry point"); vkDestroyDevice(dev, NULL); continue; }
        /* The shape the rig and real QEMU both use: a memfd mapped shared. */
        {
            size_t sz = 1u<<20;
            int mfd = (int)syscall(SYS_memfd_create, "probe", 0u);
            void *p = MAP_FAILED;
            if (mfd >= 0 && ftruncate(mfd, (off_t)sz) == 0)
                p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED, mfd, 0);
            if (p != MAP_FAILED) {
                memset(p, 1, sz);
                uint32_t bits = 0;
                VkMemoryHostPointerPropertiesEXT h = {VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT, 0, 0};
                if (fn(dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, p, &h) == VK_SUCCESS)
                    bits = h.memoryTypeBits;
                int type = -1;
                for (int t = 0; t < 32; t++) if (bits & (1u<<t)) { type = t; break; }
                VkImportMemoryHostPointerInfoEXT imp = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT};
                imp.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
                imp.pHostPointer = p;
                VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                ai.pNext = &imp; ai.allocationSize = sz; ai.memoryTypeIndex = type < 0 ? 0 : (uint32_t)type;
                VkDeviceMemory mem = VK_NULL_HANDLE;
                VkResult r = vkAllocateMemory(dev, &ai, NULL, &mem);
                printf("  MEMFD SHARED %zu bytes  bits=0x%-6x type=%2d  vkAllocateMemory=%d\n", sz, bits, type, (int)r);
                if (r == VK_SUCCESS) vkFreeMemory(dev, mem, NULL);
                munmap(p, sz);
            }
            if (mfd >= 0) close(mfd);
        }
        static const size_t sizes[] = {4096, 64u<<10, 1u<<20, 8u<<20};
        VkDeviceMemory kept[8]; unsigned nkept = 0;
        for (unsigned k = 0; k < 4; k++) {
            size_t sz = sizes[k];
            void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
            if (p == MAP_FAILED) continue;
            for (size_t o = 0; o < sz; o += 4096) ((char *)p)[o] = 1;
            uint32_t bits = 0;
            VkMemoryHostPointerPropertiesEXT h = {VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT, 0, 0};
            if (fn(dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, p, &h) == VK_SUCCESS)
                bits = h.memoryTypeBits;
            int type = -1;
            for (int t = 0; t < 32; t++) if (bits & (1u<<t)) { type = t; break; }
            VkImportMemoryHostPointerInfoEXT imp = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT};
            imp.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
            imp.pHostPointer = p;
            VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.pNext = &imp; ai.allocationSize = sz; ai.memoryTypeIndex = type < 0 ? 0 : (uint32_t)type;
            VkDeviceMemory mem = VK_NULL_HANDLE;
            VkResult r = vkAllocateMemory(dev, &ai, NULL, &mem);
            printf("  import %8zu bytes  bits=0x%-6x type=%2d  vkAllocateMemory=%d\n", sz, bits, type, (int)r);
            if (r == VK_SUCCESS && nkept < 8) kept[nkept++] = mem;
        }
        for (unsigned k = 0; k < nkept; k++) vkFreeMemory(dev, kept[k], NULL);
        vkDestroyDevice(dev, NULL);
    }
    vkDestroyInstance(inst, NULL);
    return 0;
}

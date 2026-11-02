#include "harness/icd/icd_rig.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/vulkan/physical_device_view.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint8_t  first_form = 1u;
constexpr std::uint8_t  identity_node = 2u;
constexpr std::uint8_t  driver_node = 4u;
constexpr std::uint8_t  clipping_node = 8u;
constexpr std::uint32_t vulkan_1_3 = 0x00403000u;
constexpr std::uint8_t  canary = 0xA5u;
constexpr std::size_t   clipping_room = 128u;

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

class Target {
public:
    Target()
    {
        instance_ = rig_.create_instance();
        physical_device_ = rig_.physical_device(instance_);
    }

    void run(std::uint8_t flags, const std::uint8_t *reply, std::size_t length)
    {
        transport_.set_reply(reply, length);
        av::icd::PhysicalDeviceView view(transport_, av::IcdRig::adapter_luid, "", vulkan_1_3, rig_.log());
        if ((flags & first_form) != 0u) {
            VkPhysicalDeviceProperties properties = {};
            view.properties(physical_device_, &properties);
            require(properties.apiVersion <= vulkan_1_3);
            return;
        }
        run_chain(view, flags);
    }

private:
    void run_chain(av::icd::PhysicalDeviceView &view, std::uint8_t flags)
    {
        VkPhysicalDeviceIDProperties identity = {};
        identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceDriverProperties driver = {};
        driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        std::uint8_t clipping[clipping_room] = {};
        std::memset(clipping, canary, sizeof clipping);
        VkPhysicalDevicePointClippingProperties label = {};
        label.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
        std::memcpy(clipping, &label, sizeof label);
        VkPhysicalDeviceProperties2 properties = {};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        void **tail = &properties.pNext;
        if ((flags & driver_node) != 0u) { *tail = &driver; tail = &driver.pNext; }
        if ((flags & identity_node) != 0u) { *tail = &identity; tail = &identity.pNext; }
        if ((flags & clipping_node) != 0u) *tail = clipping;
        const void *const chain = properties.pNext;
        const void *const after_driver = driver.pNext;
        const void *const after_identity = identity.pNext;
        view.properties2(physical_device_, &properties);
        require(properties.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 && properties.pNext == chain);
        require(properties.properties.apiVersion <= vulkan_1_3);
        require(driver.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES && driver.pNext == after_driver);
        require(identity.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES &&
                identity.pNext == after_identity);
        std::memcpy(&label, clipping, sizeof label);
        require(label.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES && label.pNext == nullptr);
        for (std::size_t offset = sizeof label; offset < sizeof clipping; offset++) require(clipping[offset] == canary);
        if ((flags & identity_node) == 0u) return;
        const std::uint64_t luid = av::IcdRig::adapter_luid;
        require(std::memcmp(identity.deviceLUID, &luid, sizeof luid) == 0 && identity.deviceLUIDValid == VK_TRUE);
    }

    av::IcdRig        rig_;
    VkInstance        instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice  physical_device_ = VK_NULL_HANDLE;
    av::MockTransport transport_;
};

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    static Target target;
    if (size == 0u) return 0;
    target.run(data[0], data + 1, size - 1u);
    return 0;
}

#ifndef AV_ICD_PHYSICAL_DEVICE_VIEW_HPP
#define AV_ICD_PHYSICAL_DEVICE_VIEW_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class DriverLog;
}

namespace av {
class ClientTransport;
}

namespace av::vkproto {
class HandleTranslation;
}

namespace av::icd {

struct PhysicalDeviceViewProbe;

class PhysicalDeviceView {
public:
    static constexpr uint32_t max_chain = 32u;
    static constexpr uint32_t only_node = 1u;

    PhysicalDeviceView(ClientTransport &transport, vkproto::HandleTranslation &handles, uint64_t luid, const char *name, uint32_t latest_version,
                       DriverLog &log);
    PhysicalDeviceView(const PhysicalDeviceView &) = delete;
    PhysicalDeviceView &operator=(const PhysicalDeviceView &) = delete;

    void properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *answer);
    void properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *answer);

    bool invariants_hold() const;

private:
    [[nodiscard]] bool remember(VkPhysicalDeviceProperties2 *answer);
    void               restore();
    void               write_luid();
    void               write_name(VkPhysicalDeviceProperties &properties) const;

    ClientTransport    &transport_;
    vkproto::HandleTranslation &handles_;
    uint64_t            luid_;
    const char         *name_;
    uint32_t            latest_version_;
    DriverLog          &log_;
    VkBaseOutStructure *nodes_[max_chain];
    VkStructureType     types_[max_chain];
    VkBaseOutStructure *nexts_[max_chain];
    uint32_t            remembered_;

    friend struct PhysicalDeviceViewProbe;
};

}

#endif

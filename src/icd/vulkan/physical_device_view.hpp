/* The physical device's properties as this guest's adapter presents them
 * (research.md §8): the host GPU's answers, with what the guest owns written
 * over them - an API version no newer than the ICD implements, and the adapter's
 * LUID, which is how an application pairs the Vulkan device with the DXGI one.
 *
 * The reply is the host's to write, and the generated decoder writes every
 * structure's sType from it, including the application's own nodes in the
 * pNext chain. A host that relabelled a node would have the LUID written into a
 * structure too small for it. So the chain is remembered before the call and
 * put back after it, and the LUID goes only into a node the application itself
 * labelled. A chain longer than can be remembered - or a cyclic one, which the
 * encoder would follow forever - is not sent. */
#ifndef AV_ICD_PHYSICAL_DEVICE_VIEW_HPP
#define AV_ICD_PHYSICAL_DEVICE_VIEW_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class ClientTransport;
}

namespace av::icd {

struct PhysicalDeviceViewProbe;

class PhysicalDeviceView {
public:
    /* The application's structure and the nodes after it. */
    static constexpr uint32_t max_chain = 32u;
    /* An adapter is one node: the mask a valid LUID comes with. */
    static constexpr uint32_t only_node = 1u;

    /* `latest_version` is the API version the ICD implements. */
    PhysicalDeviceView(ClientTransport &transport, uint64_t luid, uint32_t latest_version, LogSink &log);
    PhysicalDeviceView(const PhysicalDeviceView &) = delete;
    PhysicalDeviceView &operator=(const PhysicalDeviceView &) = delete;

    /* The host's answer, or all zero where its reply did not reach. */
    void properties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *answer);
    void properties2(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2 *answer);

    bool invariants_hold() const;

private:
    [[nodiscard]] bool remember(VkPhysicalDeviceProperties2 *answer);
    void               restore();
    void               write_luid();

    ClientTransport    &transport_;
    uint64_t            luid_;
    uint32_t            latest_version_;
    LogSink            &log_;
    VkBaseOutStructure *nodes_[max_chain];
    VkStructureType     types_[max_chain];
    VkBaseOutStructure *nexts_[max_chain];
    uint32_t            remembered_;

    friend struct PhysicalDeviceViewProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_PHYSICAL_DEVICE_VIEW_HPP

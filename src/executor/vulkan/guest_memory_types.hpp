#ifndef AV_GUEST_MEMORY_TYPES_HPP
#define AV_GUEST_MEMORY_TYPES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct GuestMemoryTypesProbe;

class GuestMemoryTypes {
public:
    static constexpr uint32_t no_type = VK_MAX_MEMORY_TYPES;

    GuestMemoryTypes();

    void     learn(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed);
    void     properties(VkPhysicalDeviceMemoryProperties &answer) const;
    uint32_t host_type(uint32_t guest) const;
    uint32_t heap_of(uint32_t guest) const;
    uint32_t source_type(uint32_t guest) const;
    bool     placed(uint32_t guest) const;
    uint32_t guest_bits(uint32_t host_bits) const;
    uint32_t unmapped(uint32_t guest_bits) const;

    bool invariants_hold() const;

private:
    static constexpr VkMemoryPropertyFlags host_access =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    uint32_t twin_origins(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed) const;
    void     append(VkMemoryPropertyFlags flags, uint32_t heap, uint32_t host, bool backed);
    void     order();
    uint32_t ready_from(uint32_t first) const;
    bool     preceded(uint32_t type, uint32_t first) const;
    void     move_to(uint32_t from, uint32_t to);
    bool     strict_subset(VkMemoryPropertyFlags inner, VkMemoryPropertyFlags outer) const;

    VkPhysicalDeviceMemoryProperties properties_;
    uint32_t                         hosts_[VK_MAX_MEMORY_TYPES];
    uint32_t                         backed_;
    uint32_t                         host_types_;
    uint32_t                         placed_;

    friend struct GuestMemoryTypesProbe;
};

}

#endif

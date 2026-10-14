/* The memory types a Vulkan guest sees on one host GPU, and the host type
 * behind each (docs/host-memory.md).
 *
 * A guest type is host visible only where the GPU's memory source backs it:
 * every host-visible allocation is one of the source's, exported, placed in
 * the host window and imported. Every other type loses HOST_VISIBLE, and
 * HOST_COHERENT and HOST_CACHED with it, and is the host API's own.
 *
 * A heap whose device-local memory is all host visible - an integrated GPU's,
 * where each usable type is DEVICE_LOCAL|HOST_VISIBLE - gains a type that is
 * DEVICE_LOCAL alone, allocated natively as the host type it was derived
 * from. Without it every render target would be source memory the guest maps,
 * and ANV keeps an optimal image's compression metadata inside that binding,
 * where the guest's mapping corrupted it.
 *
 * The guest's types are in the order the spec requires - no type's flags a
 * strict subset of an earlier type's - so a guest index is not a host index:
 * every index and memoryTypeBits crossing between guest and host is
 * translated here. */
#ifndef AV_GUEST_MEMORY_TYPES_HPP
#define AV_GUEST_MEMORY_TYPES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct GuestMemoryTypesProbe;

class GuestMemoryTypes {
public:
    /* What host_type and source_type answer for a guest type that has none. */
    static constexpr uint32_t no_type = VK_MAX_MEMORY_TYPES;

    /* No types until learnt. */
    GuestMemoryTypes();

    /* The guest's view of `host`, whose types in `backed` the memory source
     * allocates, exports and maps. Counts past Vulkan's arrays are cut to
     * them. */
    void     learn(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed);
    void     properties(VkPhysicalDeviceMemoryProperties &answer) const;
    /* The host type an allocation of guest type `guest` is made as. */
    uint32_t host_type(uint32_t guest) const;
    /* The host type the memory source allocates for guest type `guest`:
     * no_type for one the source does not back. */
    uint32_t source_type(uint32_t guest) const;
    /* Every guest type whose host type is in `host_bits`. */
    uint32_t guest_bits(uint32_t host_bits) const;
    /* `guest_bits` without the types the source backs, unless none would be
     * left: what memory the guest never maps may be bound to. */
    uint32_t unmapped(uint32_t guest_bits) const;

    /* Counts within the arrays; every type on a host type, every host type
     * shown; host visible exactly where backed; and in the spec's order. */
    bool invariants_hold() const;

private:
    static constexpr VkMemoryPropertyFlags host_access =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    /* The host types a type DEVICE_LOCAL alone is derived from: on each heap
     * with no such type, the first the source backs that is device local and
     * host accessible and nothing else, as many as Vulkan's array has room
     * for. */
    uint32_t twin_origins(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed) const;
    void     append(VkMemoryPropertyFlags flags, uint32_t heap, uint32_t host, bool backed);
    /* Sorts the types into the spec's order, keeping the host's wherever the
     * spec allows it. */
    void     order();
    /* The first type from `first` on that no type from `first` on must
     * precede, and whether one must precede `type`. */
    uint32_t ready_from(uint32_t first) const;
    bool     preceded(uint32_t type, uint32_t first) const;
    /* Type `from` moved to `to`, the ones between shifted up by one. */
    void     move_to(uint32_t from, uint32_t to);
    bool     strict_subset(VkMemoryPropertyFlags inner, VkMemoryPropertyFlags outer) const;

    VkPhysicalDeviceMemoryProperties properties_;
    uint32_t                         hosts_[VK_MAX_MEMORY_TYPES];
    uint32_t                         backed_;
    uint32_t                         host_types_;

    friend struct GuestMemoryTypesProbe;
};

}  // namespace av

#endif  // AV_GUEST_MEMORY_TYPES_HPP

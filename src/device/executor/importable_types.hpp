/* The memory types the host GPU imports a host pointer into, for one physical
 * device, and the guest's view of that device's memory narrowed to them
 * (feature 045, T033). The guest's only mappable memory is its own RAM, which
 * reaches the host GPU as an imported host pointer, so a type that cannot take
 * one is not host visible to the guest - NVIDIA's BAR type among them. The mask
 * is the driver's (VulkanHostImport::probe_types), learnt once per physical
 * device the guest asks about. */
#ifndef AV_IMPORTABLE_TYPES_HPP
#define AV_IMPORTABLE_TYPES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct ImportableTypesProbe;

class ImportableTypes {
public:
    ImportableTypes();

    void learn(uint64_t physical, uint32_t mask);
    bool known_for(uint64_t physical) const;
    bool includes(uint32_t type) const;
    /* Clears HOST_VISIBLE, and HOST_COHERENT and HOST_CACHED, which are valid
     * only with it, from every type outside the mask; nothing else changes. */
    void restrict(VkPhysicalDeviceMemoryProperties &properties) const;

    /* A mask belongs to a device learnt. */
    bool invariants_hold() const;

private:
    uint64_t physical_;
    uint32_t mask_;

    friend struct ImportableTypesProbe;
};

}  // namespace av

#endif  // AV_IMPORTABLE_TYPES_HPP

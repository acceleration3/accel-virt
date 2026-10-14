/* Which of one physical device's memory types the guest may map
 * (docs/host-memory.md): the ones the GPU's memory source can allocate,
 * export and map, learnt once per physical device the guest asks about. The
 * guest's view of the device's memory is narrowed to them: every other type
 * is still the host's to allocate, but not host visible to the guest. */
#ifndef AV_IMPORTABLE_TYPES_HPP
#define AV_IMPORTABLE_TYPES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct ImportableTypesProbe;

class ImportableTypes {
public:
    ImportableTypes();

    /* The types `physical`'s memory source maps. */
    void learn(uint64_t physical, uint32_t mappable);
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

/* What one physical device imports the guest's memory as: which kind of handle,
 * and the memory types it takes that kind into, with the guest's view of the
 * device's memory narrowed to them (feature 045, T033).
 *
 * The guest's only mappable memory is its own RAM, which reaches the host GPU
 * as an import, so a type that cannot take one is not host visible to the
 * guest - NVIDIA's BAR type among them. The dma-buf is preferred wherever the
 * driver takes one, because a GPU that refuses a userptr over file-backed
 * pages - and guest RAM is always file-backed - takes only that; the two kinds
 * are not interchangeable afterwards, since a buffer is made for one handle
 * type. Both masks are the driver's (VulkanHostImport::probe_dmabuf_types and
 * probe_types), learnt once per physical device the guest asks about. */
#ifndef AV_IMPORTABLE_TYPES_HPP
#define AV_IMPORTABLE_TYPES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct ImportableTypesProbe;

class ImportableTypes {
public:
    ImportableTypes();

    /* What `physical` was measured to import: the dma-buf where it takes one,
     * and the host pointer otherwise. Nothing is known of a device learnt with
     * neither. */
    void learn(uint64_t physical, uint32_t by_dmabuf, uint32_t by_pointer);
    bool known_for(uint64_t physical) const;
    /* The AV_BLOB_* kind decided, or 0 before anything was learnt. */
    uint32_t kind() const;
    bool includes(uint32_t type) const;
    /* Clears HOST_VISIBLE, and HOST_COHERENT and HOST_CACHED, which are valid
     * only with it, from every type outside the mask; nothing else changes. */
    void restrict(VkPhysicalDeviceMemoryProperties &properties) const;

    /* A mask belongs to a device learnt, and a kind to a mask. */
    bool invariants_hold() const;

private:
    uint64_t physical_;
    uint32_t mask_;
    uint32_t kind_;

    friend struct ImportableTypesProbe;
};

}  // namespace av

#endif  // AV_IMPORTABLE_TYPES_HPP

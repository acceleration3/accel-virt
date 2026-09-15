/* The resource ids one rvk context owns (docs/protocol.md, Context setup): its
 * three transport resources - ring, fence page and reply slot, in that order -
 * then its chunk slots, the host-visible memory the guest binds and imports
 * (feature 045). The driver names a context's resources from its first id; the
 * device, which only knows the ring the setup named, reads the same layout back
 * from there. */
#ifndef AV_CONTEXT_RESOURCES_HPP
#define AV_CONTEXT_RESOURCES_HPP

#include <stdint.h>

namespace av {

class ContextResources {
public:
    static constexpr uint32_t transport_resources = 3u;
    static constexpr uint32_t chunk_slots = 64u;
    static constexpr uint32_t per_context = transport_resources + chunk_slots;

    explicit ContextResources(uint32_t first);

    uint32_t ring() const;
    uint32_t fence() const;
    uint32_t reply() const;
    /* The id of chunk slot `slot`, which the caller keeps below chunk_slots. */
    uint32_t chunk(uint32_t slot) const;
    /* Which chunk slot `id` is, or chunk_slots for an id that is none of this
     * context's chunks - including one a first id near the top would wrap to. */
    uint32_t chunk_slot(uint32_t id) const;

private:
    uint32_t first_;
};

}  // namespace av

#endif  // AV_CONTEXT_RESOURCES_HPP

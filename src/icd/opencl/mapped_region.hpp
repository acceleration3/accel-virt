/* One live mapping of a buffer or an image (OpenMappings keeps them).
 *
 * A guest cannot be handed the host's own pointer, so a map is one of two
 * things. Where the buffer's bytes are already in memory the guest has mapped
 * - a feature-041 host-visible blob the context lent, or pages the
 * application lent - the map costs nothing and the pointer is the guest's
 * view of the same bytes. Where they are not, the ICD stages: a private copy
 * filled at the map and written back at the unmap.
 *
 * Which of the two a buffer maps by is decided once, when it is created, and
 * the log says which. An application never sees the difference except in how
 * long a map takes. */
#ifndef AV_CLICD_MAPPED_REGION_HPP
#define AV_CLICD_MAPPED_REGION_HPP

#include "icd/common/staged_mapping.hpp"
#include "icd/opencl/cl_image_region.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class MappedRegion : public icd::StagedMapping {
public:
    /* Whose memory a mapping hands the application: the buffer's own (the
     * arena's, or pages the application lent), which the host's buffer is
     * made over, so nothing is copied; a private copy, filled at the map and
     * written back at the unmap; or the application's own
     * (CL_MEM_USE_HOST_PTR), copied the same way and never freed. */
    enum class Memory : uint8_t { shared, staged, application };

    MappedRegion();
    /* A mapping of `bytes` at `offset` into the buffer the host knows as
     * `buffer`, which the host's own map answered with `host`. */
    MappedRegion(uint64_t buffer, uint64_t host, size_t offset, size_t bytes, cl_map_flags flags, Memory memory);
    /* A staged mapping of the `region` of `image` at `origin`, in texels of
     * `element_bytes`, its rows and slices packed in `bytes`. No host map
     * stands behind it - the wire carries none - so the unmap writes it back
     * as an image write. */
    MappedRegion(uint64_t image, const size_t *origin, const size_t *region, size_t element_bytes, size_t bytes,
                 cl_map_flags flags);

    /* What the host's own map answered, which is what an unmap names: the
     * guest's memory is the application's, and the host has never heard of
     * it. */
    uint64_t      host() const;
    bool          image() const;
    ClImageRegion image_layout() const;
    /* Whether the application asked to write through this mapping into
     * memory that is not the buffer's own, which is what says the bytes have
     * to go back at the unmap. A read-only map is thrown away instead. */
    bool          writes_back() const;

    bool invariants_hold() const;

private:
    uint64_t host_;
    size_t   origin_[3];
    size_t   region_[3];
    size_t   element_bytes_;
    Memory   memory_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_MAPPED_REGION_HPP

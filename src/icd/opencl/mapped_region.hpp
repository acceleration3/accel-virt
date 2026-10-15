/* One live mapping of a buffer.
 *
 * A guest cannot be handed the host's own pointer, so a map is one of two
 * things. Where the buffer's bytes are already in memory the guest has mapped
 * - a feature-041 host-visible blob the context lent - the map costs nothing
 * and the pointer is the guest's view of the same bytes. Where they are not,
 * the ICD stages: a guest block filled at the map and written back at the
 * unmap.
 *
 * Which of the two a buffer maps by is decided once, when it is created, and
 * the log says which. An application never sees the difference except in how
 * long a map takes. */
#ifndef AV_CLICD_MAPPED_REGION_HPP
#define AV_CLICD_MAPPED_REGION_HPP

#include "icd/opencl/cl_image_region.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct MappedRegionProbe;

class MappedRegion {
public:
    /* Whose memory a mapping hands the application: the arena's, which the
     * host's buffer is made over, so nothing is copied; a block of the ICD's
     * own, copied in at the map and back at the unmap, and freed; or the
     * application's own (CL_MEM_USE_HOST_PTR), copied the same way and never
     * freed. */
    enum class Memory : uint8_t { shared, staged, application };

    MappedRegion();

    /* Takes a mapping of `bytes` at `offset` into the buffer the host knows
     * as `buffer`, whose guest memory is `at`. */
    void take(uint64_t buffer, uint64_t host, size_t offset, size_t bytes, cl_map_flags flags,
              uint8_t *at, Memory memory);
    void give_up();
    bool held() const;

    uint64_t     buffer() const;
    /* What the host's own map answered, which is what an unmap names: the
     * guest's block is the application's, and the host has never heard of
     * it. */
    uint64_t     host() const;
    size_t       offset() const;
    size_t       bytes() const;
    uint8_t     *at() const;
    /* Whether `at` is a block of the ICD's own, freed with the mapping. */
    bool         staged() const;

    /* For a mapping of an image, which no host map stands behind - the wire
     * carries none - where in the image it is, in texels, and the image's
     * element size: the staged block holds the region with its rows and
     * slices packed, and the unmap writes it back as an image write. */
    void          place_in_image(const size_t *origin, const size_t *region, size_t element_bytes);
    bool          image() const;
    ClImageRegion image_layout() const;

    /* Whether `pointer` is the one this mapping answered. An unmap names the
     * pointer the map returned, not an address inside it. */
    bool answers(const void *pointer) const;
    /* Whether the application asked to write through this mapping into
     * memory that is not the buffer's own, which is what says the bytes have
     * to go back at the unmap. A read-only map is thrown away instead. */
    bool writes_back() const;

    bool invariants_hold() const;

private:
    friend struct MappedRegionProbe;

    uint64_t     buffer_;
    uint64_t     host_;
    uint8_t     *at_;
    size_t       offset_;
    size_t       bytes_;
    cl_map_flags flags_;
    Memory       memory_;
    size_t       origin_[3];
    size_t       region_[3];
    size_t       element_bytes_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_MAPPED_REGION_HPP

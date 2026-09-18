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

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class MappedRegion {
public:
    MappedRegion();

    /* Takes a mapping of `bytes` at `offset` into the buffer the host knows
     * as `buffer`, whose guest memory is `at`. `staged` says the bytes were
     * copied in and have to be copied back. */
    void take(uint64_t buffer, uint64_t host, size_t offset, size_t bytes, cl_map_flags flags,
              uint8_t *at, bool staged);
    void give_up();
    bool held() const;

    uint64_t     buffer() const;
    /* What the host's own map answered, which is what an unmap names: the
     * guest's block is the application's, and the host has never heard of
     * it. */
    uint64_t     host() const;
    size_t       offset() const;
    size_t       bytes() const;
    cl_map_flags flags() const;
    uint8_t     *at() const;
    bool         staged() const;

    /* Whether `pointer` is the one this mapping answered. An unmap names the
     * pointer the map returned, not an address inside it. */
    bool answers(const void *pointer) const;
    /* Whether the application asked to write through this mapping, which is
     * what says the bytes have to go back at the unmap. A read-only map that
     * was staged is thrown away instead. */
    bool writes_back() const;

    bool invariants_hold() const;

private:
    uint64_t     buffer_;
    uint64_t     host_;
    uint8_t     *at_;
    size_t       offset_;
    size_t       bytes_;
    cl_map_flags flags_;
    bool         staged_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_MAPPED_REGION_HPP

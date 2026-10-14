/* The slices one Direct3D 11 context places its buffers on
 * (docs/mapped-buffers-plan.md, 3.1): the session's BufferSlices, for an
 * immediate context of a session that draws buffers in place, and none for a
 * deferred context. A deferred context's resource records are copies of the
 * immediate context's, so the slice one names is the immediate context's to
 * rename and retire: a deferred context only ever forgets it. Retired by both,
 * a slice went back free twice - once while the immediate buffer was still on
 * it - and two buffers took the same slice. */
#ifndef AV_D3D_CONTEXT_SLICES_HPP
#define AV_D3D_CONTEXT_SLICES_HPP

#include "icd/d3d/buffer_slices.hpp"

#include <stdint.h>

namespace av::d3d {

class ContextSlices {
public:
    /* No slices: what a deferred context has, and an immediate one until it
     * owns its session's. */
    ContextSlices();
    ContextSlices(const ContextSlices &) = delete;
    ContextSlices &operator=(const ContextSlices &) = delete;

    /* The immediate context's: its session's slices, null for a session
     * that has none. */
    void own(BufferSlices *slices);

    /* A slice of at least `bytes` into `slice`; false, `slice` untouched, with
     * no slices or none to take. */
    [[nodiscard]] bool place(uint64_t bytes, BufferSlices::Slice &slice);
    /* `slice` exchanged for a fresh one of at least `bytes`, the old retired
     * behind what was recorded; false, `slice` untouched, with no slices or
     * none to take. */
    [[nodiscard]] bool rename(uint64_t bytes, BufferSlices::Slice &slice);
    /* `slice` left: retired behind what was recorded when these are the
     * slices it came from, only forgotten on a context that owns none. */
    void               leave(BufferSlices::Slice &slice);

private:
    BufferSlices *slices_;
};

}  // namespace av::d3d

#endif  // AV_D3D_CONTEXT_SLICES_HPP

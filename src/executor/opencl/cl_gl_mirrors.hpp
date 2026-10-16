/* The cl_mems one context made to stand in for GL objects of the guest's
 * (GlMirror), found by the cl_mem: what an acquire and a release copy, what
 * the GL object queries answer, and what goes once the guest's last
 * reference to a cl_mem does. */
#ifndef AV_CL_GL_MIRRORS_HPP
#define AV_CL_GL_MIRRORS_HPP

#include "executor/opencl/cl_handle_registry.hpp"
#include "executor/opencl/gl_mirror.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ClGlMirrorsProbe;

class ClGlMirrors {
public:
    /* Every mirror's cl_mem is a handle the guest holds, so the registry
     * already bounds them, and a smaller table would be a second ceiling an
     * application meets first: DaVinci Resolve keeps every texture it hands
     * OpenCL for its whole session, and was refused once it held 256. */
    static constexpr uint32_t max_mirrors = ClHandleRegistry::max_handles;

    ClGlMirrors();
    ~ClGlMirrors();
    ClGlMirrors(const ClGlMirrors &) = delete;
    ClGlMirrors &operator=(const ClGlMirrors &) = delete;
    ClGlMirrors(ClGlMirrors &&) = delete;
    ClGlMirrors &operator=(ClGlMirrors &&) = delete;

    /* False for a mirror of nothing, a mem already held, when full, and when
     * no memory can be had for the table. */
    [[nodiscard]] bool remember(const GlMirror &mirror);
    /* The mirror `mem` is, or null for a mem not held. */
    GlMirror          *find(const void *mem);
    /* Forgets `mem`, answering what it was. False, `gone` untouched, for a
     * mem not held. */
    [[nodiscard]] bool forget(const void *mem, GlMirror &gone);
    /* Forgets whichever is held last: false once none is. */
    [[nodiscard]] bool forget_any(GlMirror &gone);
    /* Forgets everything without answering it, and gives the table's memory
     * back. */
    void               forget_all();
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    friend struct ClGlMirrorsProbe;

    /* Twice the mirrors, so the index is never more than half full and a
     * probe always ends at an empty bucket. */
    static constexpr uint32_t buckets = 2u * max_mirrors;
    /* 2^64 over the golden ratio: a cl_mem is an aligned pointer, whose low
     * bits are all alike, and multiplying by this spreads the ones that
     * differ into the bits a bucket is taken from (HandleRegistry). */
    static constexpr uint64_t spread = 0x9E3779B97F4A7C15ull;
    /* The mirrors and the index, in one mapping: 192 KiB at full size. */
    static constexpr size_t   table_bytes = max_mirrors * sizeof(GlMirror) + buckets * sizeof(uint32_t);

    /* Maps the table on the first mirror a context makes. */
    [[nodiscard]] bool reserve();
    /* The slot holding `mem`, or max_mirrors for none. */
    uint32_t           slot_of(const void *mem) const;
    /* The bucket holding `mem`, or the empty one it would go in. */
    uint32_t           bucket_of(const void *mem) const;
    uint32_t           home_of(const void *mem) const;
    /* Empties `bucket`, moving back the entries after it that probed past it,
     * so no lookup ever needs a tombstone to keep going. */
    void               unindex(uint32_t bucket);
    void               forget_slot(uint32_t slot, GlMirror &gone);

    /* The table lives in memory mapped for the context that shares GL
     * objects, not in the object: every OpenCL session holds one of these,
     * few ever share a GL object, and the registry's bound of mirrors in each
     * would be 12 MB of the executor's root for nothing. A fresh mapping costs
     * nothing until touched, and the held stay the first count_, so a context
     * pays for the most it held at once.
     *
     * Slots at or past count_ are never read. The index maps a cl_mem to its
     * slot, a slot plus one in each bucket, zero for empty: a lookup is one
     * short probe however many are held, and an acquire looks up every
     * object it names twice. */
    void     *table_;
    GlMirror *mirrors_;
    uint32_t *index_;
    uint32_t  count_;
};

}  // namespace av

#endif  // AV_CL_GL_MIRRORS_HPP

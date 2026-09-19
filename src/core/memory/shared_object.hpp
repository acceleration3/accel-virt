/* One shared object the adapter knows: the identity a second process opens an
 * allocation by (feature 053, contracts/shared_objects.md).
 *
 * The identity has to be minted in the kernel because the only thing that
 * reliably crosses a process boundary is the allocation private driver data
 * dxgkrnl hands the opening process (docs/kb/04-memory.md, "Allocation
 * lifetime traps"). What is here is the half of that only the kernel can
 * judge: whether an id was ever minted at all, and how many devices hold it.
 *
 * Reached under the shared-object table's mutex, so every method is legal up
 * to APC. */
#ifndef AV_SHARED_OBJECT_HPP
#define AV_SHARED_OBJECT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/shared_object_kind.hpp"

#include <stdint.h>

namespace av {

class Allocation;
struct SharedObjectProbe;

class SharedObject {
public:
    SharedObject(Platform &platform, uint64_t id, const SharedObjectKind &kind, uint64_t size,
                 Allocation &allocation);
    SharedObject(const SharedObject &) = delete;
    SharedObject &operator=(const SharedObject &) = delete;

    uint64_t                id() const;
    const SharedObjectKind &kind() const;
    uint64_t                size() const;
    /* Whether it is this allocation's identity, which is how the table finds
     * what to forget when an allocation is destroyed. */
    bool                    names(const Allocation &allocation) const;
    uint32_t                opens() const;

    /* One more device holding it. no_space at the ceiling: a count that wrapped
     * would make an object nobody holds look held, or the other way about. */
    [[nodiscard]] Status open();
    /* One fewer. invalid_parameter when nobody holds it, which is a close
     * dxgkrnl never makes and a guest cannot drive - but a count that went
     * below zero would free an object others still have open. */
    [[nodiscard]] Status close();

    bool invariants_hold() const;

private:
    /* Every device of every process could hold one; a count this size cannot
     * be reached by anything that also has a handle table. */
    static constexpr uint32_t max_opens = 0xFFFFFFFFu;

    Platform        &platform_;
    uint64_t         id_;
    SharedObjectKind kind_;
    uint64_t         size_;
    /* The allocation it names, which outlives it: the table forgets the object
     * when the allocation is destroyed. */
    Allocation      *allocation_;
    uint32_t         opens_;

    friend struct SharedObjectProbe;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_HPP

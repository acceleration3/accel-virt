/* The OpenCL objects this executor made, by kind, with the references the
 * guest holds on each.
 *
 * An OpenCL handle crosses the wire as the host ICD's own pointer, and the
 * ICD dereferences whatever it is handed - a number a guest invented, a cl_mem
 * named where a cl_context belongs, an object whose last reference is gone.
 * So the decoder asks this for the object a number names, and a number that
 * names none is a command the host refuses (feature 044, research section 2).
 *
 * References are counted because OpenCL counts them: an application that
 * retains a context and releases it once has not given the context up, and a
 * registry that forgot it then would refuse the application's next call. The
 * count only follows what the guest did, and the ICD holds at least those
 * references, so an object is forgotten no later than the ICD may free it. */
#ifndef AV_CL_HANDLE_REGISTRY_HPP
#define AV_CL_HANDLE_REGISTRY_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stdint.h>

namespace av {

/* One object the guest holds: the references it holds on it and its kind. */
struct ClHandle {
    uint32_t            references_;
    clproto::HandleKind kind_;
};

class ClHandleRegistry {
public:
    explicit ClHandleRegistry(BlockSource &blocks);
    ClHandleRegistry(const ClHandleRegistry &) = delete;
    ClHandleRegistry &operator=(const ClHandleRegistry &) = delete;

    /* Room for `objects` more, asked before the ICD makes any, so remembering
     * that many cannot fail for want of memory: no_memory when there is none,
     * which the guest is told as CL_OUT_OF_HOST_MEMORY. */
    [[nodiscard]] Status make_room(uint64_t objects);
    /* Holds `object`, which a call just answered, with one reference. One
     * already held as this kind is unchanged: enumeration answers the same
     * platform every time without a reference. conflict when the address is
     * held as another kind - the host could no longer say which object a
     * guest's number means - and no_memory when no room was made for it and
     * none can be. Null holds nothing. */
    [[nodiscard]] Status remember(clproto::HandleKind kind, void *object);
    /* The object of `kind` held under `wire`, or null: for zero, for a number
     * never held, and for one held as another kind. */
    void *resolve(clproto::HandleKind kind, uint64_t wire) const;
    /* A retain the ICD accepted. Nothing for an object not held as `kind`. */
    void  retain(clproto::HandleKind kind, void *object);
    /* A release the ICD accepted; the last one forgets the object. */
    void  release(clproto::HandleKind kind, void *object);
    /* Gives `icd` back every reference the guest still holds - once for a
     * saturated count, which no longer says how many - dependents before what
     * they depend on, then forgets everything. Platforms are never released:
     * the API has no call for it. */
    void  release_all(clproto::ServerDispatch &icd);
    void  forget_all();

    uint32_t count() const;

    /* Every object held is one, with a reference. */
    bool invariants_hold() const;

private:
    friend struct ClHandleRegistryProbe;

    /* The entry `object` is held in as `kind`, or null. */
    ClHandle *held(clproto::HandleKind kind, uint64_t object);

    KeyedSlots<uint64_t, ClHandle> handles_;
};

}  // namespace av

#endif  // AV_CL_HANDLE_REGISTRY_HPP

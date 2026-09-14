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

#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stdint.h>

namespace av {

class ClHandleRegistry {
public:
    /* Objects the guest holds, not commands it sent, and a guest that wants
     * more is refused. Half of a Vulkan context's HandleRegistry, because a
     * slot here is wider and ExecutorRoot holds one per session by value: the
     * root has to go on fitting the stack a test gives it. */
    static constexpr uint32_t max_handles = 2048u;

    ClHandleRegistry();
    ClHandleRegistry(const ClHandleRegistry &) = delete;
    ClHandleRegistry &operator=(const ClHandleRegistry &) = delete;

    /* Holds `object`, which a call just answered, with one reference. One
     * already held as this kind is unchanged: enumeration answers the same
     * platform every time without a reference. False when full, or when the
     * address is held as another kind - the host could no longer say which
     * object a guest's number means. Null holds nothing. */
    [[nodiscard]] bool remember(clproto::HandleKind kind, void *object);
    /* The object of `kind` held under `wire`, or null: for zero, for a number
     * never held, and for one held as another kind. */
    void *resolve(clproto::HandleKind kind, uint64_t wire) const;
    /* A retain the ICD accepted. Nothing for an object not held as `kind`. */
    void  retain(clproto::HandleKind kind, void *object);
    /* A release the ICD accepted; the last one forgets the object. */
    void  release(clproto::HandleKind kind, void *object);
    void  forget_all();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct ClHandleRegistryProbe;

    /* The slot holding `object` as any kind, or `count_` for none. */
    uint32_t slot_of(uint64_t object) const;

    /* One slot is the same index in all three. */
    uint64_t            objects_[max_handles];
    uint32_t            references_[max_handles];
    clproto::HandleKind kinds_[max_handles];
    uint32_t            count_;
};

}  // namespace av

#endif  // AV_CL_HANDLE_REGISTRY_HPP

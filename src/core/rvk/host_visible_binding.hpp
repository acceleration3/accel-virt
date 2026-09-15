/* One host-visible chunk bound to an rvk context (feature 045): the allocation a
 * process made of guest RAM the host GPU imports, and the resource of the
 * context's range it became on the device. The allocation stays the process's;
 * this names which of the context's chunk slots holds it. */
#ifndef AV_HOST_VISIBLE_BINDING_HPP
#define AV_HOST_VISIBLE_BINDING_HPP

#include <stdint.h>

namespace av {

class Allocation;

class HostVisibleBinding {
public:
    HostVisibleBinding();

    /* `resource_id` is never 0: that is how a free slot is told apart. */
    void              take(const Allocation &allocation, uint32_t resource_id);
    void              clear();
    bool              bound() const;
    const Allocation *allocation() const;
    uint32_t          resource_id() const;

    bool invariants_hold() const;

private:
    friend struct HostVisibleBindingProbe;

    const Allocation *allocation_;
    uint32_t          resource_id_;
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_BINDING_HPP

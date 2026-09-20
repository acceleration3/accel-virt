/* A WDDM resource: the object D3DKMTShareObjects mints an NT handle over.
 *
 * dxgkrnl's DxgkShareObjectsInternal refuses any handle whose table entry is
 * not a resource, and refuses it with no message at all, so an allocation
 * shared across processes has to belong to one (research.md §16). A resource
 * here is the handle dxgkrnl keeps and the count of allocations created into
 * it: it exists so that it can be named, and it goes when the last of them
 * does.
 *
 * It declares no invariant of its own. A resource holding no allocations is a
 * legal state - dxgkrnl may destroy a subset and take the resource in a later
 * call - so the only thing always true of the count is that it is a count. */
#ifndef AV_RESOURCE_HPP
#define AV_RESOURCE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Platform;

class Resource {
public:
    explicit Resource(Platform &platform);
    Resource(const Resource &) = delete;
    Resource &operator=(const Resource &) = delete;

    /* One more allocation created into this resource. */
    void                 took_one();
    /* One fewer. invalid_state for a resource holding none, which is a destroy
     * of an allocation this resource never held. */
    [[nodiscard]] Status released_one();
    uint32_t             allocations() const;

private:
    Platform &platform_;
    uint32_t  allocations_;
};

}  // namespace av

#endif  // AV_RESOURCE_HPP

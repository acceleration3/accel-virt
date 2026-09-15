/* The dispatchable handles this ICD has handed the loader, each at an address
 * that stays put until it is destroyed.
 *
 * The loader de-duplicates physical devices by address, and asks for the same
 * ones through vkEnumeratePhysicalDevices and vk_icdEnumerateAdapterPhysicalDevices,
 * so one host object on one connection is one handle however often it is
 * wrapped. A pointer from outside is only ever looked at after the pool says it
 * is a live handle of this registry. The registry takes no lock: IcdRoot
 * serializes it. */
#ifndef AV_ICD_HANDLE_REGISTRY_HPP
#define AV_ICD_HANDLE_REGISTRY_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/vulkan/loader_handle.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::icd {

class Connection;
struct HandleRegistryProbe;

class HandleRegistry {
public:
    explicit HandleRegistry(BlockSource &memory);
    HandleRegistry(const HandleRegistry &) = delete;
    HandleRegistry &operator=(const HandleRegistry &) = delete;

    /* The handle for `host` on `connection` as a `kind`: the one already made if
     * there is one, else a new one made through `parent`. invalid_parameter for
     * host 0, no connection, an instance given a parent, anything else given
     * none, or a parent that is not live, not of a lower kind or not on the same
     * connection; no_memory when the pool refuses. `handle` is null unless ok. */
    [[nodiscard]] Status wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent,
                              LoaderHandle *&handle);
    /* The live handle of `kind` at `pointer`, or null: a stack address, a
     * destroyed handle and a handle of another kind are not one. */
    LoaderHandle *made(void *pointer, LoaderHandle::Kind kind) const;
    /* The live instance on `connection`, or null. */
    LoaderHandle *instance_of(const Connection *connection) const;
    /* Destroys `handle` and every handle made through it, at any depth.
     * invalid_parameter for a pointer that is not a live handle. */
    [[nodiscard]] Status destroy(LoaderHandle *handle);
    uint32_t             live() const;
    /* How many of them send through `connection`. */
    uint32_t             live_on(const Connection *connection) const;
    /* How many command buffers own a batch. */
    uint32_t             batches() const;

    bool invariants_hold() const;

private:
    LoaderHandle *existing(LoaderHandle::Kind kind, uint64_t host, const Connection *connection) const;
    bool          parent_fits(LoaderHandle::Kind kind, const Connection *connection, LoaderHandle *parent) const;
    bool          unique(const LoaderHandle &handle) const;

    ObjectPool<LoaderHandle> pool_;

    friend struct HandleRegistryProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_HANDLE_REGISTRY_HPP

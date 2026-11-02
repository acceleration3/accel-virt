#ifndef AV_ICD_HANDLE_REGISTRY_HPP
#define AV_ICD_HANDLE_REGISTRY_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "icd/common/handle_tree.hpp"
#include "icd/vulkan/loader_handle.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::icd {

class Connection;
struct HandleRegistryProbe;

class HandleRegistry {
public:
    explicit HandleRegistry(BlockSource &memory);
    HandleRegistry(const HandleRegistry &) = delete;
    HandleRegistry &operator=(const HandleRegistry &) = delete;

    [[nodiscard]] Status wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent,
                              LoaderHandle *&handle);
    LoaderHandle *made(void *pointer, LoaderHandle::Kind kind) const;
    LoaderHandle *child(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index) const;
    LoaderHandle *instance_of(const Connection *connection) const;
    [[nodiscard]] Status destroy(LoaderHandle *handle);
    uint32_t             live() const;
    uint32_t             live_on(const Connection *connection) const;
    uint32_t             batches() const;
    uint32_t             memories() const;

    void report(StateWriter &writer, const Connection *connection) const;

    bool invariants_hold() const;

private:
    uint32_t      number_of(const LoaderHandle *handle) const;
    LoaderHandle *existing(LoaderHandle::Kind kind, uint64_t host, const Connection *connection) const;
    bool          parent_fits(LoaderHandle::Kind kind, const Connection *connection, LoaderHandle *parent) const;
    bool          unique(const LoaderHandle &handle) const;
    uint64_t      key_of(LoaderHandle::Kind kind, uint64_t host, const Connection *connection) const;
    uint64_t      address_of(const Connection *connection) const;
    void          unindex(const LoaderHandle &handle);

    HandleTree<LoaderHandle>             tree_;
    KeyedSlots<uint64_t, LoaderHandle *> index_;
    KeyedSlots<uint64_t, LoaderHandle *> instances_;
    uint32_t                             unindexed_;

    friend struct HandleRegistryProbe;
};

}

#endif

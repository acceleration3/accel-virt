#include "icd/vulkan/handle_registry.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/device_memory.hpp"

namespace av::icd {

namespace {

/* LoaderHandle::Kind's names, in its order. */
constexpr const char *kind_names[] = {"instance", "physical_device", "device", "queue", "command_buffer"};

}  // namespace

HandleRegistry::HandleRegistry(BlockSource &memory)
    : tree_(memory), index_(memory, Pool::paged), instances_(memory, Pool::paged), unindexed_(0u)
{
    AV_SCOPE("HandleRegistry::HandleRegistry");
}

Status HandleRegistry::wrap(LoaderHandle::Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent,
                            LoaderHandle *&handle)
{
    AV_SCOPE("HandleRegistry::wrap");
    handle = nullptr;
    if (host == 0u || connection == nullptr) return Status::invalid_parameter;
    if (!parent_fits(kind, connection, parent)) return Status::invalid_parameter;
    handle = existing(kind, host, connection);
    if (handle != nullptr) return Status::ok;
    /* Room for the instance first, so a connection's instance is always found. */
    if (kind == LoaderHandle::Kind::instance && instance_of(connection) == nullptr &&
        instances_.reserve(instances_.count() + 1u) != Status::ok)
        return Status::no_memory;
    const Status made = tree_.make(handle, kind, host, connection, parent);
    if (made != Status::ok) return made;
    /* A handle the index cannot take is still found, by the walk. */
    uint32_t slot = 0u;
    if (index_.insert(key_of(kind, host, connection), handle, slot) != Status::ok) unindexed_++;
    /* A second instance on one connection is not the one its handles are made
     * through: the first stays. */
    if (kind == LoaderHandle::Kind::instance)
        static_cast<void>(instances_.insert(address_of(connection), handle, slot));
    return Status::ok;
}

/* splitmix64's finaliser over the three. */
uint64_t HandleRegistry::key_of(LoaderHandle::Kind kind, uint64_t host, const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::key_of");
    uint64_t mixed = host ^ (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(connection)) * 0x9E3779B97F4A7C15u) ^
                     (static_cast<uint64_t>(kind) << 56u);
    mixed ^= mixed >> 30u;
    mixed *= 0xBF58476D1CE4E5B9u;
    mixed ^= mixed >> 27u;
    mixed *= 0x94D049BB133111EBu;
    return mixed ^ (mixed >> 31u);
}

uint64_t HandleRegistry::address_of(const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::address_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(connection));
}

void HandleRegistry::unindex(const LoaderHandle &handle)
{
    AV_SCOPE("HandleRegistry::unindex");
    const uint64_t             connection = address_of(handle.connection());
    LoaderHandle *const *const instance = instances_.find(connection);
    if (instance != nullptr && *instance == &handle) static_cast<void>(instances_.erase(connection));
    const uint64_t             key = key_of(handle.kind(), handle.host(), handle.connection());
    LoaderHandle *const *const indexed = index_.find(key);
    if (indexed != nullptr && *indexed == &handle)
        static_cast<void>(index_.erase(key));
    else
        unindexed_--;
}

bool HandleRegistry::parent_fits(LoaderHandle::Kind kind, const Connection *connection, LoaderHandle *parent) const
{
    AV_SCOPE("HandleRegistry::parent_fits");
    if (kind == LoaderHandle::Kind::instance) return parent == nullptr;
    return tree_.parent_fits(kind, parent) && parent->connection() == connection;
}

LoaderHandle *HandleRegistry::existing(LoaderHandle::Kind kind, uint64_t host, const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::existing");
    LoaderHandle *const *const indexed = index_.find(key_of(kind, host, connection));
    if (indexed != nullptr && (*indexed)->names(kind, host, connection)) return *indexed;
    if (unindexed_ == 0u) return nullptr;
    LoaderHandle *found = nullptr;
    static_cast<void>(tree_.nodes().all_of([&](const LoaderHandle &handle) {
        if (!handle.names(kind, host, connection)) return true;
        found = const_cast<LoaderHandle *>(&handle);
        return false;
    }));
    return found;
}

LoaderHandle *HandleRegistry::made(void *pointer, LoaderHandle::Kind kind) const
{
    AV_SCOPE("HandleRegistry::made");
    return tree_.made(pointer, kind);
}

LoaderHandle *HandleRegistry::child(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index) const
{
    AV_SCOPE("HandleRegistry::child");
    return tree_.child(parent, kind, index);
}

LoaderHandle *HandleRegistry::instance_of(const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::instance_of");
    LoaderHandle *const *const instance = instances_.find(address_of(connection));
    return instance != nullptr ? *instance : nullptr;
}

Status HandleRegistry::destroy(LoaderHandle *handle)
{
    AV_SCOPE("HandleRegistry::destroy");
    if (!tree_.holds(handle)) return Status::invalid_parameter;
    tree_.for_subtree(*handle, [this](const LoaderHandle &going) { unindex(going); });
    static_cast<void>(tree_.destroy(*handle));
    return Status::ok;
}

uint32_t HandleRegistry::live() const
{
    AV_SCOPE("HandleRegistry::live");
    return tree_.live();
}

uint32_t HandleRegistry::live_on(const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::live_on");
    uint32_t counted = 0u;
    static_cast<void>(tree_.nodes().all_of([&](const LoaderHandle &handle) {
        if (handle.connection() == connection) counted++;
        return true;
    }));
    return counted;
}

uint32_t HandleRegistry::batches() const
{
    AV_SCOPE("HandleRegistry::batches");
    uint32_t counted = 0u;
    static_cast<void>(tree_.nodes().all_of([&](const LoaderHandle &handle) {
        if (handle.batch() != nullptr) counted++;
        return true;
    }));
    return counted;
}

uint32_t HandleRegistry::memories() const
{
    AV_SCOPE("HandleRegistry::memories");
    uint32_t counted = 0u;
    static_cast<void>(tree_.nodes().all_of([&](const LoaderHandle &handle) {
        if (handle.device_memory() != nullptr) counted++;
        return true;
    }));
    return counted;
}

bool HandleRegistry::unique(const LoaderHandle &handle) const
{
    AV_SCOPE("HandleRegistry::unique");
    uint32_t sharing = 0u;
    static_cast<void>(tree_.nodes().all_of([&](const LoaderHandle &other) {
        if (other.names(handle.kind(), handle.host(), handle.connection())) sharing++;
        return true;
    }));
    return sharing == 1u;
}

void HandleRegistry::report(StateWriter &writer, const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::report");
    uint32_t number = 0u;
    static_cast<void>(tree_.nodes().all_of([&](const LoaderHandle &handle) {
        const uint32_t own = number++;
        if (handle.connection() != connection) return true;
        writer.open("handle", own);
        writer.text("kind", kind_names[static_cast<uint32_t>(handle.kind())]);
        writer.hex("host", handle.host());
        if (handle.parent() == nullptr) writer.text("parent", "none");
        else writer.number("parent", number_of(handle.parent()));
        if (handle.device_memory() != nullptr) {
            writer.open("memory", 0u);
            handle.device_memory()->report(writer);
            writer.close();
        }
        if (handle.batch() != nullptr) {
            writer.open("batch", 0u);
            handle.batch()->report(writer);
            writer.close();
        }
        writer.close();
        return true;
    }));
}

uint32_t HandleRegistry::number_of(const LoaderHandle *handle) const
{
    AV_SCOPE("HandleRegistry::number_of");
    uint32_t number = 0u;
    uint32_t found = tree_.live();
    static_cast<void>(tree_.nodes().all_of([&](const LoaderHandle &candidate) {
        if (&candidate == handle) found = number;
        number++;
        return true;
    }));
    return found;
}

bool HandleRegistry::invariants_hold() const
{
    AV_SCOPE("HandleRegistry::invariants_hold");
    /* A parent is proven live before the handle's own invariants read it. */
    if (!tree_.invariants_hold() || !index_.invariants_hold() || !instances_.invariants_hold()) return false;
    bool instances_live = true;
    instances_.for_each([&](uint64_t connection, LoaderHandle *const &instance) {
        if (!tree_.holds(instance) || instance->kind() != LoaderHandle::Kind::instance ||
            address_of(instance->connection()) != connection)
            instances_live = false;
    });
    if (!instances_live) return false;
    uint32_t   indexed = 0u;
    const bool whole = tree_.nodes().all_of([&](const LoaderHandle &handle) {
        LoaderHandle *const *const entry = index_.find(key_of(handle.kind(), handle.host(), handle.connection()));
        if (entry != nullptr && *entry == &handle) indexed++;
        return handle.invariants_hold() && unique(handle);
    });
    return whole && indexed == index_.count() && indexed + unindexed_ == tree_.live();
}

}  // namespace av::icd

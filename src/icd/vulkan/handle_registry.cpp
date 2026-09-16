#include "icd/vulkan/handle_registry.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/device_memory.hpp"

namespace av::icd {

namespace {

/* LoaderHandle::Kind's names, in its order. */
constexpr const char *kind_names[] = {"instance", "physical_device", "device", "queue", "command_buffer"};
constexpr LoaderHandle::Kind deepest_kind = LoaderHandle::Kind::command_buffer;

}  // namespace

HandleRegistry::HandleRegistry(BlockSource &memory) : pool_(memory)
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
    return pool_.create(handle, kind, host, connection, parent);
}

bool HandleRegistry::parent_fits(LoaderHandle::Kind kind, const Connection *connection, LoaderHandle *parent) const
{
    AV_SCOPE("HandleRegistry::parent_fits");
    if (kind == LoaderHandle::Kind::instance) return parent == nullptr;
    if (parent == nullptr || pool_.find(parent) == nullptr) return false;
    return parent->kind() < kind && parent->connection() == connection;
}

LoaderHandle *HandleRegistry::existing(LoaderHandle::Kind kind, uint64_t host, const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::existing");
    LoaderHandle *found = nullptr;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &handle) {
        if (!handle.names(kind, host, connection)) return true;
        found = const_cast<LoaderHandle *>(&handle);
        return false;
    }));
    return found;
}

LoaderHandle *HandleRegistry::made(void *pointer, LoaderHandle::Kind kind) const
{
    AV_SCOPE("HandleRegistry::made");
    LoaderHandle *const handle = pool_.find(pointer);
    return handle != nullptr && handle->kind() == kind ? handle : nullptr;
}

LoaderHandle *HandleRegistry::child(const LoaderHandle &parent, LoaderHandle::Kind kind, uint32_t index) const
{
    AV_SCOPE("HandleRegistry::child");
    LoaderHandle *found = nullptr;
    uint32_t      skipped = 0u;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &handle) {
        if (handle.kind() != kind || handle.parent() != &parent) return true;
        if (skipped++ != index) return true;
        found = const_cast<LoaderHandle *>(&handle);
        return false;
    }));
    return found;
}

LoaderHandle *HandleRegistry::instance_of(const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::instance_of");
    LoaderHandle *found = nullptr;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &handle) {
        if (handle.kind() != LoaderHandle::Kind::instance || handle.connection() != connection) return true;
        found = const_cast<LoaderHandle *>(&handle);
        return false;
    }));
    return found;
}

Status HandleRegistry::destroy(LoaderHandle *handle)
{
    AV_SCOPE("HandleRegistry::destroy");
    if (pool_.find(handle) == nullptr) return Status::invalid_parameter;
    /* Deepest kind first: a handle's parents are all of lower kinds, so every
     * chain a pass walks is made of handles no pass has destroyed yet. */
    for (auto kind = static_cast<uint32_t>(deepest_kind); kind > static_cast<uint32_t>(handle->kind()); kind--) {
        pool_.destroy_if([&](const LoaderHandle &candidate) {
            return static_cast<uint32_t>(candidate.kind()) == kind && candidate.descends_from(*handle);
        });
    }
    return pool_.destroy(handle);
}

uint32_t HandleRegistry::live() const
{
    AV_SCOPE("HandleRegistry::live");
    return pool_.live();
}

uint32_t HandleRegistry::live_on(const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::live_on");
    uint32_t counted = 0u;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &handle) {
        if (handle.connection() == connection) counted++;
        return true;
    }));
    return counted;
}

uint32_t HandleRegistry::batches() const
{
    AV_SCOPE("HandleRegistry::batches");
    uint32_t counted = 0u;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &handle) {
        if (handle.batch() != nullptr) counted++;
        return true;
    }));
    return counted;
}

uint32_t HandleRegistry::memories() const
{
    AV_SCOPE("HandleRegistry::memories");
    uint32_t counted = 0u;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &handle) {
        if (handle.device_memory() != nullptr) counted++;
        return true;
    }));
    return counted;
}

bool HandleRegistry::unique(const LoaderHandle &handle) const
{
    AV_SCOPE("HandleRegistry::unique");
    uint32_t sharing = 0u;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &other) {
        if (other.names(handle.kind(), handle.host(), handle.connection())) sharing++;
        return true;
    }));
    return sharing == 1u;
}

void HandleRegistry::report(StateWriter &writer, const Connection *connection) const
{
    AV_SCOPE("HandleRegistry::report");
    uint32_t number = 0u;
    static_cast<void>(pool_.all_of([&](const LoaderHandle &handle) {
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
    uint32_t found = pool_.live();
    static_cast<void>(pool_.all_of([&](const LoaderHandle &candidate) {
        if (&candidate == handle) found = number;
        number++;
        return true;
    }));
    return found;
}

bool HandleRegistry::invariants_hold() const
{
    AV_SCOPE("HandleRegistry::invariants_hold");
    if (!pool_.invariants_hold()) return false;
    /* A parent is proven live before the handle's own invariants read it. */
    return pool_.all_of([&](const LoaderHandle &handle) {
        if (handle.parent() != nullptr && pool_.find(handle.parent()) == nullptr) return false;
        return handle.invariants_hold() && unique(handle);
    });
}

}  // namespace av::icd

#include "icd/d3d/umd_connections.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::d3d {

UmdConnections::UmdConnections(Kmt &kmt) : connections_()
{
    AV_SCOPE("UmdConnections::UmdConnections");
    for (ObjectStorage<UmdConnection> &connection : connections_) static_cast<void>(connection.construct(kmt));
}

UmdConnections::~UmdConnections()
{
    AV_SCOPE("UmdConnections::~UmdConnections");
    for (ObjectStorage<UmdConnection> &connection : connections_) connection.destroy();
}

Status UmdConnections::acquire(uint64_t luid, UmdConnection *&held)
{
    AV_SCOPE("UmdConnections::acquire");
    held = nullptr;
    UmdConnection *chosen = nullptr;
    for (ObjectStorage<UmdConnection> &slot : connections_) {
        UmdConnection &connection = slot.object();
        if (connection.holds(luid)) {
            chosen = &connection;
            break;
        }
        if (chosen == nullptr && !connection.open()) chosen = &connection;
    }
    if (chosen == nullptr) return Status::no_space;
    const Status acquired = chosen->acquire(luid);
    if (acquired == Status::ok) held = chosen;
    return acquired;
}

uint32_t UmdConnections::opens() const
{
    AV_SCOPE("UmdConnections::opens");
    uint32_t counted = 0u;
    for (const ObjectStorage<UmdConnection> &connection : connections_) counted += connection.object().opens();
    return counted;
}

uint32_t UmdConnections::open() const
{
    AV_SCOPE("UmdConnections::open");
    uint32_t counted = 0u;
    for (const ObjectStorage<UmdConnection> &connection : connections_) counted += connection.object().open() ? 1u : 0u;
    return counted;
}

void UmdConnections::report(StateWriter &writer) const
{
    AV_SCOPE("UmdConnections::report");
    for (uint32_t index = 0u; index < max_adapters; index++) {
        const UmdConnection &connection = connections_[index].object();
        if (!connection.open()) continue;
        writer.open("connection", index);
        connection.report(writer);
        writer.close();
    }
}

bool UmdConnections::invariants_hold() const
{
    AV_SCOPE("UmdConnections::invariants_hold");
    for (uint32_t index = 0u; index < max_adapters; index++) {
        const UmdConnection &connection = connections_[index].object();
        if (!connection.invariants_hold()) return false;
        if (!connection.open()) continue;
        for (uint32_t other = index + 1u; other < max_adapters; other++)
            if (connections_[other].object().shares_adapter(connection)) return false;
    }
    return true;
}

}

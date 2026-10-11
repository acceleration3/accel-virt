#include "icd/common/opened_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/shared_object_kind.hpp"

namespace av::icd {

OpenedObjects::OpenedObjects(SharedHandles &handles, BlockSource &memory, LogSink &log, const char *prefix)
    : handles_(handles), lines_(log, prefix), rows_(memory)
{
    AV_SCOPE("OpenedObjects::OpenedObjects");
}

OpenedObjectRow *OpenedObjects::row_of(uint32_t kind, uint64_t made)
{
    AV_SCOPE("OpenedObjects::row_of");
    if (made == 0u) return nullptr;
    return rows_.find_if([kind, made](const OpenedObjectRow &row) { return row.made_ == made && row.kind_ == kind; });
}

const OpenedObjectRow *OpenedObjects::row_of(uint32_t kind, uint64_t made) const
{
    AV_SCOPE("OpenedObjects::row_of");
    if (made == 0u) return nullptr;
    return rows_.find_if([kind, made](const OpenedObjectRow &row) { return row.made_ == made && row.kind_ == kind; });
}

bool OpenedObjects::holds(uint64_t object_id) const
{
    AV_SCOPE("OpenedObjects::holds");
    return rows_.find_if([object_id](const OpenedObjectRow &row) { return row.object_id_ == object_id; }) !=
           nullptr;
}

bool OpenedObjects::opened_bytes(uint32_t kind, uint64_t size, uint64_t &bytes) const
{
    AV_SCOPE("OpenedObjects::opened_bytes");
    const SharedObjectKind named(kind);
    bytes = 0u;
    if (named.is_fence()) return true;
    if (!named.is_memory()) return false;
    bytes = SharedBytes(size).pages();
    return bytes != 0u;
}

Status OpenedObjects::open(ClientTransport &wire, uint32_t device, uint64_t handle, uint32_t kind, uint64_t size,
                           uint64_t &made)
{
    AV_SCOPE("OpenedObjects::open");
    made = 0u;
    uint64_t asked = 0u;
    if (handle == 0u || !opened_bytes(kind, size, asked)) return Status::invalid_parameter;
    uint64_t object_id = 0u;
    const Status adopted = handles_.adopt(device, handle, object_id);
    if (adopted == Status::no_memory) {
        lines_.write("no memory to keep a Win32 handle");
        return Status::no_memory;
    }
    if (adopted != Status::ok) {
        lines_.write("a Win32 handle this process may not open");
        return Status::not_found;
    }
    if (holds(object_id)) {
        lines_.write("an object this context already imported");
        return Status::conflict;
    }
    /* The row first: an object the device opened is one this context holds
     * and must be able to give back. */
    OpenedObjectRow *row = nullptr;
    if (rows_.create(row, OpenedObjectRow{object_id, 0u, asked, kind}) != Status::ok) {
        lines_.write("no memory to keep an opened shared object");
        return Status::no_memory;
    }
    SharedObjectCall call(wire);
    uint64_t         opened = 0u;
    /* An open that answered no handle is malformed to the call itself - an ok
     * carrying nothing is an answer no guest could use - so there is nothing
     * left here to check for. */
    if (call.open(object_id, kind, asked, opened) != Status::ok) {
        lines_.note("the device would not open a shared object for this context, status", call.device_status());
        static_cast<void>(rows_.destroy(row));
        return Status::invalid_state;
    }
    row->made_ = opened;
    made = opened;
    return Status::ok;
}

bool OpenedObjects::answered(uint32_t kind, uint64_t made) const
{
    AV_SCOPE("OpenedObjects::answered");
    return row_of(kind, made) != nullptr;
}

bool OpenedObjects::give_back(ClientTransport &wire, uint32_t kind, uint64_t made)
{
    AV_SCOPE("OpenedObjects::give_back");
    OpenedObjectRow *const row = row_of(kind, made);
    if (row == nullptr) return false;
    SharedObjectCall call(wire);
    static_cast<void>(call.release(row->object_id_, row->kind_, row->size_));
    static_cast<void>(rows_.destroy(row));
    return true;
}

void OpenedObjects::release_all(ClientTransport &wire)
{
    AV_SCOPE("OpenedObjects::release_all");
    SharedObjectCall call(wire);
    rows_.destroy_if([&call](const OpenedObjectRow &row) {
        static_cast<void>(call.release(row.object_id_, row.kind_, row.size_));
        return true;
    });
}

uint32_t OpenedObjects::live() const
{
    AV_SCOPE("OpenedObjects::live");
    return rows_.live();
}

void OpenedObjects::report(StateWriter &writer) const
{
    AV_SCOPE("OpenedObjects::report");
    rows_.for_each([&writer](const OpenedObjectRow &row) {
        writer.open("object", row.object_id_);
        writer.number("kind", row.kind_);
        writer.number("made", row.made_);
        writer.number("size", row.size_);
        writer.close();
    });
}

bool OpenedObjects::row_holds(const OpenedObjectRow &row) const
{
    AV_SCOPE("OpenedObjects::row_holds");
    const SharedObjectKind kind(row.kind_);
    if (row.object_id_ == 0u || row.made_ == 0u) return false;
    if (kind.is_fence()) return row.size_ == 0u;
    return kind.is_memory() && row.size_ != 0u && SharedBytes(row.size_).pages() == row.size_;
}

bool OpenedObjects::invariants_hold() const
{
    AV_SCOPE("OpenedObjects::invariants_hold");
    return rows_.all_of([this](const OpenedObjectRow &row) {
        if (!row_holds(row)) return false;
        uint32_t alike = 0u;
        rows_.for_each([&row, &alike](const OpenedObjectRow &other) {
            if (other.object_id_ == row.object_id_ || (other.made_ == row.made_ && other.kind_ == row.kind_))
                alike++;
        });
        return alike == 1u;
    });
}

}  // namespace av::icd

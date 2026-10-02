#include "icd/common/opened_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/shared_object_kind.hpp"

namespace av::icd {

OpenedObjects::OpenedObjects(SharedHandles &handles, LogSink &log, const char *prefix)
    : handles_(handles), lines_(log, prefix), rows_{}, live_(0u)
{
    AV_SCOPE("OpenedObjects::OpenedObjects");
}

OpenedObjectRow *OpenedObjects::row_of(uint32_t kind, uint64_t made)
{
    AV_SCOPE("OpenedObjects::row_of");
    if (made == 0u) return nullptr;
    for (uint32_t index = 0u; index < live_; index++)
        if (rows_[index].made_ == made && rows_[index].kind_ == kind) return &rows_[index];
    return nullptr;
}

const OpenedObjectRow *OpenedObjects::row_of(uint32_t kind, uint64_t made) const
{
    AV_SCOPE("OpenedObjects::row_of");
    if (made == 0u) return nullptr;
    for (uint32_t index = 0u; index < live_; index++)
        if (rows_[index].made_ == made && rows_[index].kind_ == kind) return &rows_[index];
    return nullptr;
}

bool OpenedObjects::holds(uint64_t object_id) const
{
    AV_SCOPE("OpenedObjects::holds");
    for (uint32_t index = 0u; index < live_; index++)
        if (rows_[index].object_id_ == object_id) return true;
    return false;
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
    if (live_ == max_objects) return Status::no_space;
    uint64_t object_id = 0u;
    if (handles_.adopt(device, handle, object_id) != Status::ok) {
        lines_.write("a Win32 handle this process may not open");
        return Status::not_found;
    }
    if (holds(object_id)) {
        lines_.write("an object this context already imported");
        return Status::conflict;
    }
    SharedObjectCall call(wire);
    uint64_t         opened = 0u;
    /* An open that answered no handle is malformed to the call itself - an ok
     * carrying nothing is an answer no guest could use - so there is nothing
     * left here to check for. */
    if (call.open(object_id, kind, asked, opened) != Status::ok) {
        lines_.note("the device would not open a shared object for this context, status", call.device_status());
        return Status::invalid_state;
    }
    rows_[live_] = OpenedObjectRow{object_id, opened, asked, kind};
    live_++;
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
    live_--;
    *row = rows_[live_];
    return true;
}

void OpenedObjects::release_all(ClientTransport &wire)
{
    AV_SCOPE("OpenedObjects::release_all");
    while (live_ != 0u) {
        const OpenedObjectRow &last = rows_[live_ - 1u];
        static_cast<void>(give_back(wire, last.kind_, last.made_));
    }
}

uint32_t OpenedObjects::live() const
{
    AV_SCOPE("OpenedObjects::live");
    return live_;
}

void OpenedObjects::report(StateWriter &writer) const
{
    AV_SCOPE("OpenedObjects::report");
    for (uint32_t index = 0u; index < live_; index++) {
        writer.open("object", rows_[index].object_id_);
        writer.number("kind", rows_[index].kind_);
        writer.number("made", rows_[index].made_);
        writer.number("size", rows_[index].size_);
        writer.close();
    }
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
    if (live_ > max_objects) return false;
    for (uint32_t index = 0u; index < live_; index++) {
        const OpenedObjectRow &row = rows_[index];
        if (!row_holds(row)) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (rows_[earlier].object_id_ == row.object_id_ ||
                (rows_[earlier].made_ == row.made_ && rows_[earlier].kind_ == row.kind_))
                return false;
    }
    return true;
}

}  // namespace av::icd

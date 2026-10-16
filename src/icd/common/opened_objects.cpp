#include "icd/common/opened_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/submission/shared_object_answer.hpp"

namespace av::icd {

OpenedObjects::OpenedObjects(SharedHandles &handles, BlockSource &memory, LogSink &log, const char *prefix)
    : handles_(handles), lines_(log, prefix),
      opens_{{memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged}}
{
    AV_SCOPE("OpenedObjects::OpenedObjects");
}

KeyedSlots<uint64_t, OpenedObjectRow> *OpenedObjects::opens_of(uint32_t kind)
{
    AV_SCOPE("OpenedObjects::opens_of");
    return kind - 1u < kinds ? &opens_[kind - 1u] : nullptr;
}

const KeyedSlots<uint64_t, OpenedObjectRow> *OpenedObjects::opens_of(uint32_t kind) const
{
    AV_SCOPE("OpenedObjects::opens_of");
    return kind - 1u < kinds ? &opens_[kind - 1u] : nullptr;
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
                           uint64_t into, uint64_t &made)
{
    AV_SCOPE("OpenedObjects::open");
    made = 0u;
    uint64_t asked = 0u;
    if (handle == 0u || !opened_bytes(kind, size, asked)) return Status::invalid_parameter;
    uint64_t     object_id = 0u;
    const Status adopted = handles_.adopt(device, handle, object_id);
    if (adopted == Status::no_memory) {
        lines_.write("no memory to keep a Win32 handle");
        return Status::no_memory;
    }
    if (adopted != Status::ok) {
        lines_.write("a Win32 handle this process may not open");
        return Status::not_found;
    }
    /* The row's room first: an object the device opened is one this context
     * holds and must be able to give back. */
    KeyedSlots<uint64_t, OpenedObjectRow> &opens = *opens_of(kind);
    if (opens.reserve(opens.count() + 1u) != Status::ok) {
        lines_.write("no memory to keep an opened shared object");
        return Status::no_memory;
    }
    SharedObjectCall call(wire);
    uint64_t         opened = 0u;
    /* An open that answered no handle is malformed to the call itself - an ok
     * carrying nothing is an answer no guest could use - so there is nothing
     * left here to check for. */
    if (call.open(object_id, kind, asked, into, opened) != Status::ok) {
        if (call.device_status() == SharedObjectAnswer::conflict) {
            lines_.write("an object this context already opened into the same object");
            return Status::conflict;
        }
        lines_.note("the device would not open a shared object for this context, status", call.device_status());
        return Status::invalid_state;
    }
    /* Two opens answered one name would leave the second unreachable, and
     * release the first twice. */
    const OpenedObjectRow row{object_id, into, asked};
    uint32_t              slot = 0u;
    if (opens.insert(opened, row, slot) != Status::ok) {
        lines_.note("the device answered a name another object of this context holds", opened);
        release(wire, kind, opened, row);
        return Status::invalid_state;
    }
    made = opened;
    return Status::ok;
}

void OpenedObjects::release(ClientTransport &wire, uint32_t kind, uint64_t made, const OpenedObjectRow &row) const
{
    AV_SCOPE("OpenedObjects::release");
    SharedObjectCall call(wire);
    static_cast<void>(call.release(row.object_id_, kind, row.size_, made));
}

bool OpenedObjects::answered(uint32_t kind, uint64_t made) const
{
    AV_SCOPE("OpenedObjects::answered");
    const KeyedSlots<uint64_t, OpenedObjectRow> *const opens = opens_of(kind);
    return opens != nullptr && made != 0u && opens->find(made) != nullptr;
}

bool OpenedObjects::give_back(ClientTransport &wire, uint32_t kind, uint64_t made)
{
    AV_SCOPE("OpenedObjects::give_back");
    KeyedSlots<uint64_t, OpenedObjectRow> *const opens = opens_of(kind);
    OpenedObjectRow                              row{};
    if (opens == nullptr || made == 0u || !opens->take(made, row)) return false;
    release(wire, kind, made, row);
    return true;
}

void OpenedObjects::give_back_into(ClientTransport &wire, uint64_t into)
{
    AV_SCOPE("OpenedObjects::give_back_into");
    if (into == 0u) return;
    for (uint32_t index = 0u; index < kinds; index++) {
        const uint32_t kind = index + 1u;
        static_cast<void>(
            opens_[index].erase_if([this, &wire, kind, into](uint64_t made, const OpenedObjectRow &row) {
                if (row.into_ != into) return false;
                release(wire, kind, made, row);
                return true;
            }));
    }
}

void OpenedObjects::release_all(ClientTransport &wire)
{
    AV_SCOPE("OpenedObjects::release_all");
    for (uint32_t index = 0u; index < kinds; index++) {
        const uint32_t kind = index + 1u;
        opens_[index].for_each(
            [this, &wire, kind](uint64_t made, const OpenedObjectRow &row) { release(wire, kind, made, row); });
        opens_[index].clear();
    }
}

uint32_t OpenedObjects::live() const
{
    AV_SCOPE("OpenedObjects::live");
    uint32_t count = 0u;
    for (const KeyedSlots<uint64_t, OpenedObjectRow> &opens : opens_) count += opens.count();
    return count;
}

void OpenedObjects::report(StateWriter &writer) const
{
    AV_SCOPE("OpenedObjects::report");
    for (uint32_t index = 0u; index < kinds; index++) {
        const uint32_t kind = index + 1u;
        opens_[index].for_each([&writer, kind](uint64_t made, const OpenedObjectRow &row) {
            writer.open("opened", made);
            writer.number("object", row.object_id_);
            writer.number("kind", kind);
            writer.hex("into", row.into_);
            writer.number("size", row.size_);
            writer.close();
        });
    }
}

bool OpenedObjects::row_holds(uint32_t kind, uint64_t made, const OpenedObjectRow &row) const
{
    AV_SCOPE("OpenedObjects::row_holds");
    const SharedObjectKind named(kind);
    if (made == 0u || row.object_id_ == 0u) return false;
    if (named.is_fence()) return row.size_ == 0u;
    return named.is_memory() && row.size_ != 0u && SharedBytes(row.size_).pages() == row.size_;
}

bool OpenedObjects::invariants_hold() const
{
    AV_SCOPE("OpenedObjects::invariants_hold");
    bool holds = true;
    for (uint32_t index = 0u; index < kinds; index++) {
        if (!opens_[index].invariants_hold()) return false;
        const uint32_t kind = index + 1u;
        opens_[index].for_each([this, kind, &holds](uint64_t made, const OpenedObjectRow &row) {
            if (!row_holds(kind, made, row)) holds = false;
        });
    }
    return holds;
}

}  // namespace av::icd

#include "icd/common/opened_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/shared_object_kind.hpp"

namespace av::icd {

OpenedObjects::OpenedObjects(SharedHandles &handles, BlockSource &memory, LogSink &log, const char *prefix)
    : handles_(handles), lines_(log, prefix), rows_(memory, Pool::paged),
      answers_{{memory, Pool::paged}, {memory, Pool::paged}, {memory, Pool::paged}}
{
    AV_SCOPE("OpenedObjects::OpenedObjects");
}

KeyedSlots<uint64_t, uint64_t> *OpenedObjects::answers_of(uint32_t kind)
{
    AV_SCOPE("OpenedObjects::answers_of");
    return kind - 1u < kinds ? &answers_[kind - 1u] : nullptr;
}

const KeyedSlots<uint64_t, uint64_t> *OpenedObjects::answers_of(uint32_t kind) const
{
    AV_SCOPE("OpenedObjects::answers_of");
    return kind - 1u < kinds ? &answers_[kind - 1u] : nullptr;
}

uint64_t OpenedObjects::object_of(uint32_t kind, uint64_t made) const
{
    AV_SCOPE("OpenedObjects::object_of");
    const KeyedSlots<uint64_t, uint64_t> *const answers = answers_of(kind);
    if (answers == nullptr || made == 0u) return 0u;
    const uint64_t *const object_id = answers->find(made);
    return object_id != nullptr ? *object_id : 0u;
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
    if (rows_.find(object_id) != nullptr) {
        lines_.write("an object this context already imported");
        return Status::conflict;
    }
    /* The row and its answer's room first: an object the device opened is one
     * this context holds and must be able to give back. */
    KeyedSlots<uint64_t, uint64_t> &answers = *answers_of(kind);
    uint32_t                        row = 0u;
    if (answers.reserve(answers.count() + 1u) != Status::ok ||
        rows_.insert(object_id, OpenedObjectRow{0u, asked, kind}, row) != Status::ok) {
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
        static_cast<void>(rows_.erase(object_id));
        return Status::invalid_state;
    }
    /* Two objects answered one name would leave the second unreachable, and
     * release the first twice. */
    uint32_t answer = 0u;
    if (answers.insert(opened, object_id, answer) != Status::ok) {
        lines_.note("the device answered a name another object of this context holds", opened);
        static_cast<void>(call.release(object_id, kind, asked));
        static_cast<void>(rows_.erase(object_id));
        return Status::invalid_state;
    }
    rows_.at(row)->made_ = opened;
    made = opened;
    return Status::ok;
}

bool OpenedObjects::answered(uint32_t kind, uint64_t made) const
{
    AV_SCOPE("OpenedObjects::answered");
    return object_of(kind, made) != 0u;
}

bool OpenedObjects::give_back(ClientTransport &wire, uint32_t kind, uint64_t made)
{
    AV_SCOPE("OpenedObjects::give_back");
    const uint64_t object_id = object_of(kind, made);
    if (object_id == 0u) return false;
    OpenedObjectRow row{};
    static_cast<void>(rows_.take(object_id, row));
    static_cast<void>(answers_of(kind)->erase(made));
    SharedObjectCall call(wire);
    static_cast<void>(call.release(object_id, row.kind_, row.size_));
    return true;
}

void OpenedObjects::release_all(ClientTransport &wire)
{
    AV_SCOPE("OpenedObjects::release_all");
    SharedObjectCall call(wire);
    rows_.for_each([&call](uint64_t object_id, const OpenedObjectRow &row) {
        static_cast<void>(call.release(object_id, row.kind_, row.size_));
    });
    rows_.clear();
    for (KeyedSlots<uint64_t, uint64_t> &answers : answers_) answers.clear();
}

uint32_t OpenedObjects::live() const
{
    AV_SCOPE("OpenedObjects::live");
    return rows_.count();
}

void OpenedObjects::report(StateWriter &writer) const
{
    AV_SCOPE("OpenedObjects::report");
    rows_.for_each([&writer](uint64_t object_id, const OpenedObjectRow &row) {
        writer.open("object", object_id);
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
    if (row.made_ == 0u) return false;
    if (kind.is_fence()) return row.size_ == 0u;
    return kind.is_memory() && row.size_ != 0u && SharedBytes(row.size_).pages() == row.size_;
}

bool OpenedObjects::invariants_hold() const
{
    AV_SCOPE("OpenedObjects::invariants_hold");
    if (!rows_.invariants_hold()) return false;
    uint32_t answered = 0u;
    for (const KeyedSlots<uint64_t, uint64_t> &answers : answers_) {
        if (!answers.invariants_hold()) return false;
        answered += answers.count();
    }
    bool holds = answered == rows_.count();
    rows_.for_each([this, &holds](uint64_t object_id, const OpenedObjectRow &row) {
        if (object_id == 0u || !row_holds(row) || object_of(row.kind_, row.made_) != object_id) holds = false;
    });
    return holds;
}

}  // namespace av::icd

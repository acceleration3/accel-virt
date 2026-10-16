#include "icd/opengl/gl_external_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/driver_log.hpp"
#include "proto/resources/shared_object_kind.hpp"

namespace av::glicd {

namespace {

/* What a GL semaphore is on the wire. Either fence kind is one host
 * semaphore, so the choice only has to be the same at every call. */
constexpr uint32_t semaphore_kind = SharedObjectKind::monitored_fence;

}  // namespace

GlExternalObjects::GlExternalObjects(icd::SharedHandles &handles, BlockSource &memory, DriverLog &log)
    : log_(log), opened_(handles, memory, log), rows_(memory, Pool::paged), names_()
{
    AV_SCOPE("GlExternalObjects::GlExternalObjects");
}

bool GlExternalObjects::known(uint32_t kind, bool &memory) const
{
    AV_SCOPE("GlExternalObjects::known");
    if (kind == SharedObjectKind::memory) {
        memory = true;
        return true;
    }
    if (kind != SharedObjectKind::monitored_fence && kind != SharedObjectKind::native_fence) return false;
    memory = false;
    return true;
}

GlSharedObjectRow *GlExternalObjects::row_of(bool memory, uint32_t name)
{
    AV_SCOPE("GlExternalObjects::row_of");
    GlSharedObjectRow *const row = rows_.find(name);
    return row != nullptr && row->memory_ == memory ? row : nullptr;
}

const GlSharedObjectRow *GlExternalObjects::row_of(bool memory, uint32_t name) const
{
    AV_SCOPE("GlExternalObjects::row_of");
    const GlSharedObjectRow *const row = rows_.find(name);
    return row != nullptr && row->memory_ == memory ? row : nullptr;
}

uint32_t GlExternalObjects::create(uint32_t kind, int32_t count, uint32_t *names)
{
    AV_SCOPE("GlExternalObjects::create");
    bool memory = false;
    if (!known(kind, memory)) return gl_invalid_value;
    if (count < 0 || (count > 0 && names == nullptr)) return gl_invalid_value;
    const uint32_t wanted = static_cast<uint32_t>(count);
    /* All of them or none: a half-filled array is one an application reads
     * whole, and the names it did not get would be numbers of its own stack. */
    if (wanted == 0u) return gl_no_error;
    /* Below 2^31 each, so the sum cannot wrap. */
    if (rows_.reserve(rows_.count() + wanted) != Status::ok) {
        log_.write("no memory for a memory object or semaphore name");
        return gl_out_of_memory;
    }
    const uint32_t first = names_.run(wanted);
    for (uint32_t index = 0u; index < wanted; index++) {
        uint32_t slot = 0u;
        /* Room was made, so only a name the mint came round to while the
         * context still holds it is refused. */
        if (rows_.insert(first + index, GlSharedObjectRow{0u, memory, false, false}, slot) != Status::ok) {
            for (uint32_t made = 0u; made < index; made++) static_cast<void>(rows_.erase(first + made));
            log_.write("every memory object and semaphore name is taken");
            return gl_out_of_memory;
        }
    }
    for (uint32_t index = 0u; index < wanted; index++) names[index] = first + index;
    return gl_no_error;
}

bool GlExternalObjects::holds(uint32_t kind, uint32_t name) const
{
    AV_SCOPE("GlExternalObjects::holds");
    bool memory = false;
    if (!known(kind, memory)) return false;
    return row_of(memory, name) != nullptr;
}

uint32_t GlExternalObjects::set_parameter(uint32_t kind, uint32_t name, uint32_t pname, int32_t value)
{
    AV_SCOPE("GlExternalObjects::set_parameter");
    bool memory = false;
    if (!known(kind, memory)) return gl_invalid_value;
    GlSharedObjectRow *const row = row_of(memory, name);
    if (row == nullptr) return gl_invalid_value;
    /* A semaphore's only parameter names a D3D12 fence's value, and nothing
     * here is ever imported from one, so no pname reaches a row of that kind. */
    if (!memory || (pname != dedicated && pname != protected_memory)) return gl_invalid_enum;
    if (pname == dedicated) row->dedicated_ = value != 0;
    else row->protected_ = value != 0;
    return gl_no_error;
}

uint32_t GlExternalObjects::get_parameter(uint32_t kind, uint32_t name, uint32_t pname,
                                          int32_t *value) const
{
    AV_SCOPE("GlExternalObjects::get_parameter");
    bool memory = false;
    if (!known(kind, memory) || value == nullptr) return gl_invalid_value;
    const GlSharedObjectRow *const row = row_of(memory, name);
    if (row == nullptr) return gl_invalid_value;
    if (!memory || (pname != dedicated && pname != protected_memory)) return gl_invalid_enum;
    *value = (pname == dedicated ? row->dedicated_ : row->protected_) ? 1 : 0;
    return gl_no_error;
}

uint32_t GlExternalObjects::wire_kind(const GlSharedObjectRow &row) const
{
    AV_SCOPE("GlExternalObjects::wire_kind");
    return row.memory_ ? SharedObjectKind::memory : semaphore_kind;
}

uint32_t GlExternalObjects::import(ClientTransport &wire, uint32_t device, uint32_t kind, uint32_t name,
                                   uint64_t size, uint32_t handle_type, void *handle)
{
    AV_SCOPE("GlExternalObjects::import");
    bool memory = false;
    if (!known(kind, memory)) return gl_invalid_value;
    if (handle_type != opaque_win32) {
        log_.write("an external object handle type this driver does not take");
        return gl_invalid_operation;
    }
    GlSharedObjectRow *const row = row_of(memory, name);
    /* A name this context never made, or one already naming an object: GL has
     * no way to say "imported twice" and the second would leak the first. */
    if (row == nullptr || row->made_ != 0u) return gl_invalid_value;
    /* What the application asked for before it got here. Dedicated memory is
     * an allocation made for this one image and protected memory is a
     * different heap; the device makes the object inside OPEN_SHARED_OBJECT
     * and is told neither, so the honest answer is the error the extension
     * has rather than an import that quietly ignores both (research.md §12). */
    if (row->dedicated_ || row->protected_) {
        log_.write("a memory object asked for dedicated or protected memory, which a shared "
                   "object cannot be");
        return gl_invalid_operation;
    }
    const uint64_t named = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
    uint64_t       opened = 0u;
    const Status   taken = opened_.open(wire, device, named, wire_kind(*row), size, 0u, opened);
    if (taken == Status::invalid_parameter) return gl_invalid_value;
    if (taken == Status::not_found || taken == Status::conflict) return gl_invalid_operation;
    if (taken != Status::ok) return gl_out_of_memory;
    /* A GL name is 32 bits; a device that answered anything else answered no
     * name this context could ever use. */
    const uint32_t made = static_cast<uint32_t>(opened);
    if (opened != made) {
        static_cast<void>(opened_.give_back(wire, wire_kind(*row), opened));
        return gl_out_of_memory;
    }
    row->made_ = made;
    return gl_no_error;
}

void GlExternalObjects::give_back(ClientTransport &wire, uint32_t name, const GlSharedObjectRow &row)
{
    AV_SCOPE("GlExternalObjects::give_back");
    static_cast<void>(opened_.give_back(wire, wire_kind(row), row.made_));
    static_cast<void>(rows_.erase(name));
}

uint32_t GlExternalObjects::destroy(ClientTransport &wire, uint32_t kind, int32_t count, const uint32_t *names)
{
    AV_SCOPE("GlExternalObjects::destroy");
    bool memory = false;
    if (!known(kind, memory)) return gl_invalid_value;
    if (count < 0 || (count > 0 && names == nullptr)) return gl_invalid_value;
    for (uint32_t index = 0u; index < static_cast<uint32_t>(count); index++) {
        GlSharedObjectRow *const row = row_of(memory, names[index]);
        /* A name this context never made is ignored, as GL ignores one it
         * never generated. */
        if (row != nullptr) give_back(wire, names[index], *row);
    }
    return gl_no_error;
}

uint32_t GlExternalObjects::made(uint32_t kind, uint32_t name) const
{
    AV_SCOPE("GlExternalObjects::made");
    bool memory = false;
    if (!known(kind, memory)) return 0u;
    const GlSharedObjectRow *const row = row_of(memory, name);
    return row != nullptr ? row->made_ : 0u;
}

void GlExternalObjects::release_all(ClientTransport &wire)
{
    AV_SCOPE("GlExternalObjects::release_all");
    opened_.release_all(wire);
    rows_.clear();
}

uint32_t GlExternalObjects::live() const
{
    AV_SCOPE("GlExternalObjects::live");
    return rows_.count();
}

void GlExternalObjects::report(StateWriter &writer) const
{
    AV_SCOPE("GlExternalObjects::report");
    writer.open("external_objects", 0u);
    writer.number("live", rows_.count());
    writer.close();
}

uint32_t GlExternalObjects::imported() const
{
    AV_SCOPE("GlExternalObjects::imported");
    uint32_t found = 0u;
    rows_.for_each([&found](uint32_t, const GlSharedObjectRow &row) { found += row.made_ != 0u ? 1u : 0u; });
    return found;
}

bool GlExternalObjects::invariants_hold() const
{
    AV_SCOPE("GlExternalObjects::invariants_hold");
    if (!rows_.invariants_hold() || !opened_.invariants_hold() || opened_.live() != imported()) return false;
    bool holds = true;
    rows_.for_each([this, &holds](uint32_t name, const GlSharedObjectRow &row) {
        if (name == 0u || (row.made_ != 0u && !opened_.answered(wire_kind(row), row.made_))) holds = false;
    });
    return holds;
}

}  // namespace av::glicd

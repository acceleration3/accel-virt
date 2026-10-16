/* The shared objects one guest context opened from a Win32 handle another API
 * published (feature 053), and what the device made of each - for every API
 * that imports by opening on the device: OpenGL's memory objects and
 * semaphores, OpenCL's external memory.
 *
 * An import is the same four steps whichever API asks: the handle turned into
 * the object behind it - one this process holds, or one the kernel lets it
 * open, which is the only thing that tells a handle another process sent from
 * a number an application invented (SharedHandles::adopt); a memory object's
 * size rounded up to the whole pages it was published as (SharedBytes); an
 * object this context already holds refused, because a context with two
 * handles over one object could release it twice; and the open sent. What the
 * device answered - `made` - is what the guest names the object by from then
 * on, and what releases it.
 *
 * A row is kept under its object, and found again under its kind and `made`:
 * OpenGL's memory objects and semaphores are separate namespaces, so a memory
 * object and a semaphore may be answered the same name, and each kind has an
 * index of its own. */
#ifndef AV_ICD_OPENED_OBJECTS_HPP
#define AV_ICD_OPENED_OBJECTS_HPP

#include "core/log/driver_log.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class LogSink;
class StateWriter;
}  // namespace av

namespace av::icd {

class SharedHandles;
struct OpenedObjectsProbe;

/* One object this context opened, kept under the object's id. A type of its
 * own so the table's rows are complete (tools/header_scan.py reads a class's
 * members only to its first nested type). */
struct OpenedObjectRow {
    /* What the device answered, which the guest names the object by. */
    uint64_t made_;
    /* The bytes it was opened as, which a release names again: whole pages
     * for memory, 0 for a fence, as the wire body demands of each. */
    uint64_t size_;
    uint32_t kind_;
};

class OpenedObjects {
public:
    /* `prefix` starts every refusal this writes ("avcl: "). Rows are kept in
     * `memory`: as many as the context opens. */
    OpenedObjects(SharedHandles &handles, BlockSource &memory, LogSink &log, const char *prefix);
    OpenedObjects(const OpenedObjects &) = delete;
    OpenedObjects &operator=(const OpenedObjects &) = delete;

    /* The object `handle` names opened on the device for this context as
     * `kind`, with what the device answered in `made`. `size` is a memory
     * object's, rounded up to whole pages before it is named, and ignored for
     * a fence.
     *
     * invalid_parameter for a handle of 0 or a memory size no whole number of
     * pages holds; not_found for a handle this process may not open; conflict
     * for an object this context already opened; no_memory when no row for
     * the handle or the object can be had, before the device is asked; invalid_state when the device would
     * not open it. `made` is 0
     * unless ok. */
    [[nodiscard]] Status open(ClientTransport &wire, uint32_t device, uint64_t handle, uint32_t kind, uint64_t size,
                              uint64_t &made);
    /* Whether the device answered `made` for an open of `kind` here. */
    bool                 answered(uint32_t kind, uint64_t made) const;
    /* The object behind `made` released on the device and its row dropped.
     * False for one this table never answered. */
    bool                 give_back(ClientTransport &wire, uint32_t kind, uint64_t made);
    /* Every object still held, released: what a context torn down gives back,
     * since the device counts this context as a holder of each. */
    void                 release_all(ClientTransport &wire);
    uint32_t             live() const;
    /* One "object" section a row, under whatever section the caller opened. */
    void                 report(StateWriter &writer) const;

    /* A live row names an object and what the device made of it, a memory
     * object has whole pages and a fence none, and no object - nor one kind's
     * answer - appears twice. */
    bool invariants_hold() const;

private:
    friend struct OpenedObjectsProbe;

    /* The index of `kind`'s answers, or null for a kind no object is opened
     * as. */
    KeyedSlots<uint64_t, uint64_t>       *answers_of(uint32_t kind);
    const KeyedSlots<uint64_t, uint64_t> *answers_of(uint32_t kind) const;
    /* The object the device answered `made` for as `kind`, or 0. */
    uint64_t                              object_of(uint32_t kind, uint64_t made) const;
    /* The bytes `kind` is opened as: whole pages of `size` for memory, 0 for a
     * fence. False for memory no whole number of pages holds. */
    bool                   opened_bytes(uint32_t kind, uint64_t size, uint64_t &bytes) const;
    bool                   row_holds(const OpenedObjectRow &row) const;

    /* SharedObjectKind's memory, monitored_fence and native_fence. */
    static constexpr uint32_t kinds = 3u;

    SharedHandles                           &handles_;
    DriverLog                                lines_;
    /* Every object opened, by its id. */
    KeyedSlots<uint64_t, OpenedObjectRow>    rows_;
    /* Each kind's objects' ids by what the device answered for them. */
    KeyedSlots<uint64_t, uint64_t>           answers_[kinds];
};

}  // namespace av::icd

#endif  // AV_ICD_OPENED_OBJECTS_HPP

/* The shared objects one guest context opened from a Win32 handle another API
 * or another process published (feature 053), and what the device made of
 * each - for every API that imports by opening on the device: OpenGL's memory
 * objects and semaphores, OpenCL's external memory, Vulkan's memory.
 *
 * An import is the same steps whichever API asks: the handle turned into the
 * object behind it - one this process holds, or one the kernel lets it open,
 * which is the only thing that tells a handle another process sent from a
 * number an application invented (SharedHandles::adopt); a memory object's
 * size rounded up to the whole pages it was published as (SharedBytes); and
 * the open sent. What the device answered - `made` - is what the guest names
 * the object by from then on, and what releases it.
 *
 * A row is kept under what the device answered, with what the open was made
 * into - a Vulkan device, whose destruction takes its opens with it - and
 * whether this context may hold one object twice is the device's to say: it
 * keeps the one record of what each context holds. OpenGL's memory objects
 * and semaphores are separate namespaces, so a memory object and a semaphore
 * may be answered the same name, and each kind has a table of its own. */
#ifndef AV_ICD_OPENED_OBJECTS_HPP
#define AV_ICD_OPENED_OBJECTS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class DriverLog;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {

class SharedHandles;
struct OpenedObjectsProbe;

/* One open this context made, kept under what the device answered. A type of
 * its own so the table's rows are complete (tools/header_scan.py reads a
 * class's members only to its first nested type). */
struct OpenedObjectRow {
    uint64_t object_id_;
    /* What the open was made into: a Vulkan device's wire handle, 0 for an
     * API with nothing to choose. */
    uint64_t into_;
    /* The bytes it was opened as, which a release names again: whole pages
     * for memory, 0 for a fence, as the wire body demands of each. */
    uint64_t size_;
};

class OpenedObjects {
public:
    /* Refusals go to the driver's `log`. Rows are kept in `memory`: as many
     * as the context opens. The table takes no lock; its owner serializes
     * it. */
    OpenedObjects(SharedHandles &handles, BlockSource &memory, DriverLog &log);
    OpenedObjects(const OpenedObjects &) = delete;
    OpenedObjects &operator=(const OpenedObjects &) = delete;

    /* The object `handle` names opened on the device for this context as
     * `kind`, made into `into`, with what the device answered in `made`.
     * `size` is a memory object's, rounded up to whole pages before it is
     * named, and ignored for a fence.
     *
     * invalid_parameter for a handle of 0 or a memory size no whole number of
     * pages holds; not_found for a handle this process may not open; conflict
     * for an object the device says this context already holds as asked;
     * no_memory when no row can be had, before the device is asked;
     * invalid_state when the device would not open it. `made` is 0 unless
     * ok. */
    [[nodiscard]] Status open(ClientTransport &wire, uint32_t device, uint64_t handle, uint32_t kind, uint64_t size,
                              uint64_t into, uint64_t &made);
    /* Whether the device answered `made` for an open of `kind` here. */
    bool                 answered(uint32_t kind, uint64_t made) const;
    /* The object behind `made` released on the device and its row dropped.
     * False for one this table never answered. */
    bool                 give_back(ClientTransport &wire, uint32_t kind, uint64_t made);
    /* Every open made into `into` given back: what a Vulkan device's
     * destruction takes with it. Nothing for 0. */
    void                 give_back_into(ClientTransport &wire, uint64_t into);
    /* Every object still held, released: what a context torn down gives back,
     * since the device counts this context as a holder of each. */
    void                 release_all(ClientTransport &wire);
    uint32_t             live() const;
    /* One "opened" section a row, under whatever section the caller opened. */
    void                 report(StateWriter &writer) const;

    /* A live row names an object and was answered a name, and a memory object
     * has whole pages and a fence none. */
    bool invariants_hold() const;

private:
    friend struct OpenedObjectsProbe;

    /* The table of `kind`'s opens, or null for a kind no object is opened
     * as. */
    KeyedSlots<uint64_t, OpenedObjectRow>       *opens_of(uint32_t kind);
    const KeyedSlots<uint64_t, OpenedObjectRow> *opens_of(uint32_t kind) const;
    /* The bytes `kind` is opened as: whole pages of `size` for memory, 0 for a
     * fence. False for memory no whole number of pages holds. */
    bool opened_bytes(uint32_t kind, uint64_t size, uint64_t &bytes) const;
    bool row_holds(uint32_t kind, uint64_t made, const OpenedObjectRow &row) const;
    /* `row`, answered `made` as `kind`, released on the device. */
    void release(ClientTransport &wire, uint32_t kind, uint64_t made, const OpenedObjectRow &row) const;

    /* SharedObjectKind's memory, monitored_fence and native_fence. */
    static constexpr uint32_t kinds = 3u;

    SharedHandles                         &handles_;
    DriverLog                             &lines_;
    /* Each kind's opens by what the device answered for them. */
    KeyedSlots<uint64_t, OpenedObjectRow>  opens_[kinds];
};

}  // namespace av::icd

#endif  // AV_ICD_OPENED_OBJECTS_HPP

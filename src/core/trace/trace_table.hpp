/* The trace table of one binary (docs/trace.md): which threads are inside
 * which functions, since when, and the lock each waits on or holds, in memory
 * a reader outside the binary maps. Lock-free and allocation-free, so every
 * method is legal at any IRQL, in an interrupt, and in a crash handler; a slot
 * is written only by the thread that owns it. One per binary, bound through
 * ScopeBinding. */
#ifndef AV_TRACE_TABLE_HPP
#define AV_TRACE_TABLE_HPP

#include "core/support/status.hpp"
#include "core/trace/trace_layout.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ScopeName;
class TraceHost;
struct TraceTableProbe;

class TraceTable {
public:
    static constexpr uint32_t no_slot = TraceLayout::max_slots;

    explicit TraceTable(TraceHost &host);
    TraceTable(const TraceTable &) = delete;
    TraceTable &operator=(const TraceTable &) = delete;

    /* Lays the table out over `region`, which the table does not own and
     * which must outlive every thread's last scope: as many slots as fit, at
     * most TraceLayout::max_slots. invalid_state when laid out already;
     * invalid_parameter for no region, one not 8-byte aligned, one without
     * room for a slot, or a module name longer than 15 bytes. */
    [[nodiscard]] Status initialize(uint8_t *region, size_t bytes, const char *module, uint32_t process);

    /* The calling thread enters `name`: the slot it was pushed on, or no_slot
     * when the table is not laid out or every slot is some thread's active one. */
    uint32_t enter(const ScopeName &name);
    /* The calling thread leaves the function it entered on `slot` last. A slot
     * the thread does not hold is left alone. */
    void     leave(uint32_t slot);

    /* The calling thread blocks on the lock at `lock`, got it, gave it back. */
    void waiting(uint64_t lock);
    void acquired(uint64_t lock);
    void released(uint64_t lock);

    /* The table stops using its region, which its owner may then release:
     * every later call writes nothing - including the leaves of scopes still
     * open when it was withdrawn. The region's bytes are left as they are. */
    void withdraw();

    /* Once, from the thread that crashed; a later call keeps the first. */
    void record_crash(uint32_t code, uint64_t address);

    const uint8_t *region() const;
    size_t         bytes() const;

    bool invariants_hold() const;

private:
    uint64_t read64(size_t offset) const;
    uint32_t read32(size_t offset) const;
    void     write64(size_t offset, uint64_t value);
    void     write32(size_t offset, uint32_t value);
    void     write16(size_t offset, uint16_t value);
    uint64_t &owner(uint32_t slot);
    uint64_t  owner_word(uint32_t slot) const;
    size_t    slot_at(uint32_t slot) const;
    uint32_t first_probe(uint64_t key) const;
    uint32_t active_slot(uint64_t key) const;
    uint32_t claim(uint64_t key);
    bool     take(uint32_t slot, uint64_t expected, uint64_t key);
    void     push(uint32_t slot, const ScopeName &name);

    TraceHost  &host_;
    uint8_t    *region_;
    TraceLayout layout_;
    /* layout_'s slot count, kept beside it for the scope path. */
    uint32_t    slots_;

    friend struct TraceTableProbe;
};

}  // namespace av

#endif  // AV_TRACE_TABLE_HPP

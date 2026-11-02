#ifndef AV_TRACE_TABLE_HPP
#define AV_TRACE_TABLE_HPP

#include "core/support/status.hpp"
#include "core/trace/trace_layout.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class EventRing;

class ScopeName;
class TraceHost;
struct TraceTableProbe;

class TraceTable {
public:
    static constexpr uint32_t no_slot = TraceLayout::max_slots;

    explicit TraceTable(TraceHost &host);
    TraceTable(const TraceTable &) = delete;
    TraceTable &operator=(const TraceTable &) = delete;

    [[nodiscard]] Status initialize(uint8_t *region, size_t bytes, const char *module, uint32_t process);

    uint32_t enter(const ScopeName &name);
    void     leave(uint32_t slot);

    void waiting(uint64_t lock);
    void acquired(uint64_t lock);
    void released(uint64_t lock);

    void withdraw();
    void record_events(EventRing &ring);
    void limit_depth(uint32_t depth);
    static constexpr uint32_t max_only = 16u;
    void only(const char *names);

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
    EventRing  *events_;
    uint32_t    max_depth_;
    uint32_t    only_[max_only];
    uint32_t    only_count_;
    uint8_t    *region_;
    TraceLayout layout_;
    uint32_t    slots_;

    friend struct TraceTableProbe;
};

}

#endif

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
}

namespace av::icd {

class SharedHandles;
struct OpenedObjectsProbe;

struct OpenedObjectRow {
    uint64_t object_id_;
    uint64_t into_;
    uint64_t size_;
};

class OpenedObjects {
public:
    OpenedObjects(SharedHandles &handles, BlockSource &memory, DriverLog &log);
    OpenedObjects(const OpenedObjects &) = delete;
    OpenedObjects &operator=(const OpenedObjects &) = delete;

    [[nodiscard]] Status open(ClientTransport &wire, uint32_t device, uint64_t handle, uint32_t kind, uint64_t size,
                              uint64_t into, uint64_t &made);
    bool                 answered(uint32_t kind, uint64_t made) const;
    bool                 give_back(ClientTransport &wire, uint32_t kind, uint64_t made);
    void                 give_back_into(ClientTransport &wire, uint64_t into);
    void                 release_all(ClientTransport &wire);
    uint32_t             live() const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct OpenedObjectsProbe;

    KeyedSlots<uint64_t, OpenedObjectRow>       *opens_of(uint32_t kind);
    const KeyedSlots<uint64_t, OpenedObjectRow> *opens_of(uint32_t kind) const;
    bool opened_bytes(uint32_t kind, uint64_t size, uint64_t &bytes) const;
    bool row_holds(uint32_t kind, uint64_t made, const OpenedObjectRow &row) const;
    void release(ClientTransport &wire, uint32_t kind, uint64_t made, const OpenedObjectRow &row) const;

    static constexpr uint32_t kinds = 3u;

    SharedHandles                         &handles_;
    DriverLog                             &lines_;
    KeyedSlots<uint64_t, OpenedObjectRow>  opens_[kinds];
};

}

#endif

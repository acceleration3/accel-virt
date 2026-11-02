#ifndef AV_CLICD_CL_EXTERNAL_MEMORY_HPP
#define AV_CLICD_CL_EXTERNAL_MEMORY_HPP

#include "icd/common/opened_objects.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class BlockSource;
class DriverLog;
}

namespace av {
class ClientTransport;
class StateWriter;
}

namespace av::icd {
class SharedHandles;
}

namespace av::clicd {

class ClPropertyList;
struct ClExternalMemoryProbe;

class ClExternalMemory {
public:
    static constexpr uint32_t handle_property_count = 5u;
    static constexpr uint64_t handle_properties[handle_property_count] = {
        0x2061u,
        0x2062u,
        0x2069u,
        0x2060u,
        0x2067u};

    ClExternalMemory(icd::SharedHandles &handles, BlockSource &memory, DriverLog &log);
    ClExternalMemory(const ClExternalMemory &) = delete;
    ClExternalMemory &operator=(const ClExternalMemory &) = delete;

    bool asked_for(const ClPropertyList &list) const;

    cl_int open(ClientTransport &wire, uint32_t device, const ClPropertyList &list, uint64_t size,
                uint64_t &made);

    bool   answered(uint64_t made) const;
    cl_int open_own(ClientTransport &wire, uint32_t device, uint64_t handle, uint64_t size, uint64_t &made,
                    bool &closed);

    bool   give_back(ClientTransport &wire, uint64_t made);
    void   release_all(ClientTransport &wire);

    uint32_t live() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct ClExternalMemoryProbe;

    bool                names_handle(const ClPropertyList &list, uint64_t &handle, bool &taken) const;

    cl_int              answer(Status opened) const;

    DriverLog          &log_;
    icd::SharedHandles &handles_;
    icd::OpenedObjects  opened_;
};

}

#endif

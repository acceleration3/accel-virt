#ifndef AV_GLICD_GL_EXTERNAL_OBJECTS_HPP
#define AV_GLICD_GL_EXTERNAL_OBJECTS_HPP

#include "core/support/keyed_slots.hpp"
#include "icd/common/id_mint.hpp"
#include "icd/common/opened_objects.hpp"

#include <stdint.h>

namespace av {
class DriverLog;
}

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}

namespace av::icd {
class SharedHandles;
}

namespace av::glicd {

struct GlExternalObjectsProbe;

struct GlSharedObjectRow {
    uint32_t made_;
    bool     memory_;
    bool     dedicated_;
    bool     protected_;
};

class GlExternalObjects {
public:
    enum : uint32_t {
        gl_no_error = 0u,
        gl_invalid_enum = 0x0500u,
        gl_invalid_value = 0x0501u,
        gl_invalid_operation = 0x0502u,
        gl_out_of_memory = 0x0505u,
    };
    static constexpr uint32_t opaque_win32 = 0x9587u;
    static constexpr uint32_t dedicated = 0x9581u;
    static constexpr uint32_t protected_memory = 0x959Bu;
    static constexpr uint32_t d3d12_fence_value = 0x9595u;

    GlExternalObjects(icd::SharedHandles &handles, BlockSource &memory, DriverLog &log);
    GlExternalObjects(const GlExternalObjects &) = delete;
    GlExternalObjects &operator=(const GlExternalObjects &) = delete;

    uint32_t create(uint32_t kind, int32_t count, uint32_t *names);
    uint32_t import(ClientTransport &wire, uint32_t device, uint32_t kind, uint32_t name, uint64_t size,
                    uint32_t handle_type, void *handle);
    uint32_t destroy(ClientTransport &wire, uint32_t kind, int32_t count, const uint32_t *names);
    uint32_t made(uint32_t kind, uint32_t name) const;
    bool     holds(uint32_t kind, uint32_t name) const;
    uint32_t set_parameter(uint32_t kind, uint32_t name, uint32_t pname, int32_t value);
    uint32_t get_parameter(uint32_t kind, uint32_t name, uint32_t pname, int32_t *value) const;

    void     release_all(ClientTransport &wire);
    uint32_t live() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct GlExternalObjectsProbe;

    bool                     known(uint32_t kind, bool &memory) const;
    GlSharedObjectRow       *row_of(bool memory, uint32_t name);
    const GlSharedObjectRow *row_of(bool memory, uint32_t name) const;
    void                     give_back(ClientTransport &wire, uint32_t name, const GlSharedObjectRow &row);
    uint32_t                 wire_kind(const GlSharedObjectRow &row) const;
    uint32_t                 imported() const;

    DriverLog         &log_;
    icd::OpenedObjects opened_;
    KeyedSlots<uint32_t, GlSharedObjectRow> rows_;
    icd::IdMint<uint32_t> names_;
};

}

#endif

#ifndef AV_D3D_UMD_CONNECTIONS_HPP
#define AV_D3D_UMD_CONNECTIONS_HPP

#include "guest/kmt/avgpu_adapters.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/d3d/umd_connection.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}

namespace av::d3d {

struct UmdConnectionsProbe;

class UmdConnections {
public:
    static constexpr uint32_t max_adapters = AvgpuAdapters::max_adapters;

    explicit UmdConnections(Kmt &kmt);
    ~UmdConnections();
    UmdConnections(const UmdConnections &) = delete;
    UmdConnections &operator=(const UmdConnections &) = delete;

    [[nodiscard]] Status acquire(uint64_t luid, UmdConnection *&held);

    uint32_t opens() const;
    uint32_t open() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    ObjectStorage<UmdConnection> connections_[max_adapters];

    friend struct UmdConnectionsProbe;
};

}

#endif

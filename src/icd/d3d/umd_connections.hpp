/* The avgpu adapters this process holds, one connection per adapter a
 * Direct3D runtime opened. A guest has one avgpu adapter per host GPU, and an
 * application choosing among them opens each: the device made on one must
 * reach that adapter's kernel objects, and so its host GPU, while the runtime
 * makes that device's allocations on the same adapter. */
#ifndef AV_D3D_UMD_CONNECTIONS_HPP
#define AV_D3D_UMD_CONNECTIONS_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/d3d/umd_connection.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}  // namespace av

namespace av::d3d {

struct UmdConnectionsProbe;

class UmdConnections {
public:
    /* As many adapters as the drivers keep sessions for (D3d12Driver). */
    static constexpr uint32_t max_adapters = 8u;

    explicit UmdConnections(Kmt &kmt);
    ~UmdConnections();
    UmdConnections(const UmdConnections &) = delete;
    UmdConnections &operator=(const UmdConnections &) = delete;

    /* The connection already open on `luid`'s adapter, or a closed one opened
     * on it, counting this open. no_space when every connection holds another
     * adapter; the connection's refusal otherwise. */
    [[nodiscard]] Status acquire(uint64_t luid, UmdConnection *&held);

    /* Opens counted across every connection, and how many are open. */
    uint32_t opens() const;
    uint32_t open() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    ObjectStorage<UmdConnection> connections_[max_adapters];

    friend struct UmdConnectionsProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_CONNECTIONS_HPP

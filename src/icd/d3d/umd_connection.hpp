/* One avgpu adapter of the kernel's, held open while any Direct3D runtime in
 * the process has it open (contracts/umd_interface.md).
 *
 * `OpenAdapter10_2` and `OpenAdapter12` for one adapter are two opens of it,
 * and both must reach the same kernel objects, so the adapter is found once,
 * by PCI identity and the LUID the runtime opened, and held while either open
 * is live. A guest has one avgpu adapter per host GPU; the other adapters an
 * application opens are other connections (UmdConnections).
 *
 * It is not icd::IcdConnection, which the Khronos ICDs share: that one makes a
 * D3DKMT device and paging queue per link and tracks their loss, because a
 * context is made on one. This holds the adapter and nothing else. */
#ifndef AV_D3D_UMD_CONNECTION_HPP
#define AV_D3D_UMD_CONNECTION_HPP

#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}  // namespace av

namespace av::d3d {

struct UmdConnectionProbe;

class UmdConnection {
public:
    explicit UmdConnection(Kmt &kmt);
    ~UmdConnection();
    UmdConnection(const UmdConnection &) = delete;
    UmdConnection &operator=(const UmdConnection &) = delete;

    /* Finds and holds the avgpu adapter whose LUID is `luid` (0: the first)
     * if no open holds one yet, and counts this one. conflict when an open
     * holds another adapter; not_supported when there is no such adapter; the
     * search's own failure otherwise. */
    [[nodiscard]] Status acquire(uint64_t luid);
    /* Drops one open, closing the adapter with the last. invalid_state when
     * none is held, which would mean a close without an open. */
    [[nodiscard]] Status release();

    bool     open() const;
    /* Whether it is open on the adapter an open of `luid` asked for. */
    bool     holds(uint64_t luid) const;
    /* Whether both are open on the adapter one LUID asked for. */
    bool     shares_adapter(const UmdConnection &other) const;
    uint32_t adapter() const;
    uint32_t opens() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    AdapterSearch search_;
    uint64_t      luid_;
    uint32_t      opens_;

    friend struct UmdConnectionProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_CONNECTION_HPP

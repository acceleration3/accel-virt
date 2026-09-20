/* The process's one way to the rvkgpu adapter (contracts/umd_interface.md).
 *
 * The D3D DDI has no instance: `OpenAdapter10_2` and `OpenAdapter12` in one
 * process are two opens of one adapter, and both must reach the same kernel
 * objects. So the adapter is found once, by PCI identity, and held while any
 * open is live - one open of the kernel's adapter, however many the runtimes
 * make of the driver's.
 *
 * It is not icd::IcdConnection, which the Khronos ICDs share: that one makes a
 * D3DKMT device and paging queue per link and tracks their loss, because a
 * context is made on one. Nothing is submitted here until feature 056 gives
 * the device functions something to submit, so this holds the adapter and
 * nothing else, and does not open state it cannot yet be right about. When 056
 * needs a device, IcdConnection is what it should reach for - it would want an
 * accessor for the adapter handle AdapterIdentity::learn takes.
 *
 * What is deliberately not here yet is the rvk context: nothing is submitted
 * until feature 056 gives the device functions something to submit, and the
 * transport this will carry is the one feature 053 makes shareable with the
 * Vulkan, OpenGL and OpenCL drivers in the same process (research.md §8). It
 * is injected into UmdRoot rather than reached for, so that join is a
 * different object passed to one constructor argument. */
#ifndef AV_D3D_UMD_CONNECTION_HPP
#define AV_D3D_UMD_CONNECTION_HPP

#include "core/support/status.hpp"
#include "guest/suite/adapter_search.hpp"

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

    /* Finds and holds the rvkgpu adapter if no open holds it yet, and counts
     * this one. not_supported when there is no such adapter; the search's own
     * failure otherwise. */
    [[nodiscard]] Status acquire();
    /* Drops one open, closing the adapter with the last. invalid_state when
     * none is held, which would mean a close without an open. */
    [[nodiscard]] Status release();

    bool     open() const;
    uint32_t adapter() const;
    uint32_t opens() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    AdapterSearch search_;
    uint32_t      opens_;

    friend struct UmdConnectionProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_CONNECTION_HPP

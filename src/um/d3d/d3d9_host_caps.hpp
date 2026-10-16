/* What the host GPU's Direct3D 9 can do, asked once per adapter, as d3d9.dll
 * asks it of the driver (pfnGetCaps).
 *
 * d3d9.dll asks before every device and decides from the answers alone
 * whether the adapter has a hardware device at all - and once a Direct3D 9
 * driver is named there is no fallback, so a wrong answer is no Direct3D 9
 * on this GPU (docs/research/d3d9/reverse-engineering.md §2). So this opens
 * its own connection, asks the host once (QueryCaps9: D3DCAPS9 and a row per
 * format), closes it, and keeps the answers with the few fix-ups
 * the runtime's HAL check requires of a driver that DXVK's answers, which
 * are an application's view, leave out. */
#ifndef AV_UM_D3D9_HOST_CAPS_HPP
#define AV_UM_D3D9_HOST_CAPS_HPP

#include "icd/d3d/d3d9_format_ops.hpp"
#include "proto/generated/d3d9/d3d9_wire_types.h"
#include "um/d3d/d3d9_ddi.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}  // namespace av

namespace av::d3d9 {

class D3d9HostCaps {
public:
    D3d9HostCaps();
    D3d9HostCaps(const D3d9HostCaps &) = delete;
    D3d9HostCaps &operator=(const D3d9HostCaps &) = delete;

    /* Asks the host through `adapter` (a D3DKMT adapter handle). False when
     * the adapter runs no Direct3D 9, which every GetCaps is then refused for. */
    [[nodiscard]] bool learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter);
    bool               learned() const;

    /* One GetCaps question, answered into the caller's buffer. */
    HRESULT answer(const D3DDDIARG_GETCAPS &args) const;
    /* Whether a blt-model present may name a frame of the host's surface
     * rather than read the buffer back: what the miniport's UMDRIVERPRIVATE
     * said. */
    bool    present_snapshots() const;
    const D3DCAPS9 &caps() const;

private:
    /* The runtime's HAL check (reverse-engineering.md §2.3) on what DXVK said. */
    void    fix_up();
    HRESULT copy_out(const D3DDDIARG_GETCAPS &args, const void *data, size_t bytes) const;
    HRESULT answer_formats(const D3DDDIARG_GETCAPS &args) const;
    HRESULT answer_quality(const D3DDDIARG_GETCAPS &args) const;

    D3DCAPS9            caps_;
    d3d::D3d9FormatOps  formats_;
    /* The host's row for each format asked, in the order asked: what
     * GETMULTISAMPLEQUALITYLEVELS is answered from. */
    d3d9proto::FormatCaps9 rows_[d3d::D3d9FormatOps::max_formats];
    bool                snapshots_;
    bool                learned_;
};

}  // namespace av::d3d9

#endif  // AV_UM_D3D9_HOST_CAPS_HPP

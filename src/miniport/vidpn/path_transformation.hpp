/* What one VidPn present path asks the adapter to do with its content: scale it,
 * rotate it, or run it through a gamma ramp. None of the three is implemented,
 * so each is refused with the status that names it - constitution VI, and
 * dxgkrnl rolls a refused path back instead of showing an untransformed frame. */
#ifndef AV_PATH_TRANSFORMATION_HPP
#define AV_PATH_TRANSFORMATION_HPP

#include "core/support/nt_status.hpp"

#include <stdint.h>

namespace av {

struct D3dkmdtVidPnPresentPathRecord;

class PathTransformation {
public:
    explicit PathTransformation(const D3dkmdtVidPnPresentPathRecord &path);

    /* What DxgkDdiUpdateActiveVidPnPresentPath answers: a path being made active
     * has every attribute decided, so only identity and no ramp succeed. */
    [[nodiscard]] NtStatus accepted() const;
    /* Whether DxgkDdiIsSupportedVidPn can say yes. A VidPn still being decided
     * leaves scaling and rotation Uninitialized, Unpinned or NotSpecified, which
     * pins nothing; every other value is a transformation, including the
     * Offset90 rotations whose identity content the driver must still turn. */
    [[nodiscard]] bool     supportable() const;

private:
    enum : int32_t {
        identity = 1,                  /* D3DKMDT_VPPS_IDENTITY, D3DKMDT_VPPR_IDENTITY */
        uninitialized = 0,             /* D3DKMDT_VPPS_UNINITIALIZED, D3DKMDT_VPPR_UNINITIALIZED */
        unpinned = 254,                /* D3DKMDT_VPPS_UNPINNED, D3DKMDT_VPPR_UNPINNED */
        not_specified = 255,           /* D3DKMDT_VPPS_NOTSPECIFIED, D3DKMDT_VPPR_NOTSPECIFIED */
        gamma_uninitialized = 0,       /* D3DDDI_GAMMARAMP_UNINITIALIZED */
        gamma_default = 1,             /* D3DDDI_GAMMARAMP_DEFAULT */
    };

    [[nodiscard]] bool default_gamma() const;

    int32_t scaling_;
    int32_t rotation_;
    int32_t gamma_;
};

}  // namespace av

#endif  // AV_PATH_TRANSFORMATION_HPP

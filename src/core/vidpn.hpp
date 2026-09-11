/* One VidPn dxgkrnl hands a display DDI, reached through
 * DxgkCbQueryVidPnInterface: its interface table and its topology. Every path
 * and mode set acquired through it is released - paths by for_each_path, mode
 * sets by VidPnModeSet. All of it is PASSIVE. */
#ifndef AV_VIDPN_HPP
#define AV_VIDPN_HPP

#include "ddi/dxgk_vidpn_interface_records.hpp"
#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class DxgkrnlInterface;
class PathVisitor;

class VidPn {
public:
    VidPn(Platform &platform, DxgkrnlInterface &dxgkrnl, void *handle);
    VidPn(const VidPn &) = delete;
    VidPn &operator=(const VidPn &) = delete;

    [[nodiscard]] Status initialize();

    /* Zero, not a failure, for a source the topology does not include. */
    [[nodiscard]] Status source_paths(uint32_t source, uint64_t &paths);
    [[nodiscard]] Status for_each_path(PathVisitor &visitor);
    [[nodiscard]] Status update_path_support(const D3dkmdtVidPnPresentPathRecord &path);

    /* The interface's type names which kind of mode set: source or target. */
    [[nodiscard]] Status acquire_mode_set(uint32_t source, void *&mode_set,
                                          const DxgkVidPnSourceModeSetInterfaceRecord *&mode_set_interface);
    [[nodiscard]] Status acquire_mode_set(uint32_t target, void *&mode_set,
                                          const DxgkVidPnTargetModeSetInterfaceRecord *&mode_set_interface);
    [[nodiscard]] Status create_mode_set(uint32_t source, void *&mode_set,
                                         const DxgkVidPnSourceModeSetInterfaceRecord *&mode_set_interface);
    [[nodiscard]] Status create_mode_set(uint32_t target, void *&mode_set,
                                         const DxgkVidPnTargetModeSetInterfaceRecord *&mode_set_interface);
    [[nodiscard]] Status release_mode_set(void *mode_set, const DxgkVidPnSourceModeSetInterfaceRecord *mode_set_interface);
    [[nodiscard]] Status release_mode_set(void *mode_set, const DxgkVidPnTargetModeSetInterfaceRecord *mode_set_interface);
    /* On success the VidPn owns the set; on failure the caller still does. */
    [[nodiscard]] Status assign_mode_set(uint32_t source, void *mode_set,
                                         const DxgkVidPnSourceModeSetInterfaceRecord *mode_set_interface);
    [[nodiscard]] Status assign_mode_set(uint32_t target, void *mode_set,
                                         const DxgkVidPnTargetModeSetInterfaceRecord *mode_set_interface);

private:
    template <typename ModeSetInterface>
    [[nodiscard]] Status obtain(int32_t (*call)(void *, uint32_t, void **, const ModeSetInterface **), uint32_t owner,
                                void *&mode_set, const ModeSetInterface *&mode_set_interface);
    [[nodiscard]] Status release_path(const D3dkmdtVidPnPresentPathRecord *path);

    Platform                               &platform_;
    DxgkrnlInterface                       &dxgkrnl_;
    void                                   *handle_;
    const DxgkVidPnInterfaceRecord         *interface_;
    void                                   *topology_;
    const DxgkVidPnTopologyInterfaceRecord *topology_interface_;
};

}  // namespace av

#endif  // AV_VIDPN_HPP

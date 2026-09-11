/* A callback of dxgkrnl's VidPn, topology and mode set interfaces that
 * FakeVidPn can be told to refuse. The mode info calls apply to source,
 * target and monitor mode sets alike. */
#ifndef AV_VIDPN_CALL_HPP
#define AV_VIDPN_CALL_HPP

namespace av {

enum class VidPnCall : unsigned {
    get_topology,
    acquire_source_mode_set,
    create_source_mode_set,
    assign_source_mode_set,
    acquire_target_mode_set,
    create_target_mode_set,
    assign_target_mode_set,
    paths_from_source,
    acquire_first_path,
    acquire_next_path,
    update_path_support,
    release_path,
    acquire_pinned_mode,
    create_mode_info,
    add_mode,
    count
};

}  // namespace av

#endif  // AV_VIDPN_CALL_HPP

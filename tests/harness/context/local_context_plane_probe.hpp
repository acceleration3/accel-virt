/* What a test of the device's control plane reaches behind a
 * LocalContextPlane: the table of contexts and the library they run on. */
#ifndef AV_LOCAL_CONTEXT_PLANE_PROBE_HPP
#define AV_LOCAL_CONTEXT_PLANE_PROBE_HPP

#include "device/context/local_context_plane.hpp"

namespace av {

struct LocalContextPlaneProbe {
    static DeviceContextTable &table(LocalContextPlane &plane) { return plane.contexts_; }
    static ExecutorLibrary    &library(LocalContextPlane &plane) { return plane.library_; }
};

}  // namespace av

#endif  // AV_LOCAL_CONTEXT_PLANE_PROBE_HPP

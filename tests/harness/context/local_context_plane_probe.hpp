#ifndef AV_LOCAL_CONTEXT_PLANE_PROBE_HPP
#define AV_LOCAL_CONTEXT_PLANE_PROBE_HPP

#include "device/context/local_context_plane.hpp"

namespace av {

struct LocalContextPlaneProbe {
    static DeviceContextTable &table(LocalContextPlane &plane) { return plane.contexts_; }
    static ExecutorLibrary    &library(LocalContextPlane &plane) { return plane.library_; }
};

}

#endif

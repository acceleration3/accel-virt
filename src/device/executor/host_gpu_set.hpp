/* Every host GPU this process found, and the one a UUID names.
 *
 * The device names a GPU by its Vulkan device UUID because enumeration order
 * does not survive a reboot or a driver update; this is what turns that name
 * back into something a backend can open. */
#ifndef AV_HOST_GPU_SET_HPP
#define AV_HOST_GPU_SET_HPP

#include "core/support/status.hpp"
#include "device/executor/host_gpu.hpp"

#include <stdint.h>

namespace av {

class HostApis;

class HostGpuSet {
public:
    /* This machine has two. A HostGpu is under a hundred bytes, so the ceiling
     * costs nothing and no host anyone will run this on comes near it. */
    static constexpr uint32_t max_gpus = 8u;

    HostGpuSet();
    HostGpuSet(const HostGpuSet &) = delete;
    HostGpuSet &operator=(const HostGpuSet &) = delete;

    /* Asks `apis` once. A host with no GPU is an empty set and ok: the library
     * still loads, still answers, and offers no sub-API - which is what stops
     * the device advertising a capset it cannot honour. More GPUs than
     * max_gpus keeps the first max_gpus and says no_space. */
    [[nodiscard]] Status initialize(HostApis &apis);

    uint32_t       count() const;
    /* The GPU whose deviceUUID is `uuid`, or null. */
    const HostGpu *find(const uint8_t *uuid) const;
    /* Every sub-API any GPU here can run, which is what the ABI's sub_apis
     * answers and the capset carries. */
    uint32_t       sub_apis() const;

    bool invariants_hold() const;

private:
    HostGpu  gpus_[max_gpus];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_HOST_GPU_SET_HPP

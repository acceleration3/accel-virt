/* One host GPU, as the executor library sees it: what it is called, what it can
 * run, and what it can be handed without a copy. */
#ifndef AV_HOST_GPU_HPP
#define AV_HOST_GPU_HPP

#include "proto/context/device_uuid.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class HostApis;

class HostGpu {
public:
    HostGpu();
    HostGpu(const HostGpu &) = delete;
    HostGpu &operator=(const HostGpu &) = delete;

    /* Caches what `apis` says about `index`, which must be below
     * `apis.gpu_count()`. The only caller is HostGpuSet, which asks for exactly
     * the indices below the count it just read, so a range check here would be
     * one no caller can fail. Asking once is the point: sub_apis() is answered
     * on QEMU's main loop during realize, and the answer is frozen into the
     * capset for the device's lifetime. */
    void describe(HostApis &apis, uint32_t index);

    bool     matches(const uint8_t *uuid) const;
    uint32_t sub_apis() const;
    /* The AV_BLOB_* kinds `sub_api` imports here; zero for a sub-API this GPU
     * does not serve, and zero for one that serves it only by copying. */
    uint32_t import_kinds(uint32_t sub_api) const;
    /* Where this GPU sat in the host's enumeration, which is what a backend
     * needs to open it again. */
    uint32_t index() const;
    /* Its line in the executor's state dump: where it sat, its deviceUUID and
     * what it runs. */
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct HostGpuProbe;

    /* Vulkan, OpenGL, OpenCL: the three bits the ABI defines, and the only
     * sub-APIs a context can name. */
    static constexpr uint32_t sub_api_count = 4u;

    /* The slot `sub_api`'s kinds live in, or sub_api_count for a value that is
     * not one of the three bits. */
    uint32_t slot_of(uint32_t sub_api) const;

    DeviceUuid uuid_;
    uint32_t sub_apis_;
    uint32_t kinds_[sub_api_count];
    uint32_t index_;
    bool     known_;
};

}  // namespace av

#endif  // AV_HOST_GPU_HPP

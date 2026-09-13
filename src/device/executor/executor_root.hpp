/* What the executor library is, behind the one symbol it exports.
 *
 * Named a root rather than a library because the device already has an
 * `av::ExecutorLibrary` - the table it loaded - and these are opposite ends of
 * the same ABI. This is the end that answers.
 *
 * There is exactly one of these in a process, in `ObjectStorage` in the
 * translation unit that defines the C entry points, because `rvk_executor_ops()`
 * takes no argument and so has nowhere to put a `this` (CLAUDE.md). */
#ifndef AV_EXECUTOR_ROOT_HPP
#define AV_EXECUTOR_ROOT_HPP

#include "core/support/status.hpp"
#include "device/executor/host_gpu_set.hpp"

#include <stdint.h>

namespace av {

class HostApis;

class ExecutorRoot {
public:
    ExecutorRoot();
    ExecutorRoot(const ExecutorRoot &) = delete;
    ExecutorRoot &operator=(const ExecutorRoot &) = delete;

    /* Asks the host's loaders once, on QEMU's main loop, before any context:
     * the answer to sub_apis() is frozen into the capset for the device's
     * lifetime, so it has to be answerable with no GPU context in hand. */
    [[nodiscard]] Status initialize(HostApis &apis);

    /* Every sub-API some GPU here can run. Zero is an answer: the device then
     * describes no capset at all rather than one it cannot honour. */
    uint32_t sub_apis() const;
    /* The RVK_BLOB_* kinds `sub_api` imports on the GPU `gpu_uuid` names.
     * Zero for a GPU nobody has, which is also what a null UUID gets. */
    uint32_t import_kinds(uint32_t sub_api, const uint8_t *gpu_uuid) const;

    bool invariants_hold() const;

private:
    HostGpuSet gpus_;
    bool       asked_;
};

}  // namespace av

#endif  // AV_EXECUTOR_ROOT_HPP

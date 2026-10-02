/* One shared object's release, told to the device: the request, the answer,
 * and the exchange between them.
 *
 * The device holds what was shared under an id for as long as the guest holds
 * the allocation the id was minted over - past the context that shared it,
 * which on Windows a surface outlives whenever DWM still has it. dxgkrnl
 * destroys that allocation only once every process has let go, so this is sent
 * after DestroyAllocation and is the only thing that lets the device's copy go.
 * An answer the device refuses is its to report; an id it never held it takes
 * as done. */
#ifndef AV_SHARED_OBJECT_RELEASE_HPP
#define AV_SHARED_OBJECT_RELEASE_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class VirtioGpuDevice;

class SharedObjectRelease {
public:
    explicit SharedObjectRelease(Platform &platform);
    SharedObjectRelease(const SharedObjectRelease &) = delete;
    SharedObjectRelease &operator=(const SharedObjectRelease &) = delete;

    /* PASSIVE, and waits for the answer: the caller holds whatever keeps the
     * control queue to one exchange at a time. The device's refusal is
     * not_supported. */
    [[nodiscard]] Status send(VirtioGpuDevice &device, uint64_t object_id);

private:
    static constexpr size_t request_bytes = 32u;
    static constexpr size_t answer_bytes = 24u;

    Platform &platform_;
    uint8_t   request_[request_bytes];
    uint8_t   answer_[answer_bytes];
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_RELEASE_HPP

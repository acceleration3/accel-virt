/* A present's pixels moved by the host (PresentCopyRequest): each surface's
 * pages and the regions are written into a payload of contiguous memory this
 * owns, the request carries it as a second buffer, and the call returns once
 * the device has answered - after every pixel moved. The host reads guest RAM
 * through its own cache; the miniport's CPU sees these pages uncached, which
 * held a game's 1280x800 blt-model present to thirty milliseconds. A present
 * the host cannot take - more pages or regions than the payload holds, a
 * device that is gone, does not answer or refuses - moves through `fallback`
 * instead. The payload is one, so presents take turns. */
#ifndef AV_PRESENT_COPIER_HPP
#define AV_PRESENT_COPIER_HPP

#include "core/platform/wait_mutex.hpp"
#include "miniport/present/pixel_mover.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Platform;
class PresentCopyRequest;
class PresentRegion;
class PresentSurface;
class VirtioGpuDevice;

class PresentCopier final : public PixelMover {
public:
    /* Room for 16384 memory entries: two 3840x2160 surfaces even when every
     * page of both is a run of its own. */
    static constexpr size_t payload_bytes = 0x40000u;

    PresentCopier(Platform &platform, VirtioGpuDevice &device, PixelMover &fallback);
    ~PresentCopier();
    PresentCopier(const PresentCopier &) = delete;
    PresentCopier &operator=(const PresentCopier &) = delete;

    /* The payload: no_memory when there is no contiguous memory for it. */
    [[nodiscard]] Status initialize();

    [[nodiscard]] Status copy(const PresentSurface &source, const PresentSurface &destination,
                              const PresentRegion *regions, uint32_t count) override;
    [[nodiscard]] Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                              uint32_t count) override;

private:
    static constexpr size_t answer_bytes = 24u;

    /* The request, with `source` (null for a fill), `destination` and the
     * regions as its payload, answered: not_supported when the device refused
     * it, too_large when the payload cannot hold it. */
    [[nodiscard]] Status ask(const PresentCopyRequest &request, const PresentSurface *source,
                             const PresentSurface &destination, const PresentRegion *regions, uint32_t count);
    /* The first refusal only: every present after it would say the same. */
    void                 log_refusal();

    Platform        &platform_;
    VirtioGpuDevice &device_;
    PixelMover      &fallback_;
    WaitMutex        payloads_;
    uint8_t         *payload_;
    uint64_t         payload_address_;
    bool             refusal_logged_;
};

}  // namespace av

#endif  // AV_PRESENT_COPIER_HPP

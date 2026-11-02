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
    static constexpr size_t payload_bytes = 0x40000u;

    PresentCopier(Platform &platform, VirtioGpuDevice &device, PixelMover &fallback);
    ~PresentCopier();
    PresentCopier(const PresentCopier &) = delete;
    PresentCopier &operator=(const PresentCopier &) = delete;

    [[nodiscard]] Status initialize();

    [[nodiscard]] Status copy(const PresentSurface &source, const PresentSurface &destination,
                              const PresentRegion *regions, uint32_t count) override;
    [[nodiscard]] Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                              uint32_t count) override;

private:
    static constexpr size_t answer_bytes = 24u;

    [[nodiscard]] Status ask(const PresentCopyRequest &request, const PresentSurface *source,
                             const PresentSurface &destination, const PresentRegion *regions, uint32_t count);
    void                 log_refusal();

    Platform        &platform_;
    VirtioGpuDevice &device_;
    PixelMover      &fallback_;
    WaitMutex        payloads_;
    uint8_t         *payload_;
    uint64_t         payload_address_;
    bool             refusal_logged_;
};

}

#endif

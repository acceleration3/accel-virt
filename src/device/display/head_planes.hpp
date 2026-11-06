#ifndef AV_HEAD_PLANES_HPP
#define AV_HEAD_PLANES_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/virtio_gpu/scanout_plane_request.hpp"

#include <stdint.h>

namespace av {

class Host;

class HeadPlanes {
public:
    static constexpr uint32_t heads = 16u;
    static constexpr uint32_t layers = ScanoutPlaneRequest::layers;

    explicit HeadPlanes(Host &host);
    ~HeadPlanes();
    HeadPlanes(const HeadPlanes &) = delete;
    HeadPlanes &operator=(const HeadPlanes &) = delete;

    [[nodiscard]] Status initialize();

    [[nodiscard]] Status set(const ScanoutPlaneRequest &plane);
    bool                 composed(uint32_t head) const;
    uint32_t             planes(uint32_t head, AvScanoutPlane *planes, uint32_t &width, uint32_t &height) const;
    void                 clear();

    bool invariants_hold() const;

private:
    bool composed_locked(uint32_t head) const;

    Host          &host_;
    void          *lock_;
    AvScanoutPlane planes_[heads][layers];
    bool           shown_[heads][layers];
    uint32_t       widths_[heads];
    uint32_t       heights_[heads];

    friend struct HeadPlanesProbe;
};

}

#endif

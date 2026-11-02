#include "device/display/head_planes.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

HeadPlanes::HeadPlanes(Host &host) : host_(host), lock_(nullptr), planes_{}, shown_{}, widths_{}, heights_{}
{
    AV_SCOPE("HeadPlanes::HeadPlanes");
}

HeadPlanes::~HeadPlanes()
{
    AV_SCOPE("HeadPlanes::~HeadPlanes");
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status HeadPlanes::initialize()
{
    AV_SCOPE("HeadPlanes::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_);
}

Status HeadPlanes::set(const ScanoutPlaneRequest &plane)
{
    AV_SCOPE("HeadPlanes::set");
    const uint32_t head = plane.head();
    const uint32_t layer = plane.layer();
    if (head >= heads || layer >= layers) return Status::invalid_parameter;
    if (lock_ == nullptr) return Status::invalid_state;
    host_.lock_mutex(lock_);
    widths_[head] = plane.head_width();
    heights_[head] = plane.head_height();
    shown_[head][layer] = plane.shown();
    AvScanoutPlane &kept = planes_[head][layer];
    kept = AvScanoutPlane{};
    if (plane.shown()) {
        kept.key = plane.key();
        kept.layer = layer;
        kept.flags = (plane.filtered() ? AV_PLANE_FILTERED : 0u) |
                     (plane.ycbcr() ? AV_PLANE_YCBCR | (plane.bt709() ? AV_PLANE_BT709 : 0u) |
                                          (plane.full_range() ? AV_PLANE_FULL_RANGE : 0u)
                                    : 0u);
        for (uint32_t edge = 0u; edge < 4u; edge++) {
            kept.source[edge] = plane.source()[edge];
            kept.destination[edge] = plane.destination()[edge];
        }
    }
    host_.unlock_mutex(lock_);
    return Status::ok;
}

bool HeadPlanes::composed(uint32_t head) const
{
    AV_SCOPE("HeadPlanes::composed");
    if (head >= heads || lock_ == nullptr) return false;
    host_.lock_mutex(lock_);
    const bool composed = composed_locked(head);
    host_.unlock_mutex(lock_);
    return composed;
}

bool HeadPlanes::composed_locked(uint32_t head) const
{
    AV_SCOPE("HeadPlanes::composed_locked");
    for (uint32_t layer = 1u; layer < layers; layer++)
        if (shown_[head][layer]) return true;
    if (!shown_[head][0]) return false;
    const AvScanoutPlane &base = planes_[head][0];
    const int32_t         width = static_cast<int32_t>(widths_[head]);
    const int32_t         height = static_cast<int32_t>(heights_[head]);
    const bool fills = base.destination[0] == 0 && base.destination[1] == 0 && base.destination[2] == width &&
                       base.destination[3] == height && base.source[0] == 0 && base.source[1] == 0 &&
                       base.source[2] == width && base.source[3] == height;
    return !fills;
}

uint32_t HeadPlanes::planes(uint32_t head, AvScanoutPlane *planes, uint32_t &width, uint32_t &height) const
{
    AV_SCOPE("HeadPlanes::planes");
    width = 0u;
    height = 0u;
    if (head >= heads || lock_ == nullptr) return 0u;
    host_.lock_mutex(lock_);
    uint32_t count = 0u;
    for (uint32_t layer = 0u; layer < layers; layer++)
        if (shown_[head][layer]) planes[count++] = planes_[head][layer];
    width = widths_[head];
    height = heights_[head];
    host_.unlock_mutex(lock_);
    return count;
}

void HeadPlanes::clear()
{
    AV_SCOPE("HeadPlanes::clear");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    for (uint32_t head = 0u; head < heads; head++) {
        for (uint32_t layer = 0u; layer < layers; layer++) {
            planes_[head][layer] = AvScanoutPlane{};
            shown_[head][layer] = false;
        }
        widths_[head] = 0u;
        heights_[head] = 0u;
    }
    host_.unlock_mutex(lock_);
}

bool HeadPlanes::invariants_hold() const
{
    AV_SCOPE("HeadPlanes::invariants_hold");
    for (uint32_t head = 0u; head < heads; head++)
        for (uint32_t layer = 0u; layer < layers; layer++)
            if (!shown_[head][layer] && planes_[head][layer].key != 0u) return false;
    return true;
}

}

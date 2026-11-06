#include "miniport/display/plane_scanout.hpp"

#include "miniport/memory/allocation.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

namespace av {

namespace {

int64_t larger(int64_t one, int64_t other)
{
    AV_SCOPE("larger");
    return one > other ? one : other;
}

int64_t smaller(int64_t one, int64_t other)
{
    AV_SCOPE("smaller");
    return one < other ? one : other;
}

uint32_t conversion(uint32_t colour_space)
{
    AV_SCOPE("conversion");
    constexpr uint32_t first_ycbcr = 5u, last_ycbcr = 9u;
    if (colour_space < first_ycbcr || colour_space > last_ycbcr) return 0u;
    const bool bt709 = colour_space >= 8u;
    const bool full = colour_space == 5u || colour_space == 7u || colour_space == 9u;
    return ScanoutPlaneRequest::flag_ycbcr | (bt709 ? ScanoutPlaneRequest::flag_bt709 : 0u) |
           (full ? ScanoutPlaneRequest::flag_full_range : 0u);
}

bool same(const ScanoutPlaneRequest &one, const ScanoutPlaneRequest &other)
{
    AV_SCOPE("same");
    if (one.shown() != other.shown() || one.head_width() != other.head_width() ||
        one.head_height() != other.head_height())
        return false;
    if (!one.shown()) return true;
    if (one.key() != other.key() || one.filtered() != other.filtered() || one.ycbcr() != other.ycbcr() ||
        one.bt709() != other.bt709() || one.full_range() != other.full_range())
        return false;
    for (uint32_t edge = 0u; edge < 4u; edge++)
        if (one.source()[edge] != other.source()[edge] || one.destination()[edge] != other.destination()[edge])
            return false;
    return true;
}

}

PlaneScanout::PlaneScanout(ObjectStorage<VirtioGpuDevice> &device) : device_(device), sent_(), told_{}
{
    AV_SCOPE("PlaneScanout::PlaneScanout");
}

ScanoutPlaneRequest PlaneScanout::request(uint32_t source, const OverlayPlane &plane, uint32_t width,
                                          uint32_t height) const
{
    AV_SCOPE("PlaneScanout::request");
    const ScanoutPlaneRequest off(source, plane.layer(), 0u, 0u, nullptr, nullptr, width, height);
    if (!plane.shown()) return off;
    const uint64_t key = static_cast<const Allocation *>(plane.allocation())->host_key();
    const RectRecord &from = plane.source();
    const RectRecord &into = plane.destination();
    const RectRecord &clip = plane.clip();
    const int64_t     into_width = static_cast<int64_t>(into.right) - into.left;
    const int64_t     into_height = static_cast<int64_t>(into.bottom) - into.top;
    const int64_t     from_width = static_cast<int64_t>(from.right) - from.left;
    const int64_t     from_height = static_cast<int64_t>(from.bottom) - from.top;
    if (key == 0u || into_width <= 0 || into_height <= 0 || from_width <= 0 || from_height <= 0) return off;
    const bool    clipped = clip.right > clip.left && clip.bottom > clip.top;
    const int64_t left = larger(larger(into.left, clipped ? clip.left : into.left), 0);
    const int64_t top = larger(larger(into.top, clipped ? clip.top : into.top), 0);
    const int64_t right = smaller(smaller(into.right, clipped ? clip.right : into.right), width);
    const int64_t bottom = smaller(smaller(into.bottom, clipped ? clip.bottom : into.bottom), height);
    if (right <= left || bottom <= top) return off;
    const int32_t shown_into[4] = {static_cast<int32_t>(left), static_cast<int32_t>(top), static_cast<int32_t>(right),
                                   static_cast<int32_t>(bottom)};
    int32_t shown_from[4] = {
        static_cast<int32_t>(from.left + (left - into.left) * from_width / into_width),
        static_cast<int32_t>(from.top + (top - into.top) * from_height / into_height),
        static_cast<int32_t>(from.left + (right - into.left) * from_width / into_width),
        static_cast<int32_t>(from.top + (bottom - into.top) * from_height / into_height)};
    if (shown_from[2] <= shown_from[0]) shown_from[2] = shown_from[0] + 1;
    if (shown_from[3] <= shown_from[1]) shown_from[3] = shown_from[1] + 1;
    const uint32_t flags = ScanoutPlaneRequest::flag_shown |
                           (plane.filtered() ? ScanoutPlaneRequest::flag_filtered : 0u) | conversion(plane.colour_space());
    return ScanoutPlaneRequest(source, plane.layer(), flags, key, shown_from, shown_into, width, height);
}

Status PlaneScanout::carry(uint32_t source, const OverlayPlane *planes, uint32_t width, uint32_t height)
{
    AV_SCOPE("PlaneScanout::carry");
    if (source >= DisplaySources::max_sources || width == 0u || height == 0u) return Status::invalid_parameter;
    if (!device_.constructed()) return Status::invalid_state;
    for (uint32_t layer = 0u; layer < OverlayPlane::max_planes; layer++) {
        const OverlayPlane        at(layer, 0u);
        const OverlayPlane       &plane = planes[layer].flipped() ? planes[layer] : at;
        const ScanoutPlaneRequest wanted = request(source, plane, width, height);
        if (told_[source][layer] && same(wanted, sent_[source][layer])) continue;
        uint8_t         bytes[AvRequestWriter::max_request_size] = {};
        AvRequestWriter writer(bytes, sizeof bytes);
        static_cast<void>(writer.scanout_plane(wanted));
        const Status sent = device_.object().send_control(bytes, writer.length());
        if (sent != Status::ok) return sent;
        sent_[source][layer] = wanted;
        told_[source][layer] = true;
    }
    return Status::ok;
}

void PlaneScanout::forget_device()
{
    AV_SCOPE("PlaneScanout::forget_device");
    for (auto &layers : told_)
        for (bool &told : layers) told = false;
}

}

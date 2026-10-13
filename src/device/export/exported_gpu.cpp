#include "device/export/exported_gpu.hpp"

#include "device/display/head_planes.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/context_plane.hpp"
#include "device/host/host.hpp"
#include "proto/export/export_request.hpp"
#include "proto/export/exported_layout.hpp"

namespace av {

ExportedGpu::ExportedGpu(Host &host) : host_(host), contexts_(nullptr), heads_(nullptr), gpu_uuid_(nullptr), greeting_()
{
    AV_SCOPE("ExportedGpu::ExportedGpu");
}

void ExportedGpu::attach(ContextPlane &contexts, const uint8_t *gpu_uuid, const ExportGreeting &greeting,
                         HeadPlanes &heads)
{
    AV_SCOPE("ExportedGpu::attach");
    host_.require_main_loop("ExportedGpu::attach");
    contexts_ = &contexts;
    heads_ = &heads;
    gpu_uuid_ = gpu_uuid;
    greeting_ = greeting;
}

bool ExportedGpu::exports() const
{
    AV_SCOPE("ExportedGpu::exports");
    return contexts_ != nullptr && contexts_->exports_surfaces();
}

ExportAnswer ExportedGpu::greet(const ExportRequest &request) const
{
    AV_SCOPE("ExportedGpu::greet");
    if (!exports()) return ExportAnswer(request, Status::invalid_state);
    ExportAnswer answer(request, Status::ok);
    answer.greet(greeting_);
    return answer;
}

ExportAnswer ExportedGpu::export_surface(const ExportRequest &request, int &descriptor) const
{
    AV_SCOPE("ExportedGpu::export_surface");
    descriptor = -1;
    if (!exports()) return ExportAnswer(request, Status::invalid_state);
    AvExportedImage image = {-1, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    if (!contexts_->export_surface(gpu_uuid_, request.surface(), image)) return ExportAnswer(request, Status::not_found);
    const ExportedLayout layout(image.width, image.height, image.fourcc, image.stride, image.offset, image.modifier);
    if (image.fd < 0 || !layout.describes_a_picture()) {
        if (image.fd >= 0) host_.close_export(image.fd);
        host_.log("virtio-gpu-av: the executor exported a surface with no picture or no dma-buf; refused");
        return ExportAnswer(request, Status::not_found);
    }
    ExportAnswer answer(request, Status::ok);
    answer.describe(layout);
    descriptor = image.fd;
    return answer;
}

ExportAnswer ExportedGpu::sync(const ExportRequest &request, int &descriptor) const
{
    AV_SCOPE("ExportedGpu::sync");
    descriptor = -1;
    if (!exports()) return ExportAnswer(request, Status::invalid_state);
    uint64_t writes = 0u;
    int      fence = -1;
    if (!contexts_->sync_surface(gpu_uuid_, request.surface(), writes, fence) || fence < 0)
        return ExportAnswer(request, Status::not_found);
    ExportAnswer answer(request, Status::ok);
    answer.count(writes);
    descriptor = fence;
    return answer;
}

/* The executor keeps the image it composed into, so what goes out is a
 * descriptor of the export path's own for it. */
ExportAnswer ExportedGpu::export_head(const ExportRequest &request, int &descriptor) const
{
    AV_SCOPE("ExportedGpu::export_head");
    descriptor = -1;
    if (contexts_ == nullptr || heads_ == nullptr || !contexts_->composes())
        return ExportAnswer(request, Status::invalid_state);
    AvScanoutPlane planes[HeadPlanes::layers] = {};
    uint32_t       width = 0u;
    uint32_t       height = 0u;
    const uint32_t count = heads_->planes(request.head(), planes, width, height);
    AvExportedImage image = {-1, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    if (count == 0u || !contexts_->compose_planes(gpu_uuid_, planes, count, width, height, image))
        return ExportAnswer(request, Status::not_found);
    const ExportedLayout layout(image.width, image.height, image.fourcc, image.stride, image.offset, image.modifier);
    const int owned = image.fd >= 0 && layout.describes_a_picture() ? host_.duplicate_export(image.fd) : -1;
    if (owned < 0) return ExportAnswer(request, Status::not_found);
    ExportAnswer answer(request, Status::ok);
    answer.describe(layout);
    descriptor = owned;
    return answer;
}

}  // namespace av

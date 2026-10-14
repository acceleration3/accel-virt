#include "device/fence/fence_window.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/fence_page.hpp"

namespace av {

FenceWindow::FenceWindow() : blob_()
{
    AV_SCOPE("FenceWindow::FenceWindow");
}

FenceWindow::FenceWindow(const BlobWindow &blob) : blob_(blob)
{
    AV_SCOPE("FenceWindow::FenceWindow");
}

Status FenceWindow::publish(const FencePage &page) const
{
    AV_SCOPE("FenceWindow::publish");
    uint8_t bytes[FencePage::encoded_size] = {};
    static_cast<void>(page.encode(bytes, sizeof bytes));   /* sized for it */
    if (!invariants_hold()) return Status::invalid_parameter;
    return blob_.write(0u, bytes, FencePage::beat_at);
}

Status FenceWindow::publish_beat(uint64_t beat) const
{
    AV_SCOPE("FenceWindow::publish_beat");
    if (!invariants_hold()) return Status::invalid_parameter;
    return blob_.store_u64(FencePage::beat_at, beat);
}

Status FenceWindow::read(FencePage &page) const
{
    AV_SCOPE("FenceWindow::read");
    uint8_t      bytes[FencePage::encoded_size] = {};
    const Status status = blob_.read(0u, bytes, sizeof bytes);
    if (status != Status::ok) return status;
    return page.decode(bytes, sizeof bytes);
}

bool FenceWindow::invariants_hold() const
{
    AV_SCOPE("FenceWindow::invariants_hold");
    return blob_.length() >= FencePage::encoded_size;
}

}  // namespace av

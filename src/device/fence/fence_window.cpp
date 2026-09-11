#include "device/fence/fence_window.hpp"

#include "proto/submission/fence_page.hpp"

namespace av {

FenceWindow::FenceWindow() : blob_() {}

FenceWindow::FenceWindow(const BlobWindow &blob) : blob_(blob) {}

Status FenceWindow::publish(const FencePage &page) const
{
    uint8_t bytes[FencePage::encoded_size] = {};
    static_cast<void>(page.encode(bytes, sizeof bytes));   /* sized for it */
    return blob_.write(0u, bytes, sizeof bytes);
}

bool FenceWindow::invariants_hold() const
{
    return blob_.length() >= FencePage::encoded_size;
}

}  // namespace av

#include "device/present/present_page_window.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/pages/present_progress.hpp"

namespace av {

PresentPageWindow::PresentPageWindow(Host &host) : host_(host), page_(nullptr)
{
    AV_SCOPE("PresentPageWindow::PresentPageWindow");
}

PresentPageWindow::~PresentPageWindow()
{
    AV_SCOPE("PresentPageWindow::~PresentPageWindow");
    release();
}

Status PresentPageWindow::map(uint64_t address)
{
    AV_SCOPE("PresentPageWindow::map");
    host_.require_main_loop("PresentPageWindow::map");
    release();
    uint64_t    length = PresentProgress::page_size;
    bool        direct = false;
    void *const pointer = host_.map_guest(address, length, direct);
    if (pointer == nullptr || length == 0u) return Status::not_found;
    if (!direct || length < PresentProgress::page_size) {
        host_.unmap_guest(pointer, length);
        return Status::not_supported;
    }
    page_ = static_cast<uint8_t *>(pointer);
    __builtin_memset(page_, 0, PresentProgress::page_size);
    static_cast<void>(PresentProgress().encode(page_, PresentProgress::page_size));
    return Status::ok;
}

void PresentPageWindow::release()
{
    AV_SCOPE("PresentPageWindow::release");
    if (page_ == nullptr) return;
    host_.unmap_guest(page_, PresentProgress::page_size);
    page_ = nullptr;
}

bool PresentPageWindow::mapped() const
{
    AV_SCOPE("PresentPageWindow::mapped");
    return page_ != nullptr;
}

uint64_t *PresentPageWindow::word(uint64_t offset) const
{
    AV_SCOPE("PresentPageWindow::word");
    return reinterpret_cast<uint64_t *>(page_ + offset);
}

void PresentPageWindow::publish(const PresentProgress &progress)
{
    AV_SCOPE("PresentPageWindow::publish");
    if (page_ == nullptr) return;
    __atomic_store_n(word(PresentProgress::completed_at), progress.completed(), __ATOMIC_RELAXED);
    __atomic_store_n(word(PresentProgress::refused_at), progress.refused(), __ATOMIC_RELEASE);
}

}  // namespace av

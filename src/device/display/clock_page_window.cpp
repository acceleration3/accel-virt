#include "device/display/clock_page_window.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

namespace av {

ClockPageWindow::ClockPageWindow(Host &host) : host_(host), page_(nullptr), heads_(0u)
{
    AV_SCOPE("ClockPageWindow::ClockPageWindow");
}

ClockPageWindow::~ClockPageWindow()
{
    AV_SCOPE("ClockPageWindow::~ClockPageWindow");
    release();
}

Status ClockPageWindow::map(uint64_t address, uint32_t heads)
{
    AV_SCOPE("ClockPageWindow::map");
    host_.require_main_loop("ClockPageWindow::map");
    release();
    const ClockPageHeader header(heads);
    uint8_t               encoded[ClockPageHeader::encoded_size] = {};
    if (header.encode(encoded, sizeof encoded) != Status::ok) return Status::invalid_parameter;
    uint64_t    length = ClockPageHeader::page_size;
    bool        direct = false;
    void *const pointer = host_.map_guest(address, length, direct);
    if (pointer == nullptr || length == 0u) return Status::not_found;
    if (!direct || length < ClockPageHeader::page_size) {
        host_.unmap_guest(pointer, length);
        return Status::not_supported;
    }
    page_ = static_cast<uint8_t *>(pointer);
    heads_ = heads;
    __builtin_memset(page_, 0, ClockPageHeader::page_size);
    __builtin_memcpy(page_, encoded, sizeof encoded);
    return Status::ok;
}

void ClockPageWindow::release()
{
    AV_SCOPE("ClockPageWindow::release");
    if (page_ == nullptr) return;
    host_.unmap_guest(page_, ClockPageHeader::page_size);
    page_ = nullptr;
    heads_ = 0u;
}

bool ClockPageWindow::mapped() const
{
    AV_SCOPE("ClockPageWindow::mapped");
    return page_ != nullptr;
}

uint32_t *ClockPageWindow::word(uint64_t offset) const
{
    AV_SCOPE("ClockPageWindow::word");
    return reinterpret_cast<uint32_t *>(page_ + offset);
}

/* The generation goes odd before the first field changes and even after the
 * last, each store ordered after what came before it: a guest that reads the
 * same even generation on both sides of its copy copied one whole record. */
void ClockPageWindow::publish(uint32_t head, const VblankRecord &blank)
{
    AV_SCOPE("ClockPageWindow::publish");
    if (page_ == nullptr || head >= heads_) return;
    uint8_t encoded[VblankRecord::encoded_size] = {};
    static_cast<void>(blank.encode(encoded, sizeof encoded));
    const uint64_t record = ClockPageHeader(heads_).record_offset(head);
    uint32_t *const generation = word(record + VblankRecord::generation_at);
    const uint32_t  before = __atomic_load_n(generation, __ATOMIC_RELAXED);
    __atomic_store_n(generation, before + 1u, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    for (uint64_t at = 0u; at < VblankRecord::encoded_size; at += sizeof(uint32_t)) {
        uint32_t value = 0u;
        __builtin_memcpy(&value, encoded + at, sizeof value);
        __atomic_store_n(word(record + VblankRecord::fields_at + at), value, __ATOMIC_RELAXED);
    }
    __atomic_store_n(generation, before + 2u, __ATOMIC_RELEASE);
}

uint32_t ClockPageWindow::interrupt_every(uint32_t head) const
{
    AV_SCOPE("ClockPageWindow::interrupt_every");
    if (page_ == nullptr || head >= heads_) return 0u;
    const uint64_t record = ClockPageHeader(heads_).record_offset(head);
    return __atomic_load_n(word(record + VblankRecord::interrupt_every_at), __ATOMIC_RELAXED);
}

bool ClockPageWindow::invariants_hold() const
{
    AV_SCOPE("ClockPageWindow::invariants_hold");
    return page_ == nullptr ? heads_ == 0u : heads_ != 0u && heads_ <= ClockPageHeader::max_heads;
}

}  // namespace av

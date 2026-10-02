#include "miniport/display/clock_page_view.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

namespace av {

ClockPageView::ClockPageView(Platform &platform) : platform_(platform), page_(nullptr)
{
    AV_SCOPE("ClockPageView::ClockPageView");
}

ClockPageView::~ClockPageView()
{
    AV_SCOPE("ClockPageView::~ClockPageView");
    release();
}

Status ClockPageView::initialize()
{
    AV_SCOPE("ClockPageView::initialize");
    platform_.require_irql_at_most(Irql::passive, "ClockPageView::initialize");
    if (page_ != nullptr) return Status::invalid_state;
    page_ = static_cast<uint8_t *>(platform_.allocate_contiguous(ClockPageHeader::page_size));
    if (page_ == nullptr) return Status::no_memory;
    __builtin_memset(page_, 0, ClockPageHeader::page_size);
    return Status::ok;
}

void ClockPageView::release()
{
    AV_SCOPE("ClockPageView::release");
    platform_.require_irql_at_most(Irql::passive, "ClockPageView::release");
    if (page_ != nullptr) platform_.release_contiguous(page_, ClockPageHeader::page_size);
    page_ = nullptr;
}

uint64_t ClockPageView::address() const
{
    AV_SCOPE("ClockPageView::address");
    return page_ != nullptr ? platform_.physical_address(page_) : 0u;
}

uint32_t *ClockPageView::word(uint64_t offset) const
{
    AV_SCOPE("ClockPageView::word");
    return reinterpret_cast<uint32_t *>(page_ + offset);
}

uint32_t ClockPageView::heads() const
{
    AV_SCOPE("ClockPageView::heads");
    uint8_t header_bytes[ClockPageHeader::encoded_size] = {};
    for (uint64_t at = 0u; at < sizeof header_bytes; at += sizeof(uint32_t)) {
        const uint32_t value = __atomic_load_n(word(at), __ATOMIC_RELAXED);
        __builtin_memcpy(header_bytes + at, &value, sizeof value);
    }
    ClockPageHeader header;
    return header.decode(header_bytes, sizeof header_bytes) == Status::ok ? header.heads() : 0u;
}

/* The generation is read before the fields and again after them; only the same
 * even number both times means no store of the device's fell in between. */
Status ClockPageView::read(uint32_t head, VblankRecord &blank) const
{
    AV_SCOPE("ClockPageView::read");
    platform_.require_irql_at_most(Irql::dispatch, "ClockPageView::read");
    if (page_ == nullptr) return Status::invalid_state;
    const uint32_t described = heads();
    if (described == 0u) return Status::malformed;
    if (head >= described) return Status::not_found;
    const uint64_t record = ClockPageHeader(described).record_offset(head);
    for (uint32_t attempt = 0u; attempt < read_attempts; attempt++) {
        const uint32_t before = __atomic_load_n(word(record + VblankRecord::generation_at), __ATOMIC_ACQUIRE);
        uint8_t        fields[VblankRecord::encoded_size] = {};
        for (uint64_t at = 0u; at < sizeof fields; at += sizeof(uint32_t)) {
            const uint32_t value = __atomic_load_n(word(record + VblankRecord::fields_at + at), __ATOMIC_RELAXED);
            __builtin_memcpy(fields + at, &value, sizeof value);
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        const uint32_t after = __atomic_load_n(word(record + VblankRecord::generation_at), __ATOMIC_RELAXED);
        if (before != after || before % 2u != 0u) continue;
        return blank.decode(fields, sizeof fields);
    }
    return Status::conflict;
}

void ClockPageView::ask(uint32_t head, uint32_t every)
{
    AV_SCOPE("ClockPageView::ask");
    platform_.require_irql_at_most(Irql::dispatch, "ClockPageView::ask");
    if (page_ == nullptr) return;
    const uint32_t described = heads();
    if (head >= described) return;
    const uint64_t record = ClockPageHeader(described).record_offset(head);
    __atomic_store_n(word(record + VblankRecord::interrupt_every_at), every, __ATOMIC_RELAXED);
}

bool ClockPageView::invariants_hold() const
{
    AV_SCOPE("ClockPageView::invariants_hold");
    return true;
}

}  // namespace av

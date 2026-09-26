#include "device/memory/blob_dma_buf.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

BlobDmaBuf::BlobDmaBuf(Host &host) : host_(host), handle_(-1), length_(0u), items_(0u)
{
    AV_SCOPE("BlobDmaBuf::BlobDmaBuf");
}

BlobDmaBuf::~BlobDmaBuf()
{
    AV_SCOPE("BlobDmaBuf::~BlobDmaBuf");
    release();
}

Status BlobDmaBuf::make(const BlobWindow &pages, uint64_t offset, uint64_t size)
{
    AV_SCOPE("BlobDmaBuf::make");
    if (handle_ >= 0) return Status::invalid_state;
    if (size == 0u || size % page_size != 0u || offset % page_size != 0u) return Status::malformed;
    void    *runs[max_items];
    uint64_t sizes[max_items];
    uint32_t count = 0u;
    uint64_t at = 0u;      /* where this extent begins in the blob */
    uint64_t taken = 0u;
    for (uint32_t index = 0u; index < pages.extents() && taken < size; ++index) {
        uint64_t bytes = 0u;
        void *const base = pages.extent(index, bytes);
        /* An extent that is not whole pages cannot be an item, and one that
         * straddles the range's start would need a part of a page. */
        if (base == nullptr || bytes == 0u || bytes % page_size != 0u) return Status::malformed;
        const uint64_t end = at + bytes;
        if (end <= offset) { at = end; continue; }
        const uint64_t skip = offset > at ? offset - at : 0u;
        const uint64_t left = size - taken;
        const uint64_t wanted = bytes - skip < left ? bytes - skip : left;
        if (count == max_items) return Status::no_memory;
        runs[count] = static_cast<uint8_t *>(base) + skip;
        sizes[count] = wanted;
        ++count;
        taken += wanted;
        at = end;
    }
    if (taken != size) return Status::too_large;
    int          made = -1;
    const Status opened = host_.dmabuf_over_guest(runs, sizes, count, made);
    if (opened != Status::ok) return opened;
    handle_ = made;
    length_ = size;
    items_ = count;
    return Status::ok;
}

Status BlobDmaBuf::make_over_host(void *memory, uint64_t length, uint64_t offset, uint64_t size)
{
    AV_SCOPE("BlobDmaBuf::make_over_host");
    if (handle_ >= 0) return Status::invalid_state;
    if (size > length || offset > length - size) return Status::too_large;
    if (size == 0u || size % page_size != 0u || offset % page_size != 0u) return Status::malformed;
    int          made = -1;
    const Status opened = host_.dmabuf_over_host_memory(memory, offset, size, made);
    if (opened != Status::ok) return opened;
    handle_ = made;
    length_ = size;
    items_ = 1u;
    return Status::ok;
}

void BlobDmaBuf::release()
{
    AV_SCOPE("BlobDmaBuf::release");
    if (handle_ < 0) return;
    host_.close_dmabuf(handle_);
    handle_ = -1;
    length_ = 0u;
    items_ = 0u;
}

int BlobDmaBuf::take()
{
    AV_SCOPE("BlobDmaBuf::take");
    const int given = handle_;
    handle_ = -1;
    length_ = 0u;
    items_ = 0u;
    return given;
}

bool     BlobDmaBuf::made() const { AV_SCOPE("BlobDmaBuf::made"); return handle_ >= 0; }
int      BlobDmaBuf::handle() const { AV_SCOPE("BlobDmaBuf::handle"); return handle_; }
uint64_t BlobDmaBuf::length() const { AV_SCOPE("BlobDmaBuf::length"); return length_; }
uint32_t BlobDmaBuf::items() const { AV_SCOPE("BlobDmaBuf::items"); return items_; }

bool BlobDmaBuf::invariants_hold() const
{
    AV_SCOPE("BlobDmaBuf::invariants_hold");
    if (handle_ < 0) return length_ == 0u && items_ == 0u;
    return length_ != 0u && items_ != 0u && items_ <= max_items && length_ % page_size == 0u;
}

}  // namespace av

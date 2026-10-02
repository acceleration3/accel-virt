#include "guest/inspect/escape_reader.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/suite/adapter_search.hpp"

namespace av {

EscapeReader::EscapeReader(Kmt &kmt, BlockSource &memory)
    : kmt_(kmt), memory_(memory), bytes_(nullptr), length_(0u), clock_(0u), page_{}
{
    AV_SCOPE("EscapeReader::EscapeReader");
}

EscapeReader::~EscapeReader()
{
    AV_SCOPE("EscapeReader::~EscapeReader");
    drop();
}

Status EscapeReader::read(TraceEscapeRecord::Kind kind)
{
    AV_SCOPE("EscapeReader::read");
    drop();
    AdapterSearch     search(kmt_);
    if (search.find_avgpu(0u) != Status::ok) return Status::not_found;
    const Status status = read_from(search.adapter(), kind);
    if (status != Status::ok) drop();
    /* The dump is read; a failure to close the adapter changes nothing in it. */
    static_cast<void>(search.close());
    return status;
}

const uint8_t *EscapeReader::bytes() const
{
    AV_SCOPE("EscapeReader::bytes");
    return bytes_;
}

size_t EscapeReader::length() const
{
    AV_SCOPE("EscapeReader::length");
    return length_;
}

uint64_t EscapeReader::clock() const
{
    AV_SCOPE("EscapeReader::clock");
    return clock_;
}

bool EscapeReader::invariants_hold() const
{
    AV_SCOPE("EscapeReader::invariants_hold");
    if (length_ > max_total) return false;
    return bytes_ != nullptr || length_ == 0u;
}

Status EscapeReader::page(uint32_t adapter, TraceEscapeRecord::Kind kind, uint64_t offset, TraceEscapeRecord &answer)
{
    AV_SCOPE("EscapeReader::page");
    const TraceEscapeRecord request(kind, offset, window_bytes);
    static_cast<void>(request.encode(page_, sizeof page_));   /* the page holds a record by construction */
    const Status escaped = kmt_.escape(adapter, 0u, 0u, page_, sizeof page_);
    if (escaped != Status::ok) return escaped;
    answer = request;
    return answer.accept_answer(page_, sizeof page_);
}

Status EscapeReader::read_from(uint32_t adapter, TraceEscapeRecord::Kind kind)
{
    AV_SCOPE("EscapeReader::read_from");
    TraceEscapeRecord answer;
    Status            status = page(adapter, kind, 0u, answer);
    if (status != Status::ok) return status;
    const uint64_t total = answer.total();
    if (total > max_total) return Status::too_large;
    if (total != 0u) {
        bytes_ = static_cast<uint8_t *>(memory_.allocate(static_cast<size_t>(total), Pool::paged));
        if (bytes_ == nullptr) return Status::no_memory;
    }
    uint64_t offset = 0u;
    for (;;) {
        const uint64_t left = total - offset;
        const uint64_t expected = left < window_bytes ? left : window_bytes;
        if (answer.total() != total || answer.window() != expected) return Status::malformed;
        for (uint32_t byte = 0u; byte < answer.window(); byte++)
            bytes_[offset + byte] = page_[TraceEscapeRecord::encoded_size + byte];
        offset += answer.window();
        length_ = static_cast<size_t>(offset);
        clock_ = answer.clock();
        if (offset == total) return Status::ok;
        status = page(adapter, kind, offset, answer);
        if (status != Status::ok) return status;
    }
}

void EscapeReader::drop()
{
    AV_SCOPE("EscapeReader::drop");
    if (bytes_ != nullptr) memory_.release(bytes_);
    bytes_ = nullptr;
    length_ = 0u;
    clock_ = 0u;
}

}  // namespace av

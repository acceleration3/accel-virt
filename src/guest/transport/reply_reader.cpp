#include "guest/transport/reply_reader.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

ReplyReader::ReplyReader() : slot_(nullptr), capacity_(0u), required_(0u)
{
    AV_SCOPE("ReplyReader::ReplyReader");
}

ReplyReader::ReplyReader(const uint8_t *slot, size_t capacity)
    : slot_(slot), capacity_(slot == nullptr ? 0u : capacity), required_(0u)
    {
        AV_SCOPE("ReplyReader::ReplyReader");
    }

Status ReplyReader::read(uint64_t expected_fence, WireReader &answer)
{
    AV_SCOPE("ReplyReader::read");
    required_ = 0u;
    if (slot_ == nullptr) return Status::invalid_state;
    ReplyHeader  header;
    const Status decoded = header.decode(slot_, capacity_);
    if (decoded != Status::ok) return decoded;
    if (header.fence_id() != expected_fence) return Status::not_found;
    if (header.too_large()) {
        required_ = header.required();
        return Status::too_large;
    }
    if (header.size() > capacity_ - ReplyHeader::encoded_size) return Status::malformed;
    answer = WireReader(slot_ + ReplyHeader::encoded_size, header.size());
    required_ = header.size();
    return Status::ok;
}

uint32_t ReplyReader::required() const
{
    AV_SCOPE("ReplyReader::required");
    return required_;
}

bool ReplyReader::invariants_hold() const
{
    AV_SCOPE("ReplyReader::invariants_hold");
    return slot_ != nullptr || capacity_ == 0u;
}

}

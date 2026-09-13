#include "guest/transport/reply_reader.hpp"

#include "proto/wire/wire_reader.hpp"

namespace av {

ReplyReader::ReplyReader() : slot_(nullptr), capacity_(0u), required_(0u) {}

ReplyReader::ReplyReader(const uint8_t *slot, size_t capacity)
    : slot_(slot), capacity_(slot == nullptr ? 0u : capacity), required_(0u) {}

Status ReplyReader::read(uint64_t expected_fence, WireReader &answer)
{
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

uint32_t ReplyReader::required() const { return required_; }

bool ReplyReader::invariants_hold() const { return slot_ != nullptr || capacity_ == 0u; }

}  // namespace av

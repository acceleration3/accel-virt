#include "proto/submission/command_stream.hpp"

#include "proto/submission/command_header.hpp"

namespace av {

CommandStream::CommandStream(const uint8_t *bytes, size_t length)
    : bytes_(bytes), length_(bytes != nullptr ? length : 0u), offset_(0u), malformed_(false) {}

Status CommandStream::next(CommandHeader &header, const uint8_t *&payload)
{
    if (malformed_) return Status::malformed;
    const size_t left = length_ - offset_;
    if (left == 0u) return Status::no_space;
    CommandHeader decoded;
    if (decoded.decode(bytes_ + offset_, left) != Status::ok || decoded.size() > left) {
        malformed_ = true;
        return Status::malformed;
    }
    header = decoded;
    payload = bytes_ + offset_ + CommandHeader::encoded_size;
    offset_ += decoded.size();
    return Status::ok;
}

size_t CommandStream::offset() const
{
    return offset_;
}

bool CommandStream::invariants_hold() const
{
    return offset_ <= length_;
}

}  // namespace av

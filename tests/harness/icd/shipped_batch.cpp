#include "harness/icd/shipped_batch.hpp"

#include "harness/guest/mock_kmt.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_stream.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <cstdlib>

namespace av {

ShippedBatch::ShippedBatch(MockKmt &kmt, std::uint32_t ring)
    : kmt_(kmt), ring_(ring), bytes_(static_cast<std::uint8_t *>(std::calloc(max_bytes, 1u))), count_(0u),
      headers_(static_cast<CommandHeader *>(std::calloc(max_commands, sizeof(CommandHeader)))),
      offsets_(static_cast<std::uint32_t *>(std::calloc(max_commands, sizeof(std::uint32_t))))
{
}

ShippedBatch::~ShippedBatch()
{
    std::free(offsets_);
    std::free(headers_);
    std::free(bytes_);
}

Status ShippedBatch::read()
{
    count_ = 0u;
    SubmissionDescriptor submitted;
    RingHeader           ring;
    const std::uint8_t *const view = kmt_.ring_named(ring_);
    if (view == nullptr || submitted.decode(kmt_.last_submission(), kmt_.last_submission_size()) != Status::ok ||
        ring.decode(view, RingHeader::encoded_size) != Status::ok)
        return Status::malformed;
    const std::uint64_t mask = ring.capacity() - 1u;
    const std::uint8_t *const data = view + RingHeader::encoded_size;
    std::uint8_t framing[BatchHeader::encoded_size] = {};
    for (std::uint32_t i = 0u; i < BatchHeader::encoded_size; i++) framing[i] = data[(submitted.begin() + i) & mask];
    BatchHeader batch;
    if (batch.decode(framing, sizeof framing) != Status::ok || batch.length() > max_bytes) return Status::malformed;
    for (std::uint32_t i = 0u; i < batch.length(); i++)
        bytes_[i] = data[(submitted.begin() + BatchHeader::encoded_size + i) & mask];
    CommandStream       stream(bytes_, batch.length());
    const std::uint8_t *payload = nullptr;
    CommandHeader       command;
    Status              next = Status::ok;
    while (count_ < max_commands && (next = stream.next(command, payload)) == Status::ok) {
        headers_[count_] = command;
        offsets_[count_] = static_cast<std::uint32_t>(payload - bytes_);
        count_++;
    }
    return next == Status::no_space ? Status::ok : Status::malformed;
}

std::uint32_t        ShippedBatch::commands() const { return count_; }
const CommandHeader &ShippedBatch::header(std::uint32_t index) const { return headers_[index]; }
const std::uint8_t  *ShippedBatch::body(std::uint32_t index) const { return bytes_ + offsets_[index]; }

}  // namespace av

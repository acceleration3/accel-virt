#include "device/execution/batch_staging.hpp"

#include "core/support/scope_exit.hpp"
#include "device/host/host.hpp"

namespace av {

BatchStaging::BatchStaging(Host &host)
    : host_(host), payload_(nullptr), payload_capacity_(0u), reply_(nullptr), reply_capacity_(0u) {}

BatchStaging::~BatchStaging() { release(); }

Status BatchStaging::allocate(uint64_t payload_capacity, uint64_t reply_capacity)
{
    host_.require_main_loop("BatchStaging::allocate");
    if (payload_ != nullptr) return Status::invalid_state;
    const size_t reply_bytes = static_cast<size_t>(reply_capacity < max_reply ? reply_capacity : max_reply);
    payload_ = static_cast<uint8_t *>(host_.allocate(static_cast<size_t>(payload_capacity)));
    if (payload_ == nullptr) return Status::no_memory;
    ScopeExit release_payload([this] { release(); });
    reply_ = reply_bytes != 0u ? static_cast<uint8_t *>(host_.allocate(reply_bytes)) : nullptr;
    if (reply_bytes != 0u && reply_ == nullptr) return Status::no_memory;
    release_payload.dismiss();
    payload_capacity_ = static_cast<size_t>(payload_capacity);
    reply_capacity_ = reply_bytes;
    return Status::ok;
}

void BatchStaging::release()
{
    host_.require_main_loop("BatchStaging::release");
    if (payload_ != nullptr) host_.release(payload_);
    if (reply_ != nullptr) host_.release(reply_);
    payload_ = nullptr;
    reply_ = nullptr;
    payload_capacity_ = 0u;
    reply_capacity_ = 0u;
}

uint8_t *BatchStaging::payload() const          { return payload_; }
size_t   BatchStaging::payload_capacity() const { return payload_capacity_; }
uint8_t *BatchStaging::reply() const            { return reply_; }
size_t   BatchStaging::reply_capacity() const   { return reply_capacity_; }

/* A capacity is only ever claimed for a block that is held. */
bool BatchStaging::invariants_hold() const
{
    return (payload_ != nullptr || payload_capacity_ == 0u) && (reply_ != nullptr || reply_capacity_ == 0u);
}

}  // namespace av

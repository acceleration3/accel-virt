#include "device/execution/batch_staging.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

BatchStaging::BatchStaging(Host &host)
    : host_(host), payload_(nullptr), payload_capacity_(0u), reply_(nullptr), reply_capacity_(0u)
    {
        AV_SCOPE("BatchStaging::BatchStaging");
    }

BatchStaging::~BatchStaging()
{
    AV_SCOPE("BatchStaging::~BatchStaging");
    release();
}

Status BatchStaging::allocate(uint64_t payload_capacity, uint64_t reply_capacity)
{
    AV_SCOPE("BatchStaging::allocate");
    host_.require_main_loop("BatchStaging::allocate");
    if (payload_ != nullptr) return Status::invalid_state;
    const size_t reply_bytes = static_cast<size_t>(reply_capacity < max_reply ? reply_capacity : max_reply);
    payload_ = static_cast<uint8_t *>(host_.allocate(static_cast<size_t>(payload_capacity), Pool::paged));
    if (payload_ == nullptr) return Status::no_memory;
    ScopeExit release_payload([this] { release(); });
    reply_ = reply_bytes != 0u ? static_cast<uint8_t *>(host_.allocate(reply_bytes, Pool::paged)) : nullptr;
    if (reply_bytes != 0u && reply_ == nullptr) return Status::no_memory;
    release_payload.dismiss();
    payload_capacity_ = static_cast<size_t>(payload_capacity);
    reply_capacity_ = reply_bytes;
    return Status::ok;
}

void BatchStaging::release()
{
    AV_SCOPE("BatchStaging::release");
    host_.require_main_loop("BatchStaging::release");
    if (payload_ != nullptr) host_.release(payload_);
    if (reply_ != nullptr) host_.release(reply_);
    payload_ = nullptr;
    reply_ = nullptr;
    payload_capacity_ = 0u;
    reply_capacity_ = 0u;
}

uint8_t *BatchStaging::payload() const
{
    AV_SCOPE("BatchStaging::payload");
    return payload_;
}
size_t   BatchStaging::payload_capacity() const
{
    AV_SCOPE("BatchStaging::payload_capacity");
    return payload_capacity_;
}
uint8_t *BatchStaging::reply() const
{
    AV_SCOPE("BatchStaging::reply");
    return reply_;
}
size_t   BatchStaging::reply_capacity() const
{
    AV_SCOPE("BatchStaging::reply_capacity");
    return reply_capacity_;
}

/* A capacity is only ever claimed for a block that is held. */
bool BatchStaging::invariants_hold() const
{
    AV_SCOPE("BatchStaging::invariants_hold");
    return (payload_ != nullptr || payload_capacity_ == 0u) && (reply_ != nullptr || reply_capacity_ == 0u);
}

}  // namespace av

#include "device/context/d3d_ordering.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "device/context/ring_consumer.hpp"
#include "device/threading/worker.hpp"

#include <time.h>

namespace av {

namespace {

constexpr long     pause_nanoseconds = 20000;
constexpr uint32_t most_pauses = 1000;
constexpr uint32_t most_settling_pauses = 100;

}

D3dOrdering::D3dOrdering(Host &host) : host_(host), mutex_(nullptr), members_{}, count_(0u)
{
    AV_SCOPE("D3dOrdering::D3dOrdering");
}

D3dOrdering::~D3dOrdering()
{
    AV_SCOPE("D3dOrdering::~D3dOrdering");
    if (mutex_ != nullptr) host_.destroy_mutex(mutex_);
}

Status D3dOrdering::initialize()
{
    AV_SCOPE("D3dOrdering::initialize");
    return mutex_ != nullptr ? Status::ok : host_.create_mutex(mutex_);
}

void D3dOrdering::add(RingConsumer &ring, Worker &worker)
{
    AV_SCOPE("D3dOrdering::add");
    if (mutex_ == nullptr) return;
    host_.lock_mutex(mutex_);
    if (count_ < capacity) members_[count_++] = Member{&ring, &worker};
    host_.unlock_mutex(mutex_);
}

void D3dOrdering::remove(RingConsumer &ring)
{
    AV_SCOPE("D3dOrdering::remove");
    if (mutex_ == nullptr) return;
    host_.lock_mutex(mutex_);
    for (uint32_t index = 0u; index < count_; index++) {
        if (members_[index].ring_ != &ring) continue;
        members_[index] = members_[--count_];
        members_[count_] = Member{nullptr, nullptr};
        break;
    }
    host_.unlock_mutex(mutex_);
}

bool D3dOrdering::wake_earlier(const RingConsumer *ring, uint64_t published_at)
{
    AV_SCOPE("D3dOrdering::wake_earlier");
    bool pending = false;
    for (uint32_t index = 0u; index < count_; index++) {
        const Member &member = members_[index];
        if (member.ring_ == ring || member.ring_->earliest_pending() >= published_at) continue;
        member.worker_->wake();
        pending = true;
    }
    return pending;
}

void D3dOrdering::pause() const
{
    AV_SCOPE("D3dOrdering::pause");
    const timespec wait = {0, pause_nanoseconds};
    nanosleep(&wait, nullptr);
}

bool D3dOrdering::must_wait(const RingConsumer *ring, uint64_t published_at)
{
    AV_SCOPE("D3dOrdering::must_wait");
    host_.lock_mutex(mutex_);
    const bool pending = wake_earlier(ring, published_at);
    host_.unlock_mutex(mutex_);
    return pending;
}

void D3dOrdering::wait_before(const RingConsumer &ring, uint64_t published_at)
{
    AV_SCOPE("D3dOrdering::wait_before");
    if (mutex_ == nullptr || published_at == 0u) return;
    for (uint32_t pauses = 0u; pauses < most_pauses && must_wait(&ring, published_at); pauses++) pause();
}

void D3dOrdering::settle()
{
    AV_SCOPE("D3dOrdering::settle");
    if (mutex_ == nullptr) return;
    for (uint32_t pauses = 0u; pauses < most_settling_pauses && must_wait(nullptr, UINT64_MAX); pauses++) pause();
}

}

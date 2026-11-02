#include "guest/transport/host_watch.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/transport_ring.hpp"
#include "proto/submission/fence_page.hpp"

namespace av {

namespace {
constexpr uint32_t flags_at = 4u;
constexpr uint32_t completed_at = 8u;
constexpr uint32_t progress_at = 16u;
constexpr uint32_t fault_code_at = 24u;
constexpr uint32_t faulted_flag = 1u;
}

HostWatch::HostWatch(Kmt &kmt, const TransportRing &ring)
    : kmt_(kmt), ring_(ring), page_(nullptr), head_(0u), completed_(0u), progress_(0u), beat_(0u), last_moved_(0u),
      now_(0u), timing_(false)
{
    AV_SCOPE("HostWatch::HostWatch");
}

void HostWatch::read_page(const uint8_t *page)
{
    AV_SCOPE("HostWatch::read_page");
    page_ = page;
}

bool HostWatch::reads_page() const
{
    AV_SCOPE("HostWatch::reads_page");
    return page_ != nullptr;
}

uint64_t HostWatch::word(uint32_t offset) const
{
    AV_SCOPE("HostWatch::word");
    if (page_ == nullptr) return 0u;
    return __atomic_load_n(reinterpret_cast<const uint64_t *>(page_ + offset), __ATOMIC_ACQUIRE);
}

uint64_t HostWatch::completed() const
{
    AV_SCOPE("HostWatch::completed");
    return word(completed_at);
}

bool HostWatch::faulted() const
{
    AV_SCOPE("HostWatch::faulted");
    if (page_ == nullptr) return false;
    return (__atomic_load_n(reinterpret_cast<const uint32_t *>(page_ + flags_at), __ATOMIC_ACQUIRE) & faulted_flag) !=
           0u;
}

uint32_t HostWatch::fault_code() const
{
    AV_SCOPE("HostWatch::fault_code");
    if (!faulted()) return 0u;
    return __atomic_load_n(reinterpret_cast<const uint32_t *>(page_ + fault_code_at), __ATOMIC_ACQUIRE);
}

void HostWatch::start()
{
    AV_SCOPE("HostWatch::start");
    static_cast<void>(moved());
    timing_ = false;
}

bool HostWatch::moved()
{
    AV_SCOPE("HostWatch::moved");
    const uint64_t head = ring_.head();
    const uint64_t completed = word(completed_at);
    const uint64_t progress = word(progress_at);
    const uint64_t beat = word(FencePage::beat_at);
    const bool     moved = head != head_ || completed != completed_ || progress != progress_ || beat != beat_;
    head_ = head;
    completed_ = completed;
    progress_ = progress;
    beat_ = beat;
    return moved;
}

bool HostWatch::keeps_waiting()
{
    AV_SCOPE("HostWatch::keeps_waiting");
    if (faulted()) return false;
    uint64_t frequency = 0u;
    now_ = kmt_.performance_counter(frequency);
    if (!timing_) {
        timing_ = true;
        last_moved_ = now_;
    }
    if (moved()) last_moved_ = now_;
    return now_ - last_moved_ < frequency * silence_seconds;
}

bool HostWatch::invariants_hold() const
{
    AV_SCOPE("HostWatch::invariants_hold");
    return last_moved_ <= now_;
}

}

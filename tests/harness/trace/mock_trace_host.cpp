#include "harness/trace/mock_trace_host.hpp"

namespace av {

MockTraceHost::MockTraceHost()
    : key_(0x1000u), number_(100u), now_(1u), level_(0u), exchanges_(0u), fail_at_(0u), fail_count_(0u), failing_(false),
      owner_(pthread_self()), hook_(nullptr), hook_context_(nullptr)
{}

void MockTraceHost::run_as(std::uint64_t key, std::uint32_t number)
{
    key_ = key;
    number_ = number;
}

void MockTraceHost::set_clock(std::uint64_t now) { now_ = now; }
void MockTraceHost::set_level(std::uint16_t level) { level_ = level; }
void MockTraceHost::fail_exchange_at(std::uint32_t nth, std::uint32_t count)
{
    fail_at_ = exchanges_ + nth;
    fail_count_ = count;
}
void MockTraceHost::fail_every_exchange(bool failing) { failing_ = failing; }

void MockTraceHost::on_next_level(Hook hook, void *context)
{
    hook_ = hook;
    hook_context_ = context;
}

std::uint32_t MockTraceHost::exchanges() const { return exchanges_; }
std::uint64_t MockTraceHost::thread_key() const
{
    return pthread_equal(pthread_self(), owner_) != 0 ? key_ : 0u;
}
std::uint32_t MockTraceHost::thread_number() const { return number_; }
std::uint64_t MockTraceHost::clock() const { return now_; }

std::uint16_t MockTraceHost::level() const
{
    const Hook hook = hook_;
    hook_ = nullptr;
    if (hook != nullptr) hook(hook_context_);
    return level_;
}

bool MockTraceHost::exchange(std::uint64_t &word, std::uint64_t expected, std::uint64_t desired)
{
    exchanges_++;
    const bool scripted = exchanges_ >= fail_at_ && exchanges_ < fail_at_ + fail_count_;
    if (failing_ || scripted || word != expected) return false;
    word = desired;
    return true;
}

}

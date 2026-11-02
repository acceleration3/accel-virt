#ifndef AV_MOCK_TRACE_HOST_HPP
#define AV_MOCK_TRACE_HOST_HPP

#include "core/trace/trace_host.hpp"

#include <cstdint>

#include <pthread.h>

namespace av {

class MockTraceHost final : public TraceHost {
public:
    using Hook = void (*)(void *context);

    MockTraceHost();

    void run_as(std::uint64_t key, std::uint32_t number);
    void set_clock(std::uint64_t now);
    void set_level(std::uint16_t level);
    void fail_exchange_at(std::uint32_t nth, std::uint32_t count = 1u);
    void fail_every_exchange(bool failing);
    void on_next_level(Hook hook, void *context);
    std::uint32_t exchanges() const;

    std::uint64_t thread_key() const override;
    std::uint32_t thread_number() const override;
    std::uint64_t clock() const override;
    std::uint16_t level() const override;
    bool          exchange(std::uint64_t &word, std::uint64_t expected, std::uint64_t desired) override;

private:
    std::uint64_t key_;
    std::uint32_t number_;
    std::uint64_t now_;
    std::uint16_t level_;
    std::uint32_t exchanges_;
    std::uint32_t fail_at_;
    std::uint32_t fail_count_;
    bool          failing_;
    pthread_t     owner_;
    mutable Hook  hook_;
    mutable void *hook_context_;
};

}

#endif

#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/completion_probe.hpp"
#include "icd/common/host_completion_wait.hpp"

#include <cstdint>

using av::MockKmt;
using av::icd::CompletionProbe;
using av::icd::HostCompletionWait;

namespace {

constexpr std::uint64_t ms = 1000000u;
constexpr std::uint64_t ticks_per_half_second = MockKmt::ticks_per_second / 2u;

class CountingProbe final : public CompletionProbe {
public:
    explicit CountingProbe(std::uint32_t answers_at) : answers_at_(answers_at) {}

    bool ask(std::uint64_t slice) override
    {
        slices_[asks_ < capacity ? asks_ : capacity - 1u] = slice;
        asks_++;
        return answers_at_ != 0u && asks_ == answers_at_;
    }
    void pause(std::uint64_t slice) override
    {
        paused_with_ = slice;
        pauses_++;
    }

    std::uint32_t asks() const { return asks_; }
    std::uint32_t pauses() const { return pauses_; }
    std::uint64_t slice(std::uint32_t index) const { return slices_[index]; }
    std::uint64_t paused_with() const { return paused_with_; }

private:
    static constexpr std::uint32_t capacity = 16u;

    std::uint32_t answers_at_;
    std::uint32_t asks_ = 0u;
    std::uint32_t pauses_ = 0u;
    std::uint64_t paused_with_ = 0u;
    std::uint64_t slices_[capacity] = {};
};

}

TEST(host_completion_wait, a_probe_that_completes_ends_the_wait_after_pausing_only_between_asks)
{ MockKmt kmt;
  kmt.reads_clock(nullptr, ticks_per_half_second);
  HostCompletionWait wait(kmt, 10000u * ms);
  CountingProbe      probe(3u);
  CHECK(wait.run(probe));
  CHECK_EQ(probe.asks(), 3u);
  CHECK_EQ(probe.pauses(), 2u);
  CHECK_EQ(probe.slice(0u), 500u * ms); }

TEST(host_completion_wait, a_probe_that_never_completes_is_asked_in_slices_until_the_timeout)
{ MockKmt kmt;
  kmt.reads_clock(nullptr, ticks_per_half_second);
  HostCompletionWait wait(kmt, 1200u * ms);
  CountingProbe      probe(0u);
  CHECK(!wait.run(probe));
  CHECK_EQ(probe.asks(), 2u);
  CHECK_EQ(probe.pauses(), 2u);
  CHECK_EQ(probe.slice(0u), 500u * ms);
  CHECK_EQ(probe.slice(1u), 200u * ms);
  CHECK_EQ(probe.paused_with(), 200u * ms); }

TEST(host_completion_wait, a_zero_timeout_still_asks_once)
{ MockKmt kmt;
  kmt.reads_clock(nullptr, ticks_per_half_second);
  HostCompletionWait wait(kmt, 0u);
  CountingProbe      probe(0u);
  CHECK(!wait.run(probe));
  CHECK_EQ(probe.asks(), 1u);
  CHECK_EQ(probe.slice(0u), 0u);
  CountingProbe done(1u);
  HostCompletionWait again(kmt, 0u);
  CHECK(again.run(done));
  CHECK_EQ(done.pauses(), 0u); }

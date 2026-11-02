#ifndef AV_SUBMIT_BENCHMARK_HPP
#define AV_SUBMIT_BENCHMARK_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class SubmitBenchmark {
public:
    static constexpr uint32_t rounds = 32u;
    static constexpr uint32_t block = 8u;
    static constexpr uint32_t trips = 2000u;

    SubmitBenchmark(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    SubmitBenchmark(const SubmitBenchmark &) = delete;
    SubmitBenchmark &operator=(const SubmitBenchmark &) = delete;

    void run();
    void round_trips();

    uint32_t doorbell_ns() const;
    uint32_t kernel_ns() const;
    uint32_t ratio() const;

private:
    [[nodiscard]] Status time_block(uint64_t &elapsed_ticks);
    [[nodiscard]] Status time_path(uint32_t &per_submit_ns);

    Kmt              &kmt_;
    GuestReport      &report_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
    uint32_t          doorbell_ns_;
    uint32_t          kernel_ns_;
    bool              measured_;
    uint32_t          trip_ns_[trips];
};

}

#endif

#include "bench_timer.hpp"

#include "device/abi/av_executor.h"
#include "executor/executor_root.hpp"
#include "harness/executor/mock_host_apis.hpp"
#include "harness/executor/root_under_test.hpp"

#include <sched.h>

#include <cstdint>

namespace {

const std::uint8_t gpu[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

}

int main()
{
    av::MockHostApis apis;
    apis.add_gpu(gpu, AV_EXECUTOR_SUB_API_VULKAN);
    av::RootUnderTest root;
    if (root->initialize(apis) != av::Status::ok) return 1;
    const std::uint32_t domain = root.domain_of(gpu);
    AvContextOptions options = {};
    options.domain = domain;
    const av::BenchTimer timer(5u);
    constexpr std::uint64_t cycles = 200u;
    const double each = timer.nanoseconds_per_operation(cycles, [&] {
        for (std::uint64_t i = 0u; i < cycles; i++) {
            av::ContextSession *const made = root->open(AV_EXECUTOR_SUB_API_VULKAN, options, nullptr);
            if (made == nullptr) return;
            root->close(made);
            while (root->live() != 0u) sched_yield();
        }
    });
    timer.report("session open, close and slot back", cycles, each);
    return 0;
}

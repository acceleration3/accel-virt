#ifndef AV_OPENCL_SCENARIO_HPP
#define AV_OPENCL_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class OpenClScenario {
public:
    static constexpr uint32_t elements = 64u;

    OpenClScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    OpenClScenario(const OpenClScenario &) = delete;
    OpenClScenario &operator=(const OpenClScenario &) = delete;

    void run();

private:
    using Release = cl_int (*)(ClientTransport &, uint64_t);

    static constexpr uint32_t max_platforms = 16u;
    static constexpr uint32_t input_count = 2u;
    static constexpr uint32_t buffer_count = 3u;
    static constexpr uint32_t output_index = 2u;

    [[nodiscard]] Status find_device();
    [[nodiscard]] Status make_queue();
    [[nodiscard]] Status make_buffers();
    [[nodiscard]] Status make_kernel();
    [[nodiscard]] Status compute();
    void                 release();
    void                 release_object(Release call, uint64_t &object);

    ScenarioSteps     steps_;
    ScenarioTransport link_;
    uint64_t          device_;
    uint64_t          context_;
    uint64_t          queue_;
    uint64_t          buffers_[buffer_count];
    uint64_t          program_;
    uint64_t          kernel_;
    cl_int            release_error_;
    float             inputs_[input_count][elements];
    float             output_[elements];
};

}

#endif

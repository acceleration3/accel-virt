/* Vector add on the host GPU from a guest process (feature 044, US1): the first
 * platform with a GPU device, a context and a queue on it, two input buffers
 * copied from guest memory and a write-only output, a program built from
 * source, its kernel run over every element, and the output read back inline
 * and compared exactly with what the inputs add to - each call encoded by the
 * generated client over one OpenCL context's transport. Every object made is
 * released in reverse whatever failed. */
#ifndef AV_OPENCL_SCENARIO_HPP
#define AV_OPENCL_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "proto/client/client_transport.hpp"
/* cl_int, with the OpenCL version the generated client targets. */
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
    /* The generated client's release calls, which all take one object. */
    using Release = cl_int (*)(ClientTransport &, uint64_t);

    /* This host has three platforms; the cap only bounds the search. */
    static constexpr uint32_t max_platforms = 16u;
    static constexpr uint32_t input_count = 2u;
    /* Two inputs, then the output, in the order the kernel takes them. */
    static constexpr uint32_t buffer_count = 3u;
    static constexpr uint32_t output_index = 2u;

    [[nodiscard]] Status find_device();
    [[nodiscard]] Status make_queue();
    [[nodiscard]] Status make_buffers();
    [[nodiscard]] Status make_kernel();
    [[nodiscard]] Status compute();
    /* Keeps the first error a release answered in release_error_. */
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

}  // namespace av

#endif  // AV_OPENCL_SCENARIO_HPP

#ifndef AV_CL_PROBE_HPP
#define AV_CL_PROBE_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class ClEntry;
class GuestReport;
class Kmt;

class ClProbe {
public:
    static constexpr uint32_t elements = 64u;

    ClProbe(Kmt &kmt, GuestReport &report, ClEntry &cl);
    ClProbe(const ClProbe &) = delete;
    ClProbe &operator=(const ClProbe &) = delete;

    void run();

    void callback_arrived(int32_t status);

private:
    [[nodiscard]] Status scenarios();

    [[nodiscard]] Status platform_step();
    [[nodiscard]] Status device_step();
    void                 luid_step();
    [[nodiscard]] Status context_step();
    [[nodiscard]] Status queue_step();
    [[nodiscard]] Status program_step();
    [[nodiscard]] Status kernel_step();
    [[nodiscard]] Status read_step();
    void                 map_step();
    void                 svm_step();
    void                 callback_step();
    void                 long_program_step();
    void                 binary_step();
    void                 link_step();
    void                 il_step();
    uint32_t             wrong_from(void *kernel, int32_t &ran);
    void                 keep(void *program, void *kernel);
    uint32_t             callbacks_seen() const;
    void                 release_all();

    static constexpr uint32_t callback_seconds = 5u;
    static constexpr uint32_t long_strings = 400u;
    static constexpr uint32_t most_kept = 8u;

    ScenarioSteps steps_;
    GuestReport  &report_;
    Kmt          &kmt_;
    ClEntry      &cl_;
    void         *platform_;
    void         *device_;
    void         *context_;
    void         *queue_;
    void         *program_;
    void         *kernel_;
    void         *long_program_;
    void         *long_kernel_;
    void         *buffers_[3];
    void         *kept_programs_[most_kept];
    void         *kept_kernels_[most_kept];
    uint32_t      programs_kept_;
    uint32_t      kernels_kept_;
    float         first_[elements];
    float         second_[elements];
    float         sum_[elements];
    volatile uint32_t callbacks_;
    volatile int32_t  callback_status_;
};

}

#endif

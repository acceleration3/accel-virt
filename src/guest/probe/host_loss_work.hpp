#ifndef AV_HOST_LOSS_WORK_HPP
#define AV_HOST_LOSS_WORK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class D3d11SharedEntry;
class HostLossRequest;
class Kmt;
class ScenarioSteps;
struct HostLossWorkProbe;

class HostLossWork {
public:
    static constexpr uint32_t removal_ms = 10000u;
    static constexpr uint32_t width = 16u;
    static constexpr uint32_t height = 16u;
    static constexpr uint8_t  before_colour[4] = {20u, 140u, 220u, 255u};
    static constexpr uint8_t  after_colour[4] = {230u, 60u, 20u, 255u};

    HostLossWork(D3d11SharedEntry &before, D3d11SharedEntry &after, HostLossRequest &request, Kmt &kmt);
    ~HostLossWork();
    HostLossWork(const HostLossWork &) = delete;
    HostLossWork &operator=(const HostLossWork &) = delete;

    void run(ScenarioSteps &steps);

    bool invariants_hold() const;

private:
    int32_t  draw(D3d11SharedEntry &entry, const uint8_t *colour);
    [[nodiscard]] Status await_removal(uint32_t &waited_ms);
    uint32_t elapsed_ms(uint64_t started) const;

    D3d11SharedEntry &before_;
    D3d11SharedEntry &after_;
    HostLossRequest  &request_;
    Kmt              &kmt_;
    bool              made_before_;
    bool              made_after_;

    friend struct HostLossWorkProbe;
};

}

#endif

#include "um/probe/d3d/d3d_probe_program.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/d3d11_work.hpp"
#include "guest/probe/d3d12_work.hpp"
#include "guest/probe/d3d9_work.hpp"
#include "guest/probe/deferred_map_work.hpp"
#include "guest/probe/host_loss_work.hpp"
#include "guest/probe/shared_clear_work.hpp"
#include "guest/probe/tir_coverage_work.hpp"
#include "guest/probe/wpf_present_work.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <windows.h>

namespace av {

namespace {
void write_report(const GuestReport &report)
{
    AV_SCOPE("write_report");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report.text(),
                                static_cast<DWORD>(report.length()), &written, nullptr));
}

}

D3d9ProbeProgram::D3d9ProbeProgram()
    : kmt_(), heap_(), report_(heap_, "avd3d9probe"), images_(), window_(), windows_(), d3d9_(), d3d9ex_()
{
    AV_SCOPE("D3d9ProbeProgram::D3d9ProbeProgram");
}

uint32_t D3d9ProbeProgram::run()
{
    AV_SCOPE("D3d9ProbeProgram::run");
    ScenarioSteps steps(kmt_, report_, "d3d9");
    if (window_.open()) {
        {
            D3d9Work work(d3d9_, images_);
            work.run(steps, window_.window());
        }
        WpfPresentWork wpf(d3d9ex_, windows_, images_);
        wpf.run(steps, window_.window());
    } else {
        report_.check("d3d9", "window", Status::not_found, window_.last_error());
    }
    report_.finish();
    write_report(report_);
    return report_.failed();
}

D3d11ProbeProgram::D3d11ProbeProgram()
    : kmt_(), heap_(), report_(heap_, "avd3d11probe"), images_(), window_(), d3d11_(), deferred_(), shared_(), tir_(), lost_(),
      remade_(), loss_(kmt_)
{
    AV_SCOPE("D3d11ProbeProgram::D3d11ProbeProgram");
}

uint32_t D3d11ProbeProgram::run()
{
    AV_SCOPE("D3d11ProbeProgram::run");
    ScenarioSteps steps(kmt_, report_, "d3d11");
    if (window_.open()) {
        {
            D3d11Work work(d3d11_, images_);
            work.run(steps, window_.window());
        }
        {
            DeferredMapWork deferred(deferred_);
            deferred.run(steps, window_.window());
        }
        {
            SharedClearWork shared(shared_);
            shared.run(steps);
        }
        {
            TirCoverageWork tir(tir_);
            tir.run(steps);
        }
        if (kmt_.initialize() == Status::ok) {
            HostLossWork loss(lost_, remade_, loss_, kmt_);
            loss.run(steps);
        } else {
            report_.check("d3d11", "loss_kmt", Status::no_memory, 0u);
        }
    } else {
        report_.check("d3d11", "window", Status::not_found, window_.last_error());
    }
    report_.finish();
    write_report(report_);
    return report_.failed();
}

D3d12ProbeProgram::D3d12ProbeProgram() : kmt_(), heap_(), report_(heap_, "avd3d12probe"), images_(), d3d12_()
{
    AV_SCOPE("D3d12ProbeProgram::D3d12ProbeProgram");
}

uint32_t D3d12ProbeProgram::run()
{
    AV_SCOPE("D3d12ProbeProgram::run");
    ScenarioSteps steps(kmt_, report_, "d3d12");
    D3d12Work     work(d3d12_, images_);
    work.run(steps);
    report_.finish();
    write_report(report_);
    return report_.failed();
}

}

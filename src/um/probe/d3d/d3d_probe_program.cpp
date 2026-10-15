#include "um/probe/d3d/d3d_probe_program.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/d3d11_work.hpp"
#include "guest/probe/d3d12_work.hpp"
#include "guest/probe/d3d9_work.hpp"
#include "guest/probe/wpf_present_work.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <windows.h>

namespace av {

namespace {

/* A report the console does not take is still the exit code. */
void write_report(const GuestReport &report)
{
    AV_SCOPE("write_report");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report.text(),
                                static_cast<DWORD>(report.length()), &written, nullptr));
}

}  // namespace

D3d9ProbeProgram::D3d9ProbeProgram()
    : kmt_(), report_("avd3d9probe"), images_(), window_(), windows_(), d3d9_(), d3d9ex_()
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

D3d11ProbeProgram::D3d11ProbeProgram() : kmt_(), report_("avd3d11probe"), images_(), window_(), d3d11_()
{
    AV_SCOPE("D3d11ProbeProgram::D3d11ProbeProgram");
}

uint32_t D3d11ProbeProgram::run()
{
    AV_SCOPE("D3d11ProbeProgram::run");
    ScenarioSteps steps(kmt_, report_, "d3d11");
    if (window_.open()) {
        D3d11Work work(d3d11_, images_);
        work.run(steps, window_.window());
    } else {
        report_.check("d3d11", "window", Status::not_found, window_.last_error());
    }
    report_.finish();
    write_report(report_);
    return report_.failed();
}

D3d12ProbeProgram::D3d12ProbeProgram() : kmt_(), report_("avd3d12probe"), images_(), d3d12_()
{
    AV_SCOPE("D3d12ProbeProgram::D3d12ProbeProgram");
}

/* No window: a D3D12 device is made on an adapter, and what this probe clears
 * is a render target of its own. */
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

}  // namespace av

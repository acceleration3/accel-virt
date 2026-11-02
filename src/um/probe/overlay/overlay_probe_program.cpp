#include "um/probe/overlay/overlay_probe_program.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/overlay_work.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <windows.h>

namespace av {

OverlayProbeProgram::OverlayProbeProgram(uint32_t hold) : kmt_(), heap_(), report_(heap_, "avoverlayprobe"), dxgi_(), hold_(hold)
{
    AV_SCOPE("OverlayProbeProgram::OverlayProbeProgram");
}

uint32_t OverlayProbeProgram::run()
{
    AV_SCOPE("OverlayProbeProgram::run");
    ScenarioSteps steps(kmt_, report_, "overlay");
    { OverlayWork work(dxgi_, hold_);
      work.run(steps); }
    report_.finish();
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(), static_cast<DWORD>(report_.length()),
                                &written, nullptr));
    return report_.failed();
}

}

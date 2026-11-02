#include "um/common/run_once.hpp"

#include "core/trace/function_scope.hpp"
#include "um/common/once_body.hpp"

#include <windows.h>

namespace av {
extern "C" BOOL WINAPI av_run_once(PINIT_ONCE, PVOID body, PVOID *)
{
    static_cast<OnceBody *>(body)->run_once();
    return TRUE;
}

bool RunOnce::run(OnceBody &body)
{
    AV_SCOPE("RunOnce::run");
    return InitOnceExecuteOnce(reinterpret_cast<PINIT_ONCE>(&once_), &av_run_once, &body, nullptr) != FALSE;
}

}

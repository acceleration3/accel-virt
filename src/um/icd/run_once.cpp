#include "um/icd/run_once.hpp"

#include "core/trace/function_scope.hpp"
#include "um/icd/once_body.hpp"

#include <windows.h>

namespace av {

/* The one-shot's callback. An ABI boundary C requires: InitOnceExecuteOnce
 * takes a bare function and one argument, so the body is the argument and
 * this forwards straight into it. */
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

}  // namespace av

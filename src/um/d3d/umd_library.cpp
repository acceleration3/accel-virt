#include "um/d3d/umd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/umd_binding.hpp"

namespace av {

UmdLibrary::Seams::Seams()
    : heap(), lock(), log(L"avumd"), kmt(), driver(heap, kmt, log),
      root(heap, kmt, lock, log, d3d::umd_binding, &driver), trace()
{
    AV_SCOPE("UmdLibrary::Seams::Seams");
    log.open();
}

d3d::UmdRoot *UmdLibrary::root()
{
    AV_SCOPE("UmdLibrary::root");
    if (!seams_.constructed() && !refused_ && seams_.construct() == Status::ok &&
        seams_.object().kmt.initialize() != Status::ok) {
        seams_.object().log.write("avumd: no D3DKMT wait event, refusing the runtime");
        seams_.destroy();
        refused_ = true;
        return nullptr;
    }
    if (seams_.constructed() && !started_trace_) {
        started_trace_ = true;
        Seams &seams = seams_.object();
        const Status traced = seams.trace.start("avumd", seams.root, crashed_);
        if (traced != Status::ok) seams.log.write("avumd: tracing is off: its section, pin or service failed");
    }
    return seams_.constructed() ? &seams_.object().root : nullptr;
}

d3d::Hresult UmdLibrary::open_adapter10_2(void *args)
{
    AV_SCOPE("UmdLibrary::open_adapter10_2");
    d3d::UmdRoot *const answering = root();
    if (answering == nullptr) return d3d::hr_fail;
    return answering->open_adapter10_2(static_cast<d3d::D3d10ddiargOpenAdapterRecord *>(args));
}

d3d::Hresult UmdLibrary::open_adapter12(void *args)
{
    AV_SCOPE("UmdLibrary::open_adapter12");
    d3d::UmdRoot *const answering = root();
    if (answering == nullptr) return d3d::hr_fail;
    return answering->open_adapter12(static_cast<d3d::D3d12ddiargOpenAdapterRecord *>(args));
}

long UmdLibrary::crashed(void *pointers)
{
    AV_SCOPE("UmdLibrary::crashed");
    return seams_.constructed() ? seams_.object().trace.crashed(pointers) : 0;
}

}  // namespace av

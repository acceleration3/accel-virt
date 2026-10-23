#include "um/d3d/umd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3dddiarg_open_adapter_record.hpp"
#include "icd/d3d/umd_binding.hpp"

#include <windows.h>

namespace av {

UmdLibrary::Seams::Seams()
    : process(L"avumd", "avumd: "), lock(), caller(), driver(process.heap(), process.kmt(), process.log()),
      root(process.heap(), process.kmt(), lock, process.log(), d3d::umd_binding, &driver)
{
    AV_SCOPE("UmdLibrary::Seams::Seams");
}

d3d::Hresult UmdLibrary::open_adapter10_2(void *args)
{
    AV_SCOPE("UmdLibrary::open_adapter10_2");
    d3d::UmdRoot *const answering = driver_.root();
    if (answering == nullptr) return d3d::hr_fail;
    return answering->open_adapter10_2(static_cast<d3d::D3d10ddiargOpenAdapterRecord *>(args));
}

d3d::Hresult UmdLibrary::open_adapter12(void *args)
{
    AV_SCOPE("UmdLibrary::open_adapter12");
    d3d::UmdRoot *const answering = driver_.root();
    if (answering == nullptr) return d3d::hr_fail;
    return answering->open_adapter12(static_cast<d3d::D3d12ddiargOpenAdapterRecord *>(args));
}

d3d::Hresult UmdLibrary::open_adapter9(void *args)
{
    AV_SCOPE("UmdLibrary::open_adapter9");
    d3d::UmdRoot *const answering = driver_.root();
    if (answering == nullptr) return d3d::hr_fail;
    return answering->open_adapter9(static_cast<d3d::D3dddiargOpenAdapterRecord *>(args));
}

void UmdLibrary::ask_shared_rows()
{
    AV_SCOPE("UmdLibrary::ask_shared_rows");
    Seams *const seams = driver_.seams();
    if (seams != nullptr) static_cast<void>(seams->driver.shared_rows().ask(seams->caller.thread_id()));
}

void UmdLibrary::withdraw_shared_rows()
{
    AV_SCOPE("UmdLibrary::withdraw_shared_rows");
    Seams *const seams = driver_.seams();
    if (seams != nullptr) seams->driver.shared_rows().withdraw(seams->caller.thread_id());
}

long UmdLibrary::crashed(void *pointers)
{
    AV_SCOPE("UmdLibrary::crashed");
    return driver_.crashed(pointers);
}

}  // namespace av

#include "um/opengl/dxgi_adapter_order.hpp"

#include "core/trace/function_scope.hpp"
#include "um/common/com_reference.hpp"

#include <windows.h>
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#include <d3dkmthk.h>
#include <dxgi.h>

namespace av {

namespace {

uint64_t key_of(const LUID &luid)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(luid.HighPart)) << 32u) | luid.LowPart;
}

}

uint64_t DxgiAdapterOrder::listed_first()
{
    AV_SCOPE("DxgiAdapterOrder::listed_first");
    using CreateFactory = HRESULT(WINAPI *)(REFIID, void **);
    const HMODULE dxgi = LoadLibraryA("dxgi.dll");
    const FARPROC create = dxgi != nullptr ? GetProcAddress(dxgi, "CreateDXGIFactory1") : nullptr;
    if (create == nullptr) return 0u;
    ComReference<IDXGIFactory1> factory;
    if (FAILED(reinterpret_cast<CreateFactory>(create)(__uuidof(IDXGIFactory1),
                                                       reinterpret_cast<void **>(factory.receive())))) return 0u;
    ComReference<IDXGIAdapter1> adapter;
    if (factory->EnumAdapters1(0u, adapter.receive()) != S_OK) return 0u;
    DXGI_ADAPTER_DESC1 described = {};
    return SUCCEEDED(adapter->GetDesc1(&described)) ? key_of(described.AdapterLuid) : 0u;
}

uint64_t DxgiAdapterOrder::drawn_on(void *dc)
{
    AV_SCOPE("DxgiAdapterOrder::drawn_on");
    if (dc == nullptr) return 0u;
    D3DKMT_OPENADAPTERFROMHDC open = {};
    open.hDc = static_cast<HDC>(dc);
    if (D3DKMTOpenAdapterFromHdc(&open) != 0) return 0u;
    D3DKMT_CLOSEADAPTER close = {};
    close.hAdapter = open.hAdapter;
    static_cast<void>(D3DKMTCloseAdapter(&close));
    return key_of(open.AdapterLuid);
}

}

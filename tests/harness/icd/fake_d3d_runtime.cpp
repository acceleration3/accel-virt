#include "harness/icd/fake_d3d_runtime.hpp"

#include "icd/d3d/ddi/d3d10ddiarg_calc_private_device_size_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/umd_root.hpp"

#include <cstring>

namespace av {
namespace {

using d3d::Hresult;

using SupportedVersions = Hresult (*)(void *, std::uint32_t *, std::uint64_t *);
using GetCaps = Hresult (*)(void *, d3d::D3d102ddiargGetCapsRecord *);
using CalcSize = std::size_t (*)(void *, const d3d::D3d10ddiargCalcPrivateDeviceSizeRecord *);
using CreateDevice10 = Hresult (*)(void *, d3d::D3d10ddiargCreateDeviceRecord *);
using CreateDevice12 = Hresult (*)(void *, const void *);
using CloseAdapter = Hresult (*)(void *);
using DestroyDevice = void (*)(void *);
using OptionalTables = Hresult (*)(void *, std::uint32_t *, d3d::D3d12ddiTableRequestRecord *);
using FillTable = Hresult (*)(void *, std::uint32_t, void *, std::size_t, std::uint32_t, void *);
using DxgiSlot = Hresult (*)(void *);

/* The eight slots of the D3D12 table in the order the header declares them, so
 * the check below can name the six D3D12Core requires by index. */
void *const *slots_of(const d3d::D3d12ddiAdapterFuncsRecord &table)
{
    return reinterpret_cast<void *const *>(&table);
}

}  // namespace

FakeD3dRuntime::FakeD3dRuntime(d3d::UmdRoot &root, d3d::RuntimeKind runtime)
    : root_(root), runtime_(runtime), callbacks_{}, funcs10_{}, funcs12_{}, dxgi_callbacks_{}, dxgi_table_{},
      adapter_(nullptr), interface_(runtime == d3d::RuntimeKind::d3d10_11 ? d3d11_interface : 0u),
      no_callbacks_(false), no_table_(false), no_dxgi_callbacks_(false), no_dxgi_table_(false), scribble_(false),
      block_{}, runtime_device_(0), runtime_core_layer_(0), kt_callbacks_(0), um_callbacks_(0)
{
    /* Non-null rows, as a real runtime's table has: the driver keeps the
     * pointer to the table, and a test that moved a row would see it. */
    callbacks_.pfnQueryAdapterInfoCb = &runtime_device_;
    callbacks_.pfnGetMultisampleMethodListCb = &runtime_core_layer_;
    callbacks_.pfnQueryAdapterInfoCb2 = &kt_callbacks_;
    dxgi_callbacks_.pfnPresentCb = &um_callbacks_;
}

void FakeD3dRuntime::offer_no_callbacks() { no_callbacks_ = true; }
void FakeD3dRuntime::offer_no_table() { no_table_ = true; }
void FakeD3dRuntime::ask_for_interface(std::uint32_t interface_version) { interface_ = interface_version; }
void FakeD3dRuntime::offer_no_dxgi_callbacks() { no_dxgi_callbacks_ = true; }
void FakeD3dRuntime::offer_no_dxgi_table() { no_dxgi_table_ = true; }
void FakeD3dRuntime::scribble_the_device_block() { scribble_ = true; }

Hresult FakeD3dRuntime::open()
{
    /* Both runtimes zero the table before the call, so a slot the driver does
     * not write reads back null (research.md §12). */
    std::memset(&funcs10_, 0, sizeof funcs10_);
    std::memset(&funcs12_, 0, sizeof funcs12_);
    if (runtime_ == d3d::RuntimeKind::d3d10_11) {
        d3d::D3d10ddiargOpenAdapterRecord args = {};
        args.hRTAdapter = &runtime_device_;
        args.Interface = interface_;
        args.Version = d3d11_runtime_build;
        args.pAdapterCallbacks = no_callbacks_ ? nullptr : &callbacks_;
        args.pAdapterFuncs = no_table_ ? nullptr : &funcs10_;
        const Hresult opened = root_.open_adapter10_2(&args);
        adapter_ = opened == d3d::hr_ok ? args.hAdapter : nullptr;
        return opened;
    }
    d3d::D3d12ddiargOpenAdapterRecord args = {};
    args.hRTAdapter = &runtime_device_;
    args.pAdapterCallbacks = no_callbacks_ ? nullptr : &callbacks_;
    args.pAdapterFuncs = no_table_ ? nullptr : &funcs12_;
    const Hresult opened = root_.open_adapter12(&args);
    adapter_ = opened == d3d::hr_ok ? args.hAdapter : nullptr;
    return opened;
}

bool FakeD3dRuntime::table_is_whole() const
{
    if (adapter_ == nullptr) return false;
    if (runtime_ == d3d::RuntimeKind::d3d10_11)
        return funcs10_.pfnCalcPrivateDeviceSize != nullptr && funcs10_.pfnCreateDevice != nullptr &&
               funcs10_.pfnCloseAdapter != nullptr && funcs10_.pfnGetSupportedVersions != nullptr &&
               funcs10_.pfnGetCaps != nullptr;
    /* Slots two through seven, which is what D3D12Core checks. */
    void *const *const slots = slots_of(funcs12_);
    for (std::uint32_t slot = 2u; slot < 8u; slot++)
        if (slots[slot] == nullptr) return false;
    return true;
}

void *FakeD3dRuntime::adapter() const { return adapter_; }

Hresult FakeD3dRuntime::supported_versions(std::uint32_t *entries, std::uint64_t *words)
{
    void *const slot =
        runtime_ == d3d::RuntimeKind::d3d10_11 ? funcs10_.pfnGetSupportedVersions : funcs12_.pfnGetSupportedVersions;
    return reinterpret_cast<SupportedVersions>(slot)(adapter_, entries, words);
}

Hresult FakeD3dRuntime::caps(std::uint32_t type, void *data, std::uint32_t bytes)
{
    d3d::D3d102ddiargGetCapsRecord args = {};
    args.Type = type;
    args.pData = data;
    args.DataSize = bytes;
    void *const slot = runtime_ == d3d::RuntimeKind::d3d10_11 ? funcs10_.pfnGetCaps : funcs12_.pfnGetCaps;
    return reinterpret_cast<GetCaps>(slot)(adapter_, &args);
}

std::size_t FakeD3dRuntime::calc_private_device_size()
{
    d3d::D3d10ddiargCalcPrivateDeviceSizeRecord args = {};
    args.Interface = interface_;
    args.Version = d3d11_runtime_build;
    void *const slot =
        runtime_ == d3d::RuntimeKind::d3d10_11 ? funcs10_.pfnCalcPrivateDeviceSize : funcs12_.pfnCalcPrivateDeviceSize;
    return reinterpret_cast<CalcSize>(slot)(adapter_, &args);
}

Hresult FakeD3dRuntime::create_device()
{
    std::memset(block_, scribble_ ? 0x5A : 0, sizeof block_);
    std::memset(&dxgi_table_, 0, sizeof dxgi_table_);
    if (runtime_ == d3d::RuntimeKind::d3d12)
        return reinterpret_cast<CreateDevice12>(funcs12_.pfnCreateDevice)(adapter_, &runtime_device_);
    d3d::D3d10ddiargCreateDeviceRecord create = {};
    create.hRTDevice = &runtime_device_;
    create.Interface = interface_;
    create.Version = d3d11_runtime_build;
    create.pKTCallbacks = &kt_callbacks_;
    create.hDrvDevice = block_;
    create.DXGIBaseDDI.pDXGIBaseCallbacks = no_dxgi_callbacks_ ? nullptr : &dxgi_callbacks_;
    create.DXGIBaseDDI.pDXGIDDIBaseFunctions = no_dxgi_table_ ? nullptr : &dxgi_table_;
    create.hRTCoreLayer = &runtime_core_layer_;
    create.pUMCallbacks = &um_callbacks_;
    return reinterpret_cast<CreateDevice10>(funcs10_.pfnCreateDevice)(adapter_, &create);
}

Hresult FakeD3dRuntime::close_adapter()
{
    void *const slot = runtime_ == d3d::RuntimeKind::d3d10_11 ? funcs10_.pfnCloseAdapter : funcs12_.pfnCloseAdapter;
    const Hresult closed = reinterpret_cast<CloseAdapter>(slot)(adapter_);
    if (closed == d3d::hr_ok) adapter_ = nullptr;
    return closed;
}

Hresult FakeD3dRuntime::optional_ddi_tables(std::uint32_t *entries, d3d::D3d12ddiTableRequestRecord *requests)
{
    if (runtime_ != d3d::RuntimeKind::d3d12) return d3d::hr_invalid_arg;
    return reinterpret_cast<OptionalTables>(funcs12_.pfnGetOptionalDDITables)(adapter_, entries, requests);
}

Hresult FakeD3dRuntime::fill_ddi_table(std::uint32_t table_type)
{
    if (runtime_ != d3d::RuntimeKind::d3d12) return d3d::hr_invalid_arg;
    unsigned char destination[64] = {};
    return reinterpret_cast<FillTable>(funcs12_.pfnFillDDITable)(adapter_, table_type, destination,
                                                                 sizeof destination, 0u, nullptr);
}

void FakeD3dRuntime::destroy_device()
{
    if (runtime_ != d3d::RuntimeKind::d3d12) return;
    reinterpret_cast<DestroyDevice>(funcs12_.pfnDestroyDevice)(block_);
}

void *FakeD3dRuntime::device_block() { return block_; }

d3d::Dxgi161DdiBaseFunctionsRecord &FakeD3dRuntime::dxgi_table() { return dxgi_table_; }

const d3d::DxgiDdiBaseCallbacksRecord &FakeD3dRuntime::dxgi_callbacks() const { return dxgi_callbacks_; }

Hresult FakeD3dRuntime::dxgi_slot(std::uint32_t index, void *args)
{
    void *const *const slots = reinterpret_cast<void *const *>(&dxgi_table_);
    return reinterpret_cast<DxgiSlot>(slots[index])(args);
}

}  // namespace av

#include "harness/icd/fake_d3d_runtime.hpp"

#include "icd/d3d/ddi/d3d10ddiarg_calc_private_device_size_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3dddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/umd_root.hpp"
#include "proto/escape/umd_adapter_info.hpp"

#include <cstring>

namespace av {
namespace {

using d3d::Hresult;

using SupportedVersions = Hresult (*)(void *, std::uint32_t *, std::uint64_t *);
using GetCaps = Hresult (*)(void *, d3d::D3d102ddiargGetCapsRecord *);
using CalcSize = std::size_t (*)(void *, const d3d::D3d10ddiargCalcPrivateDeviceSizeRecord *);
using CreateDevice10 = Hresult (*)(void *, d3d::D3d10ddiargCreateDeviceRecord *);
using CreateDevice12 = Hresult (*)(void *, d3d::D3d12ddiargCreateDeviceRecord *);
using CreateDevice9 = Hresult (*)(void *, void *);
using CloseAdapter = Hresult (*)(void *);
using DestroyDevice = void (*)(void *);
using OptionalTables = Hresult (*)(void *, std::uint32_t *, d3d::D3d12ddiTableRequestRecord *);
using FillTable = Hresult (*)(void *, std::uint32_t, void *, std::size_t, std::uint32_t, void *);
/* The eight slots of the D3D12 table in the order the header declares them, so
 * the check below can name the six D3D12Core requires by index. */
void *const *slots_of(const d3d::D3d12ddiAdapterFuncsRecord &table)
{
    return reinterpret_cast<void *const *>(&table);
}

/* The runtime's QueryAdapterInfo callback, which the driver asks for the LUID
 * the kernel driver answers: this fake has no kernel driver, so it refuses,
 * and the driver takes the first avgpu adapter. The driver calls it through
 * a pointer to its own function-local argument struct, which no type here can
 * name, so -fsanitize=function would see a mismatch that the ABI does not. */
__attribute__((no_sanitize("function"))) std::int32_t refuse_adapter_info(void *, void *) { return -1; }

/* UMDRIVERPRIVATE as avgpu.sys writes it for a caller with room for two
 * words: the tag, then the adapter's LUID. */
struct AdapterInfoQuery {
    void         *private_data;
    std::uint32_t private_bytes;
};

__attribute__((no_sanitize("function"))) std::int32_t answer_adapter_info(void *adapter, void *query)
{
    auto *const  asked = static_cast<AdapterInfoQuery *>(query);
    std::size_t written = 0u;
    return UmdAdapterInfo(*static_cast<const std::uint64_t *>(adapter), 0u)
                       .encode(static_cast<std::uint8_t *>(asked->private_data), asked->private_bytes, written) ==
                   Status::ok
               ? 0
               : -1;
}

}  // namespace

FakeD3dRuntime::FakeD3dRuntime(d3d::UmdRoot &root, d3d::RuntimeKind runtime)
    : root_(root), runtime_(runtime), callbacks_{}, funcs9_{}, version9_(d3d9_newest_version), driver_version_(0u),
      device9_(nullptr), funcs10_{}, funcs12_{}, dxgi_callbacks_{}, dxgi_table_{},
      adapter_(nullptr),
      interface_(runtime == d3d::RuntimeKind::d3d10_11 ? d3d11_interface
                 : runtime == d3d::RuntimeKind::d3d9   ? d3d9_interface
                                                       : 0u),
      no_callbacks_(false), no_table_(false), no_dxgi_callbacks_(false), no_dxgi_table_(false), scribble_(false),
      block_{}, table_{}, runtime_device_(0), luid_(0u), runtime_core_layer_(0), kt_callbacks_(0), um_callbacks_(0)
{
    /* Non-null rows, as a real runtime's table has: the driver keeps the
     * pointer to the table, and a test that moved a row would see it. */
    callbacks_.pfnQueryAdapterInfoCb = reinterpret_cast<void *>(&refuse_adapter_info);
    callbacks_.pfnGetMultisampleMethodListCb = &runtime_core_layer_;
    callbacks_.pfnQueryAdapterInfoCb2 = &kt_callbacks_;
    dxgi_callbacks_.pfnPresentCb = &um_callbacks_;
}

void FakeD3dRuntime::offer_no_callbacks() { no_callbacks_ = true; }
void FakeD3dRuntime::offer_no_table() { no_table_ = true; }
void FakeD3dRuntime::ask_for_interface(std::uint32_t interface_version) { interface_ = interface_version; }
void FakeD3dRuntime::ask_for_version(std::uint32_t version) { version9_ = version; }
void FakeD3dRuntime::offer_no_dxgi_callbacks() { no_dxgi_callbacks_ = true; }
void FakeD3dRuntime::offer_no_dxgi_table() { no_dxgi_table_ = true; }
void FakeD3dRuntime::scribble_the_device_block() { scribble_ = true; }

void FakeD3dRuntime::answer_luid(std::uint64_t luid)
{
    luid_ = luid;
    callbacks_.pfnQueryAdapterInfoCb = reinterpret_cast<void *>(&answer_adapter_info);
}

Hresult FakeD3dRuntime::open()
{
    /* Both runtimes zero the table before the call, so a slot the driver does
     * not write reads back null (research.md §12). */
    std::memset(&funcs10_, 0, sizeof funcs10_);
    std::memset(&funcs12_, 0, sizeof funcs12_);
    std::memset(&funcs9_, 0, sizeof funcs9_);
    if (runtime_ == d3d::RuntimeKind::d3d9) {
        d3d::D3dddiargOpenAdapterRecord args = {};
        args.hAdapter = luid_ != 0u ? static_cast<void *>(&luid_) : &runtime_device_;
        args.Interface = interface_;
        args.Version = version9_;
        args.pAdapterCallbacks = no_callbacks_ ? nullptr : &callbacks_;
        args.pAdapterFuncs = no_table_ ? nullptr : &funcs9_;
        const Hresult opened = root_.open_adapter9(&args);
        adapter_ = opened == d3d::hr_ok ? args.hAdapter : nullptr;
        driver_version_ = opened == d3d::hr_ok ? args.DriverVersion : 0u;
        return opened;
    }
    if (runtime_ == d3d::RuntimeKind::d3d10_11) {
        d3d::D3d10ddiargOpenAdapterRecord args = {};
        args.hRTAdapter = luid_ != 0u ? static_cast<void *>(&luid_) : &runtime_device_;
        args.Interface = interface_;
        args.Version = d3d11_runtime_build;
        args.pAdapterCallbacks = no_callbacks_ ? nullptr : &callbacks_;
        args.pAdapterFuncs = no_table_ ? nullptr : &funcs10_;
        const Hresult opened = root_.open_adapter10_2(&args);
        adapter_ = opened == d3d::hr_ok ? args.hAdapter : nullptr;
        return opened;
    }
    d3d::D3d12ddiargOpenAdapterRecord args = {};
    args.hRTAdapter = luid_ != 0u ? static_cast<void *>(&luid_) : &runtime_device_;
    args.pAdapterCallbacks = no_callbacks_ ? nullptr : &callbacks_;
    args.pAdapterFuncs = no_table_ ? nullptr : &funcs12_;
    const Hresult opened = root_.open_adapter12(&args);
    adapter_ = opened == d3d::hr_ok ? args.hAdapter : nullptr;
    return opened;
}

bool FakeD3dRuntime::table_is_whole() const
{
    if (adapter_ == nullptr) return false;
    if (runtime_ == d3d::RuntimeKind::d3d9)
        return funcs9_.pfnGetCaps != nullptr && funcs9_.pfnCreateDevice != nullptr && funcs9_.pfnCloseAdapter != nullptr;
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
    void *const slot = runtime_ == d3d::RuntimeKind::d3d10_11 ? funcs10_.pfnGetCaps
                       : runtime_ == d3d::RuntimeKind::d3d9   ? funcs9_.pfnGetCaps
                                                              : funcs12_.pfnGetCaps;
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
    if (runtime_ == d3d::RuntimeKind::d3d9) {
        /* D3DDDIARG_CREATEDEVICE begins with hDevice: the runtime's handle in,
         * the driver's out. */
        void *create[16] = {};
        create[0] = &runtime_device_;
        const Hresult made = reinterpret_cast<CreateDevice9>(funcs9_.pfnCreateDevice)(adapter_, create);
        device9_ = made == d3d::hr_ok ? create[0] : nullptr;
        return made;
    }
    if (runtime_ == d3d::RuntimeKind::d3d12) {
        d3d::D3d12ddiargCreateDeviceRecord create = {};
        create.hRTDevice = &runtime_device_;
        create.pKTCallbacks = &kt_callbacks_;
        create.hDrvDevice = block_;
        create.p12UMCallbacks = &um_callbacks_;
        return reinterpret_cast<CreateDevice12>(funcs12_.pfnCreateDevice)(adapter_, &create);
    }
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
    void *const slot = runtime_ == d3d::RuntimeKind::d3d10_11 ? funcs10_.pfnCloseAdapter
                       : runtime_ == d3d::RuntimeKind::d3d9   ? funcs9_.pfnCloseAdapter
                                                              : funcs12_.pfnCloseAdapter;
    const Hresult closed = reinterpret_cast<CloseAdapter>(slot)(adapter_);
    if (closed == d3d::hr_ok) adapter_ = nullptr;
    return closed;
}

Hresult FakeD3dRuntime::optional_ddi_tables(std::uint32_t *entries, d3d::D3d12ddiTableRequestRecord *requests)
{
    if (runtime_ != d3d::RuntimeKind::d3d12) return d3d::hr_invalid_arg;
    return reinterpret_cast<OptionalTables>(funcs12_.pfnGetOptionalDDITables)(adapter_, entries, requests);
}

Hresult FakeD3dRuntime::fill_ddi_table(std::uint32_t table_type, std::size_t bytes)
{
    if (runtime_ != d3d::RuntimeKind::d3d12) return d3d::hr_invalid_arg;
    for (void *&slot : table_) slot = nullptr;
    return reinterpret_cast<FillTable>(funcs12_.pfnFillDDITable)(adapter_, table_type, table_, bytes, 0u, nullptr);
}

std::uint32_t FakeD3dRuntime::filled_slots() const
{
    std::uint32_t filled = 0u;
    for (void *const slot : table_) filled += slot != nullptr ? 1u : 0u;
    return filled;
}

void FakeD3dRuntime::destroy_device()
{
    if (runtime_ != d3d::RuntimeKind::d3d12) return;
    reinterpret_cast<DestroyDevice>(funcs12_.pfnDestroyDevice)(block_);
}

std::uint32_t FakeD3dRuntime::driver_version() const { return driver_version_; }
void         *FakeD3dRuntime::device9() const { return device9_; }

/* d3d9.dll destroys a device through its device table; the root's slot is
 * what that table's DestroyDevice thunk calls (tools/gen_d3d9_ddi.py). */
Hresult FakeD3dRuntime::destroy_device9()
{
    const Hresult destroyed = root_.destroy_device9(device9_);
    if (destroyed == d3d::hr_ok) device9_ = nullptr;
    return destroyed;
}

void *FakeD3dRuntime::device_block() { return block_; }

d3d::Dxgi161DdiBaseFunctionsRecord &FakeD3dRuntime::dxgi_table() { return dxgi_table_; }

const d3d::DxgiDdiBaseCallbacksRecord &FakeD3dRuntime::dxgi_callbacks() const { return dxgi_callbacks_; }

}  // namespace av

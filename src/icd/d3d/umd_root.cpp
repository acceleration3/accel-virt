#include "icd/d3d/umd_root.hpp"

#include "icd/common/thread_lock_hold.hpp"
#include "icd/d3d/ddi/d3dddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_callbacks_record.hpp"

#include "core/support/runtime_call.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/support/log_sink.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/d3d/ddi/d3d10_2ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_table_request_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/dxgi1_6_1_ddi_base_functions_record.hpp"
#include "icd/d3d/dxgi_entry_points.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "icd/d3d/umd_host_driver.hpp"
#include "icd/d3d/umd_entry_points.hpp"

namespace av::d3d {
namespace {


}  // namespace

UmdRoot::UmdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, LogSink &log, UmdBinding &binding,
                 UmdHostDriver *builder)
    : kmt_(kmt), lock_(lock), log_(log), lines_(log, "avumd: "), binding_(binding), builder_(builder), connections_(kmt),
      adapters_(memory), devices_(memory, log)
{
    AV_SCOPE("UmdRoot::UmdRoot");
    binding_.bind(*this);
}

UmdRoot::~UmdRoot()
{
    AV_SCOPE("UmdRoot::~UmdRoot");
    binding_.unbind();
}

/* The LUID avgpu.sys answers UMDRIVERPRIVATE with: which adapter the runtime
 * opened. 0 - an older driver, or a refusal - takes the first avgpu adapter. */
uint64_t UmdRoot::adapter_luid(void *runtime_adapter, const void *callbacks)
{
    AV_SCOPE("UmdRoot::adapter_luid");
    constexpr uint64_t umd_private_tag = 0x0044494C555641ull;
    struct QueryAdapterInfo {
        void    *pPrivateDriverData;
        uint32_t PrivateDriverDataSize;
    };
    using QueryAdapterInfoCb = int32_t(AV_RUNTIME_CALL *)(void *, QueryAdapterInfo *);
    const auto query = reinterpret_cast<QueryAdapterInfoCb>(
        static_cast<const D3dddiAdapterCallbacksRecord *>(callbacks)->pfnQueryAdapterInfoCb);
    uint64_t         words[2] = {};
    QueryAdapterInfo info = {words, sizeof(words)};
    if (query == nullptr || query(runtime_adapter, &info) < 0 || words[0] != umd_private_tag) return 0u;
    lines_.note("adapter luid", words[1]);
    return words[1];
}

Hresult UmdRoot::open(UmdAdapter *&opened, RuntimeKind runtime, void *runtime_adapter, const void *callbacks,
                      uint32_t interface_version, uint32_t runtime_version)
{
    AV_SCOPE("UmdRoot::open");
    opened = nullptr;
    if (callbacks == nullptr) return hr_invalid_arg;
    const DdiVersionList versions(runtime);
    if (runtime == RuntimeKind::d3d10_11 && !versions.opens(interface_version)) {
        lines_.note("open refused, interface", interface_version);
        return hr_no_interface;
    }
    UmdConnection *connection = nullptr;
    const Status   held = connections_.acquire(adapter_luid(runtime_adapter, callbacks), connection);
    if (held != Status::ok) {
        lines_.note("no avgpu adapter, status", static_cast<uint64_t>(held));
        return hr_fail;
    }
    bool      keep = false;
    ScopeExit give_back([this, &keep, connection] {
        if (keep) return;
        const Status released = connection->release();
        if (released != Status::ok) lines_.note("connection release, status", static_cast<uint64_t>(released));
    });
    UmdAdapter  *adapter = nullptr;
    const Status made = adapters_.open(adapter, runtime, runtime_adapter,
                                       static_cast<const D3dddiAdapterCallbacksRecord *>(callbacks), *connection,
                                       interface_version, runtime_version);
    if (made != Status::ok) {
        lines_.note("no memory for the adapter, status", static_cast<uint64_t>(made));
        return hr_out_of_memory;
    }
    const Status learned = adapter->identity().learn(kmt_, connection->adapter(), log_);
    if (learned != Status::ok) {
        /* Discarded rather than closed: the adapter is the one this function
         * made a line ago, so there is no handle to be wrong and no device to
         * have been made on it - nothing for a close to refuse, and so no
         * refusal to judge. */
        adapters_.discard(*adapter);
        return hr_fail;
    }
    keep = true;
    opened = adapter;
    return hr_ok;
}

Hresult UmdRoot::open_adapter10_2(D3d10ddiargOpenAdapterRecord *args)
{
    AV_SCOPE("UmdRoot::open_adapter10_2");
    if (args == nullptr || args->pAdapterFuncs == nullptr) return hr_invalid_arg;
    const icd::ThreadLockHold release(lock_);
    UmdAdapter   *adapter = nullptr;
    const Hresult opened = open(adapter, RuntimeKind::d3d10_11, args->hRTAdapter, args->pAdapterCallbacks,
                                args->Interface, args->Version);
    if (opened != hr_ok) return opened;
    fill_d3d10_adapter_funcs(*static_cast<D3d102ddiAdapterFuncsRecord *>(args->pAdapterFuncs));
    args->hAdapter = adapter;
    return hr_ok;
}

Hresult UmdRoot::open_adapter12(D3d12ddiargOpenAdapterRecord *args)
{
    AV_SCOPE("UmdRoot::open_adapter12");
    if (args == nullptr || args->pAdapterFuncs == nullptr) return hr_invalid_arg;
    const icd::ThreadLockHold release(lock_);
    UmdAdapter   *adapter = nullptr;
    const Hresult opened = open(adapter, RuntimeKind::d3d12, args->hRTAdapter, args->pAdapterCallbacks, 0u, 0u);
    if (opened != hr_ok) return opened;
    fill_d3d12_adapter_funcs(*static_cast<D3d12ddiAdapterFuncsRecord *>(args->pAdapterFuncs));
    args->hAdapter = adapter;
    return hr_ok;
}

Hresult UmdRoot::open_adapter9(D3dddiargOpenAdapterRecord *args)
{
    AV_SCOPE("UmdRoot::open_adapter9");
    constexpr uint32_t interface9 = 9u;
    constexpr uint32_t win7_interface = 0x2003u;   /* D3D_UMD_INTERFACE_VERSION_WIN7 */
    if (args == nullptr || args->pAdapterFuncs == nullptr) return hr_invalid_arg;
    if (args->Interface != interface9 || args->Version < win7_interface) {
        lines_.note("open9 refused, version", args->Version);
        return hr_no_interface;
    }
    const icd::ThreadLockHold release(lock_);
    UmdAdapter   *adapter = nullptr;
    const Hresult opened = open(adapter, RuntimeKind::d3d9, args->hAdapter, args->pAdapterCallbacks,
                                args->Interface, args->Version);
    if (opened != hr_ok) return opened;
    fill_d3d9_adapter_funcs(*static_cast<D3dddiAdapterFuncsRecord *>(args->pAdapterFuncs));
    args->hAdapter = adapter;
    args->DriverVersion = win7_interface;
    return hr_ok;
}

Hresult UmdRoot::create_device9(void *adapter, void *create)
{
    AV_SCOPE("UmdRoot::create_device9");
    const icd::ThreadLockHold release(lock_);
    UmdAdapter *const open_adapter = adapters_.adapter_of(adapter);
    if (open_adapter == nullptr || create == nullptr || open_adapter->runtime() != RuntimeKind::d3d9)
        return hr_invalid_arg;
    if (builder_ == nullptr) return hr_not_impl;
    const Hresult built = builder_->build9(*open_adapter, create, open_adapter->connection().adapter());
    if (built == hr_ok) open_adapter->device_made();
    return built;
}

Hresult UmdRoot::destroy_device9(void *device)
{
    AV_SCOPE("UmdRoot::destroy_device9");
    const icd::ThreadLockHold release(lock_);
    UmdAdapter *const made_on = builder_ != nullptr ? builder_->tear_down9(device) : nullptr;
    if (made_on == nullptr) return hr_invalid_arg;
    made_on->device_gone();
    return hr_ok;
}

Hresult UmdRoot::supported_versions(void *adapter, uint32_t *entries, uint64_t *words)
{
    AV_SCOPE("UmdRoot::supported_versions");
    const icd::ThreadLockHold release(lock_);
    const UmdAdapter *const open_adapter = adapters_.adapter_of(adapter);
    if (open_adapter == nullptr) return hr_invalid_arg;
    return open_adapter->versions().answer(entries, words);
}

Hresult UmdRoot::caps(void *adapter, D3d102ddiargGetCapsRecord *args)
{
    AV_SCOPE("UmdRoot::caps");
    const icd::ThreadLockHold release(lock_);
    UmdAdapter *const open_adapter = adapters_.adapter_of(adapter);
    if (open_adapter == nullptr || args == nullptr) return hr_invalid_arg;
    if (builder_ != nullptr) return builder_->caps(*open_adapter, *args, open_adapter->connection().adapter());
    /* No host to ask, so no capability is claimed (constitution VI). D3D12Core
     * survives this: it falls back and records generic pipeline level 1
     * (research.md §12). */
    lines_.note("cap refused, type", args->Type);
    return hr_invalid_arg;
}

size_t UmdRoot::private_device_bytes(void *adapter) const
{
    AV_SCOPE("UmdRoot::private_device_bytes");
    /* The cookie is a magic and a pointer whatever the device becomes, so this
     * does not depend on the adapter, the interface or the flags - but a
     * handle this root never handed out is answered zero, which makes the
     * runtime's create fail rather than hand over a block of the wrong size. */
    return adapters_.adapter_of(adapter) == nullptr ? 0u : DeviceTable::private_bytes;
}

Hresult UmdRoot::adopt_device(UmdAdapter &adapter, D3d10ddiargCreateDeviceRecord &create, UmdDevice *&made)
{
    AV_SCOPE("UmdRoot::adopt_device");
    made = nullptr;
    UmdDevice   *device = nullptr;
    const Status created = devices_.make(device, adapter, create.hDrvDevice);
    if (created != Status::ok) {
        lines_.note("no device, status", static_cast<uint64_t>(created));
        return created == Status::no_memory ? hr_out_of_memory : hr_invalid_arg;
    }
    const Hresult adopted = device->adopt(create);
    if (adopted != hr_ok) {
        forget_device(create.hDrvDevice);
        return adopted;
    }
    made = device;
    return hr_ok;
}

void UmdRoot::forget_device(void *block)
{
    AV_SCOPE("UmdRoot::forget_device");
    const Status destroyed = devices_.destroy(block);
    if (destroyed != Status::ok) lines_.note("device destroy, status", static_cast<uint64_t>(destroyed));
}

Hresult UmdRoot::create_device(void *adapter, D3d10ddiargCreateDeviceRecord *create)
{
    AV_SCOPE("UmdRoot::create_device");
    const icd::ThreadLockHold release(lock_);
    UmdAdapter *const open_adapter = adapters_.adapter_of(adapter);
    if (open_adapter == nullptr || create == nullptr) return hr_invalid_arg;
    UmdDevice    *device = nullptr;
    const Hresult adopted = adopt_device(*open_adapter, *create, device);
    if (adopted != hr_ok) return adopted;
    /* The runtime frees its own block and never calls pfnDestroyDevice for a
     * create that failed, so the object model built above is given back here
     * rather than leaked. */
    const Hresult built =
        builder_ == nullptr ? hr_not_impl : builder_->build(*device, *create, open_adapter->connection().adapter());
    if (built != hr_ok) forget_device(create->hDrvDevice);
    return built;
}

Hresult UmdRoot::create_device12(void *adapter, D3d12ddiargCreateDeviceRecord *create)
{
    AV_SCOPE("UmdRoot::create_device12");
    const icd::ThreadLockHold release(lock_);
    UmdAdapter *const open_adapter = adapters_.adapter_of(adapter);
    if (open_adapter == nullptr || create == nullptr || create->hDrvDevice == nullptr) return hr_invalid_arg;
    UmdDevice   *device = nullptr;
    const Status made = devices_.make(device, *open_adapter, create->hDrvDevice);
    if (made != Status::ok) {
        lines_.note("no device, status", static_cast<uint64_t>(made));
        return hr_out_of_memory;
    }
    /* As for Direct3D 11, a failed create is given back here: the runtime
     * never destroys a device it was told was not made. */
    const Hresult built =
        builder_ == nullptr ? hr_not_impl : builder_->build12(*device, *create, open_adapter->connection().adapter());
    if (built != hr_ok) forget_device(create->hDrvDevice);
    return built;
}

Hresult UmdRoot::close_adapter(void *adapter)
{
    AV_SCOPE("UmdRoot::close_adapter");
    const icd::ThreadLockHold release(lock_);
    /* Taken before the close, which frees the adapter that names it. */
    UmdAdapter *const    open_adapter = adapters_.adapter_of(adapter);
    UmdConnection *const connection = open_adapter != nullptr ? &open_adapter->connection() : nullptr;
    const Status         closed = adapters_.close(adapter);
    if (closed == Status::conflict) {
        lines_.note("adapter closed with live devices", open_adapter->devices());
        return hr_fail;
    }
    if (closed != Status::ok) return hr_invalid_arg;
    const Status released = connection->release();
    if (released != Status::ok) lines_.note("connection release, status", static_cast<uint64_t>(released));
    return hr_ok;
}

void UmdRoot::destroy_device(void *block)
{
    AV_SCOPE("UmdRoot::destroy_device");
    const icd::ThreadLockHold release(lock_);
    UmdDevice *const device = devices_.device_of(block);
    if (builder_ != nullptr && device != nullptr) builder_->tear_down(*device, block);
    forget_device(block);
}

Hresult UmdRoot::optional_ddi_tables(void *adapter, uint32_t *entries, D3d12ddiTableRequestRecord *requests)
{
    AV_SCOPE("UmdRoot::optional_ddi_tables");
    const icd::ThreadLockHold release(lock_);
    if (adapters_.adapter_of(adapter) == nullptr || entries == nullptr) return hr_invalid_arg;
    static_cast<void>(requests);
    /* None: 057 names the optional tables it fills. */
    *entries = 0u;
    return hr_ok;
}

Hresult UmdRoot::fill_ddi_table(void *adapter, uint32_t table_type, void *table, size_t bytes, uint32_t index,
                                void *runtime_table)
{
    AV_SCOPE("UmdRoot::fill_ddi_table");
    constexpr uint32_t dxgi_table = 3u;
    const icd::ThreadLockHold release(lock_);
    if (adapters_.adapter_of(adapter) == nullptr || table == nullptr) return hr_invalid_arg;
    if (table_type == dxgi_table) {
        if (bytes < sizeof(Dxgi161DdiBaseFunctionsRecord)) return hr_invalid_arg;
        fill_dxgi_base_functions(*static_cast<Dxgi161DdiBaseFunctionsRecord *>(table));
        return hr_ok;
    }
    const Hresult filled = builder_ == nullptr ? hr_not_impl : builder_->fill_table12(table_type, table, bytes, index, runtime_table);
    if (filled != hr_ok) lines_.note("ddi table refused, type", table_type);
    return filled;
}

UmdDevice *UmdRoot::device_of(void *block) const
{
    AV_SCOPE("UmdRoot::device_of");
    return devices_.device_of(block);
}

void UmdRoot::report(StateWriter &writer)
{
    AV_SCOPE("UmdRoot::report");
    writer.open("umd", 0u);
    if (!lock_.try_acquire()) {
        writer.flag("busy", true);
        writer.close();
        return;
    }
    ScopeExit release([this] { lock_.release(); });
    writer.number("adapters", adapters_.open_adapters());
    writer.number("devices", devices_.live_devices());
    connections_.report(writer);
    adapters_.report(writer);
    devices_.report(writer);
    writer.close();
}

bool UmdRoot::invariants_hold() const
{
    AV_SCOPE("UmdRoot::invariants_hold");
    if (!connections_.invariants_hold()) return false;
    if (!adapters_.invariants_hold() || !devices_.invariants_hold()) return false;
    /* A connection is held once per open adapter and by nothing else, so a
     * process with no adapter open holds no kernel adapter either. */
    if (connections_.opens() != adapters_.open_adapters()) return false;
    return (connections_.open() != 0u) == (adapters_.open_adapters() != 0u);
}

}  // namespace av::d3d

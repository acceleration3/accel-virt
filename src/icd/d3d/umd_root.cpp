#include "icd/d3d/umd_root.hpp"

#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/d3d/ddi/d3d10_2ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_open_adapter_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_table_request_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_open_adapter_record.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "icd/d3d/umd_entry_points.hpp"

namespace av::d3d {
namespace {

constexpr uint32_t line_bytes = 128u;

}  // namespace

UmdRoot::UmdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, icd::LogSink &log, UmdBinding &binding)
    : kmt_(kmt), lock_(lock), log_(log), binding_(binding), connection_(kmt),
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

void UmdRoot::note(const char *what, uint64_t value)
{
    AV_SCOPE("UmdRoot::note");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append("rvkumd: ");
    text.append(what);
    text.append(" ");
    text.append_decimal(value);
    log_.write(text.text());
}

Hresult UmdRoot::open(UmdAdapter *&opened, RuntimeKind runtime, void *runtime_adapter, const void *callbacks,
                      uint32_t interface_version, uint32_t runtime_version)
{
    AV_SCOPE("UmdRoot::open");
    opened = nullptr;
    if (callbacks == nullptr) return hr_invalid_arg;
    const DdiVersionList versions(runtime);
    if (runtime == RuntimeKind::d3d10_11 && !versions.serves(interface_version)) {
        note("open refused, interface", interface_version);
        return hr_no_interface;
    }
    const Status held = connection_.acquire();
    if (held != Status::ok) {
        note("no rvkgpu adapter, status", static_cast<uint64_t>(held));
        return hr_fail;
    }
    bool      keep = false;
    ScopeExit give_back([this, &keep] {
        if (keep) return;
        const Status released = connection_.release();
        if (released != Status::ok) note("connection release, status", static_cast<uint64_t>(released));
    });
    UmdAdapter  *adapter = nullptr;
    const Status made = adapters_.open(adapter, runtime, runtime_adapter,
                                       static_cast<const D3dddiAdapterCallbacksRecord *>(callbacks),
                                       interface_version, runtime_version);
    if (made != Status::ok) {
        note("no memory for the adapter, status", static_cast<uint64_t>(made));
        return hr_out_of_memory;
    }
    const Status learned = adapter->identity().learn(kmt_, connection_.adapter(), log_);
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
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
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
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    UmdAdapter   *adapter = nullptr;
    const Hresult opened = open(adapter, RuntimeKind::d3d12, args->hRTAdapter, args->pAdapterCallbacks, 0u, 0u);
    if (opened != hr_ok) return opened;
    fill_d3d12_adapter_funcs(*static_cast<D3d12ddiAdapterFuncsRecord *>(args->pAdapterFuncs));
    args->hAdapter = adapter;
    return hr_ok;
}

Hresult UmdRoot::supported_versions(void *adapter, uint32_t *entries, uint64_t *words)
{
    AV_SCOPE("UmdRoot::supported_versions");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    const UmdAdapter *const open_adapter = adapters_.adapter_of(adapter);
    if (open_adapter == nullptr) return hr_invalid_arg;
    return open_adapter->versions().answer(entries, words);
}

Hresult UmdRoot::caps(void *adapter, D3d102ddiargGetCapsRecord *args)
{
    AV_SCOPE("UmdRoot::caps");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    if (adapters_.adapter_of(adapter) == nullptr || args == nullptr) return hr_invalid_arg;
    /* No D3D capability is implemented and tested here, so none is claimed
     * (constitution VI). D3D12Core survives this: it falls back and records
     * generic pipeline level 1 (research.md §12). */
    note("cap refused, type", args->Type);
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
        note("no device, status", static_cast<uint64_t>(created));
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
    if (destroyed != Status::ok) note("device destroy, status", static_cast<uint64_t>(destroyed));
}

Hresult UmdRoot::create_device(void *adapter, D3d10ddiargCreateDeviceRecord *create)
{
    AV_SCOPE("UmdRoot::create_device");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    UmdAdapter *const open_adapter = adapters_.adapter_of(adapter);
    if (open_adapter == nullptr || create == nullptr) return hr_invalid_arg;
    UmdDevice    *device = nullptr;
    const Hresult adopted = adopt_device(*open_adapter, *create, device);
    if (adopted != hr_ok) return adopted;
    /* Feature 056 fills the device-functions table here and answers hr_ok. The
     * runtime frees its own block and never calls pfnDestroyDevice for a
     * create that failed, so the object model built above is given back here
     * rather than leaked. */
    forget_device(create->hDrvDevice);
    return hr_not_impl;
}

Hresult UmdRoot::create_device12(void *adapter, const void *create)
{
    AV_SCOPE("UmdRoot::create_device12");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    if (adapters_.adapter_of(adapter) == nullptr || create == nullptr) return hr_invalid_arg;
    /* Feature 057 mirrors D3D12DDIARG_CREATEDEVICE and builds its device over
     * DeviceTable, as the D3D11 create above does. */
    return hr_not_impl;
}

Hresult UmdRoot::close_adapter(void *adapter)
{
    AV_SCOPE("UmdRoot::close_adapter");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    const Status closed = adapters_.close(adapter);
    if (closed == Status::conflict) {
        note("adapter closed with live devices", adapters_.adapter_of(adapter)->devices());
        return hr_fail;
    }
    if (closed != Status::ok) return hr_invalid_arg;
    const Status released = connection_.release();
    if (released != Status::ok) note("connection release, status", static_cast<uint64_t>(released));
    return hr_ok;
}

void UmdRoot::destroy_device(void *block)
{
    AV_SCOPE("UmdRoot::destroy_device");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    forget_device(block);
}

Hresult UmdRoot::optional_ddi_tables(void *adapter, uint32_t *entries, D3d12ddiTableRequestRecord *requests)
{
    AV_SCOPE("UmdRoot::optional_ddi_tables");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    if (adapters_.adapter_of(adapter) == nullptr || entries == nullptr) return hr_invalid_arg;
    static_cast<void>(requests);
    /* None: 057 names the optional tables it fills. */
    *entries = 0u;
    return hr_ok;
}

Hresult UmdRoot::fill_ddi_table(void *adapter, uint32_t table_type)
{
    AV_SCOPE("UmdRoot::fill_ddi_table");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    if (adapters_.adapter_of(adapter) == nullptr) return hr_invalid_arg;
    note("ddi table refused, type", table_type);
    return hr_not_impl;
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
    connection_.report(writer);
    adapters_.report(writer);
    devices_.report(writer);
    writer.close();
}

bool UmdRoot::invariants_hold() const
{
    AV_SCOPE("UmdRoot::invariants_hold");
    if (!connection_.invariants_hold()) return false;
    if (!adapters_.invariants_hold() || !devices_.invariants_hold()) return false;
    /* The connection is held once per open adapter and by nothing else, so a
     * process with no adapter open holds no kernel adapter either. */
    if (connection_.opens() != adapters_.open_adapters()) return false;
    return connection_.open() == (adapters_.open_adapters() != 0u);
}

}  // namespace av::d3d

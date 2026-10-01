/* avumd.dll as the Direct3D runtimes reach it (contracts/umd_interface.md):
 * the answers to the two exports, the adapters opened through them, the
 * devices made on those adapters, and a connection to each kernel adapter
 * they opened, which every open of that adapter shares.
 *
 * One per process, in ObjectStorage in the exports' translation unit, bound
 * into UmdBinding for its lifetime so a table thunk - which the runtime calls
 * with its argument structure and no context - can find it.
 *
 * Nothing is opened before the first OpenAdapter call, and a connection is
 * closed with the last CloseAdapter of its adapter: a process that loaded the DLL and asked
 * it nothing holds no kernel object. */
#ifndef AV_D3D_UMD_ROOT_HPP
#define AV_D3D_UMD_ROOT_HPP

#include "core/trace/state_reporter.hpp"
#include "icd/d3d/adapter_table.hpp"
#include "icd/d3d/device_table.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/umd_connections.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
}  // namespace av

namespace av::icd {
class LogSink;
class ThreadLock;
}  // namespace av::icd

namespace av::d3d {

class UmdBinding;
class UmdHostDriver;
struct UmdRootProbe;
struct D3d102ddiargGetCapsRecord;
struct D3d10ddiargCreateDeviceRecord;
struct D3d10ddiargOpenAdapterRecord;
struct D3d12ddiTableRequestRecord;
struct D3d12ddiargCreateDeviceRecord;
struct D3d12ddiargOpenAdapterRecord;

class UmdRoot final : public StateReporter {
public:
    /* Binds itself into `binding` until destroyed. */
    UmdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, icd::LogSink &log, UmdBinding &binding,
            UmdHostDriver *builder = nullptr);
    ~UmdRoot();
    UmdRoot(const UmdRoot &) = delete;
    UmdRoot &operator=(const UmdRoot &) = delete;

    /* The exports. hr_invalid_arg for a null argument or null callbacks;
     * hr_no_interface for an Interface this driver does not serve;
     * hr_fail when there is no avgpu adapter; hr_out_of_memory with no room.
     * Nothing is left open unless hr_ok. */
    Hresult open_adapter10_2(D3d10ddiargOpenAdapterRecord *args);
    Hresult open_adapter12(D3d12ddiargOpenAdapterRecord *args);

    /* The adapter-table slots, each reached from a thunk with the adapter
     * handle the open handed back. A handle this root did not hand out is
     * hr_invalid_arg and is never dereferenced. */
    Hresult supported_versions(void *adapter, uint32_t *entries, uint64_t *words);
    Hresult caps(void *adapter, D3d102ddiargGetCapsRecord *args);
    size_t  private_device_bytes(void *adapter) const;
    Hresult create_device(void *adapter, D3d10ddiargCreateDeviceRecord *create);
    /* D3D12's create, over the same device table: the device is adopted into
     * it and built by the host driver, and without one no device is made and
     * the runtime is told so. */
    Hresult create_device12(void *adapter, D3d12ddiargCreateDeviceRecord *create);
    Hresult close_adapter(void *adapter);
    void    destroy_device(void *block);
    Hresult optional_ddi_tables(void *adapter, uint32_t *entries, D3d12ddiTableRequestRecord *requests);
    /* DXGI's table is the root's own seam, as it is inside D3D11's create;
     * every other table is the host driver's. */
    /* `index` and `runtime_table` are D3D12Core's: which of a type's tables
     * this is, and the handle it names that table by. */
    Hresult fill_ddi_table(void *adapter, uint32_t table_type, void *table, size_t bytes, uint32_t index,
                           void *runtime_table);

    /* What a DXGI thunk resolves its argument's hDevice through. */
    UmdDevice *device_of(void *block) const;

    /* `umd/0` with the adapter and device counts, then each open connection, each
     * adapter and each device (docs/trace.md). Never waits: with the lock held
     * elsewhere only `umd/0 busy=true` is written. */
    void report(StateWriter &writer) override;

    bool invariants_hold() const;

private:
    /* Opens the adapter, learns its identity and counts the open; the first
     * refusal, with everything it made given back. */
    uint64_t adapter_luid(void *runtime_adapter, const void *callbacks);
    Hresult open(UmdAdapter *&opened, RuntimeKind runtime, void *runtime_adapter, const void *callbacks,
                 uint32_t interface_version, uint32_t runtime_version);
    /* The object model of one create, without the runtime's answer: the
     * device, its recorded handles and its published DXGI table. Feature 056's
     * create is this plus a filled device-functions table and hr_ok. */
    Hresult adopt_device(UmdAdapter &adapter, D3d10ddiargCreateDeviceRecord &create, UmdDevice *&made);
    void    forget_device(void *block);
    void    note(const char *what, uint64_t value);

    Kmt             &kmt_;
    icd::ThreadLock &lock_;
    icd::LogSink    &log_;
    UmdBinding      &binding_;
    UmdHostDriver *builder_;
    UmdConnections   connections_;
    AdapterTable     adapters_;
    DeviceTable      devices_;

    friend struct UmdRootProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_ROOT_HPP

#ifndef AV_D3D_UMD_ROOT_HPP
#define AV_D3D_UMD_ROOT_HPP

#include "core/trace/state_reporter.hpp"
#include "icd/d3d/adapter_table.hpp"
#include "icd/d3d/device_table.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "icd/d3d/umd_connections.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class DriverLog;
}

namespace av {
class BlockSource;
class Kmt;
class ThreadLock;
}

namespace av::d3d {

class UmdHostDriver;
struct UmdRootProbe;
struct D3d102ddiargGetCapsRecord;
struct D3d10ddiargCreateDeviceRecord;
struct D3d10ddiargOpenAdapterRecord;
struct D3dddiargOpenAdapterRecord;
struct D3d12ddiTableRequestRecord;
struct D3d12ddiargCreateDeviceRecord;
struct D3d12ddiargOpenAdapterRecord;

class UmdRoot final : public StateReporter {
public:
    UmdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, DriverLog &log, UmdBinding &binding,
            UmdHostDriver *builder = nullptr);
    ~UmdRoot();
    UmdRoot(const UmdRoot &) = delete;
    UmdRoot &operator=(const UmdRoot &) = delete;

    Hresult open_adapter10_2(D3d10ddiargOpenAdapterRecord *args);
    Hresult open_adapter12(D3d12ddiargOpenAdapterRecord *args);
    Hresult open_adapter9(D3dddiargOpenAdapterRecord *args);

    Hresult supported_versions(void *adapter, uint32_t *entries, uint64_t *words);
    Hresult caps(void *adapter, D3d102ddiargGetCapsRecord *args);
    size_t  private_device_bytes(void *adapter) const;
    Hresult create_device(void *adapter, D3d10ddiargCreateDeviceRecord *create);
    Hresult create_device12(void *adapter, D3d12ddiargCreateDeviceRecord *create);
    Hresult create_device9(void *adapter, void *create);
    Hresult destroy_device9(void *device);
    Hresult close_adapter(void *adapter);
    void    destroy_device(void *block);
    Hresult optional_ddi_tables(void *adapter, uint32_t *entries, D3d12ddiTableRequestRecord *requests);
    Hresult fill_ddi_table(void *adapter, uint32_t table_type, void *table, size_t bytes, uint32_t index,
                           void *runtime_table);

    UmdDevice *device_of(void *block) const;

    void report(StateWriter &writer) override;

    bool invariants_hold() const;

private:
    uint64_t adapter_luid(void *runtime_adapter, const void *callbacks);
    Hresult open(UmdAdapter *&opened, RuntimeKind runtime, void *runtime_adapter, const void *callbacks,
                 uint32_t interface_version, uint32_t runtime_version);
    Hresult adopt_device(UmdAdapter &adapter, D3d10ddiargCreateDeviceRecord &create, UmdDevice *&made);
    void    forget_device(void *block); 

    ThreadLock      &lock_;
    DriverLog      &lines_;
    UmdBinding      &binding_;
    UmdHostDriver *builder_;
    UmdConnections   connections_;
    AdapterTable     adapters_;
    DeviceTable      devices_;

    friend struct UmdRootProbe;
};

}

#endif

#ifndef AV_D3D_ADAPTER_TABLE_HPP
#define AV_D3D_ADAPTER_TABLE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/d3d/runtime_kind.hpp"
#include "icd/d3d/umd_adapter.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::d3d {

struct D3dddiAdapterCallbacksRecord;
class UmdConnection;

struct AdapterTableProbe;

class AdapterTable {
public:
    explicit AdapterTable(BlockSource &memory);
    AdapterTable(const AdapterTable &) = delete;
    AdapterTable &operator=(const AdapterTable &) = delete;

    [[nodiscard]] Status open(UmdAdapter *&opened, RuntimeKind runtime, void *runtime_adapter,
                              const D3dddiAdapterCallbacksRecord *callbacks, UmdConnection &connection,
                              uint32_t interface_version, uint32_t runtime_version);
    UmdAdapter          *adapter_of(void *handle) const;
    [[nodiscard]] Status close(void *handle);
    uint32_t             open_adapters() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    ObjectPool<UmdAdapter> adapters_;

    friend struct AdapterTableProbe;
};

}

#endif

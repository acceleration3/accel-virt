/* Every adapter open in the process, and the only way back from the handle the
 * runtime holds (contracts/umd_interface.md). An adapter's own address is its
 * handle, so the pool that made it is what judges a handle coming the other
 * way: a pointer this table never handed out answers null and is never
 * dereferenced. */
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
}  // namespace av

namespace av::d3d {

struct D3dddiAdapterCallbacksRecord;
class UmdConnection;

struct AdapterTableProbe;

class AdapterTable {
public:
    explicit AdapterTable(BlockSource &memory);
    AdapterTable(const AdapterTable &) = delete;
    AdapterTable &operator=(const AdapterTable &) = delete;

    /* no_memory when the pool has none; ok with `opened` set otherwise. */
    [[nodiscard]] Status open(UmdAdapter *&opened, RuntimeKind runtime, void *runtime_adapter,
                              const D3dddiAdapterCallbacksRecord *callbacks, UmdConnection &connection,
                              uint32_t interface_version, uint32_t runtime_version);
    /* The adapter that handle names, or null. */
    UmdAdapter          *adapter_of(void *handle) const;
    /* invalid_parameter for a handle this did not hand out; conflict while a
     * device made through it is live. */
    [[nodiscard]] Status close(void *handle);
    uint32_t             open_adapters() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    ObjectPool<UmdAdapter> adapters_;

    friend struct AdapterTableProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_ADAPTER_TABLE_HPP

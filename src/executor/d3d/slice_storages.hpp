#ifndef AV_SLICE_STORAGES_HPP
#define AV_SLICE_STORAGES_HPP

#include "core/support/object_pool.hpp"
#include "executor/d3d/slice_bindings.hpp"
#include "executor/d3d/slice_storage.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class D3d11BatchProgress;
class D3d11HostDevice;
class DriverLog;
class SourceMemory;

class SliceStorages {
public:
    SliceStorages(BlockSource &blocks, DriverLog &lines);
    SliceStorages(const SliceStorages &) = delete;
    SliceStorages &operator=(const SliceStorages &) = delete;

    const VulkanBuffer *bind(D3d11HostDevice &device, SourceMemory &store, const D3d11BatchProgress &progress);
    [[nodiscard]] bool  attach(const SliceBindings::Binding &binding);
    bool                leave(uint32_t resource, SliceBindings::Binding &was);
    bool                next_bound_to(const SourceMemory &store, SliceBindings::Binding &was);
    void                retire(const SourceMemory &store, const D3d11BatchProgress &progress);
    void                reclaim(const D3d11BatchProgress &progress);
    void                destroy_all();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    SliceStorage *find(const SourceMemory &store);

    ObjectPool<SliceStorage> storages_;
    SliceBindings            bindings_;
    DriverLog               &lines_;
    uint32_t                 retiring_;
    bool                     reported_;
};

}

#endif

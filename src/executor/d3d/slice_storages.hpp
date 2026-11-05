#ifndef AV_SLICE_STORAGES_HPP
#define AV_SLICE_STORAGES_HPP

#include "core/support/object_pool.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/slice_bindings.hpp"
#include "executor/d3d/slice_storage.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class DriverLog;
class SliceImporter;
class SourceMemory;

class SliceStorages {
public:
    SliceStorages(BlockSource &blocks, DriverLog &lines);
    SliceStorages(const SliceStorages &) = delete;
    SliceStorages &operator=(const SliceStorages &) = delete;

    const VulkanBuffer *bind(SliceImporter &importer, SourceMemory &store, uint64_t next, uint64_t reached);
    [[nodiscard]] bool  attach(const SliceBindings::Binding &binding);
    bool                leave(uint32_t resource, SliceBindings::Binding &was);
    bool                bound(uint32_t resource, SliceBindings::Binding &was) const;
    template <typename Visitor>
    void                take_all_bound_to(const SourceMemory &store, Visitor visit);
    void                retire(const SourceMemory &store, uint64_t next);
    bool                holds(const SourceMemory &store);
    void                reclaim(uint64_t reached);
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

template <typename Visitor>
void SliceStorages::take_all_bound_to(const SourceMemory &store, Visitor visit)
{
    AV_SCOPE("SliceStorages::take_all_bound_to");
    bindings_.take_all_bound_to(store, visit);
}

}

#endif

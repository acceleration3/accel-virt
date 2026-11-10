#ifndef AV_EXECUTOR_D3D9_LENT_COPIES_HPP
#define AV_EXECUTOR_D3D9_LENT_COPIES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/shim/d3d9_vulkan_lane.hpp"
#include "executor/shim/vulkan_buffer.hpp"
#include "executor/shim/vulkan_timeline.hpp"
#include "proto/generated/d3d9/d3d9_wire_types.h"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DSurface9;

namespace av {

class BlockSource;
class SourceMemory;

class D3d9LentCopies {
public:
    static constexpr uint32_t most_memories = 4u;

    explicit D3d9LentCopies(BlockSource &blocks);
    ~D3d9LentCopies();
    D3d9LentCopies(const D3d9LentCopies &) = delete;
    D3d9LentCopies &operator=(const D3d9LentCopies &) = delete;

    [[nodiscard]] Status copy(IDirect3DDevice9 *device, IDirect3DSurface9 *source, const d3d9proto::Rect &area,
                              uint64_t handle, SourceMemory &memory, uint32_t pitch, uint32_t x, uint32_t y);
    void forget_device(IDirect3DDevice9 *device);
    void release();

private:
    struct Storage {
        VulkanBuffer       buffer_;
        const SourceMemory *memory_;
        uint64_t           handle_;
        uint64_t           used_;
    };
    struct Copier {
        D3d9VulkanLane vulkan_;
        VulkanTimeline done_;
        uint64_t       issued_;
        uint64_t       uses_;
        Storage        storages_[most_memories];
        bool           refused_;
    };

    Copier *copier_for(IDirect3DDevice9 *device);
    void    close(Copier &copier);
    const VulkanBuffer *storage_for(Copier &copier, uint64_t handle, SourceMemory &memory);

    ObjectPool<Copier>            copiers_;
    KeyedSlots<uint64_t, Copier *> copier_of_;
};

}

#endif

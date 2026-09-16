#include "device/context/device_context_table.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

DeviceContextTable::DeviceContextTable(Host &host, BlobTable &blobs, FenceInterrupts &interrupts,
                                       const ExecutorLibrary &library)
    : host_(host), blobs_(blobs), interrupts_(interrupts), library_(library), slots_{}
    {
        AV_SCOPE("DeviceContextTable::DeviceContextTable");
    }

DeviceContextTable::~DeviceContextTable()
{
    AV_SCOPE("DeviceContextTable::~DeviceContextTable");
    destroy_all();
}

ObjectStorage<DeviceContext> *DeviceContextTable::slot_of(uint32_t id)
{
    AV_SCOPE("DeviceContextTable::slot_of");
    for (ObjectStorage<DeviceContext> &slot : slots_)
        if (slot.constructed() && slot.object().id() == id) return &slot;
    return nullptr;
}

Status DeviceContextTable::create(uint32_t id)
{
    AV_SCOPE("DeviceContextTable::create");
    host_.require_main_loop("DeviceContextTable::create");
    if (id == 0u) return Status::invalid_parameter;
    if (slot_of(id) != nullptr) return Status::conflict;
    for (ObjectStorage<DeviceContext> &slot : slots_) {
        if (slot.construct(host_, blobs_, interrupts_, library_, id) != Status::ok) continue;   /* occupied */
        const Status status = slot.object().start();
        if (status != Status::ok) slot.destroy();
        return status;
    }
    return Status::no_space;
}

Status DeviceContextTable::destroy(uint32_t id)
{
    AV_SCOPE("DeviceContextTable::destroy");
    host_.require_main_loop("DeviceContextTable::destroy");
    ObjectStorage<DeviceContext> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    slot->destroy();
    return Status::ok;
}

void DeviceContextTable::destroy_all()
{
    AV_SCOPE("DeviceContextTable::destroy_all");
    host_.require_main_loop("DeviceContextTable::destroy_all");
    for (ObjectStorage<DeviceContext> &slot : slots_) slot.destroy();
}

DeviceContext *DeviceContextTable::find(uint32_t id)
{
    AV_SCOPE("DeviceContextTable::find");
    host_.require_main_loop("DeviceContextTable::find");
    ObjectStorage<DeviceContext> *const slot = slot_of(id);
    return slot != nullptr ? &slot->object() : nullptr;
}

bool DeviceContextTable::holds_doorbell(uint32_t index)
{
    AV_SCOPE("DeviceContextTable::holds_doorbell");
    host_.require_main_loop("DeviceContextTable::holds_doorbell");
    for (const ObjectStorage<DeviceContext> &slot : slots_)
        if (slot.constructed() && slot.object().binding().bound() && slot.object().binding().doorbell() == index)
            return true;
    return false;
}

uint32_t DeviceContextTable::live() const
{
    AV_SCOPE("DeviceContextTable::live");
    uint32_t count = 0u;
    for (const ObjectStorage<DeviceContext> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

/* Every live context has a running worker and an id no other live context has. */
bool DeviceContextTable::invariants_hold() const
{
    AV_SCOPE("DeviceContextTable::invariants_hold");
    for (uint32_t first = 0u; first < max_contexts; first++) {
        if (!slots_[first].constructed()) continue;
        const DeviceContext &context = slots_[first].object();
        if (!context.running() || context.id() == 0u) return false;
        for (uint32_t second = first + 1u; second < max_contexts; second++)
            if (slots_[second].constructed() && slots_[second].object().id() == context.id()) return false;
    }
    return true;
}

}  // namespace av

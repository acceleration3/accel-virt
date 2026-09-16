#include "icd/vulkan/loader_handle.hpp"

#include "core/support/object_pool.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/device_memory.hpp"

namespace av::icd {

LoaderHandle::LoaderHandle(Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent)
    : loader_data_(reinterpret_cast<void *>(loader_magic)), host_(host), connection_(connection), kind_(kind),
      parent_(parent), batches_(nullptr), batch_(nullptr), memories_(nullptr), memory_(nullptr) {}

/* A batch or memory is only ever taken from the pool it names, and only this
 * handle destroys it. */
LoaderHandle::~LoaderHandle()
{
    if (batch_ != nullptr) static_cast<void>(batches_->destroy(batch_));
    if (memory_ != nullptr) static_cast<void>(memories_->destroy(memory_));
}

void LoaderHandle::record_into(ObjectPool<CommandBatch> &batches, CommandBatch &batch)
{
    batches_ = &batches;
    batch_ = &batch;
}

CommandBatch *LoaderHandle::batch() const { return batch_; }

void LoaderHandle::allocate_from(ObjectPool<DeviceMemory> &memories, DeviceMemory &memory)
{
    memories_ = &memories;
    memory_ = &memory;
}

DeviceMemory *LoaderHandle::device_memory() const { return memory_; }

LoaderHandle::Kind LoaderHandle::kind() const { return kind_; }
uint64_t           LoaderHandle::host() const { return host_; }
Connection        *LoaderHandle::connection() const { return connection_; }
LoaderHandle      *LoaderHandle::parent() const { return parent_; }

bool LoaderHandle::names(Kind kind, uint64_t host, const Connection *connection) const
{
    return kind_ == kind && host_ == host && connection_ == connection;
}

bool LoaderHandle::descends_from(const LoaderHandle &ancestor) const
{
    for (const LoaderHandle *above = parent_; above != nullptr; above = above->parent_)
        if (above == &ancestor) return true;
    return false;
}

bool LoaderHandle::invariants_hold() const
{
    if (host_ == 0u || connection_ == nullptr) return false;
    if ((batch_ == nullptr) != (batches_ == nullptr)) return false;
    if (batch_ != nullptr && kind_ != Kind::command_buffer) return false;
    if ((memory_ == nullptr) != (memories_ == nullptr)) return false;
    if (memory_ != nullptr && kind_ != Kind::device) return false;
    if (kind_ == Kind::instance) return parent_ == nullptr;
    return parent_ != nullptr && parent_->kind_ < kind_ && parent_->connection_ == connection_;
}

}  // namespace av::icd

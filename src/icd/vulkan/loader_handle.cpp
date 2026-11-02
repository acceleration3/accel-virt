#include "icd/vulkan/loader_handle.hpp"

#include "core/support/object_pool.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/device_memory.hpp"

namespace av::icd {

LoaderHandle::LoaderHandle(Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent)
    : loader_data_(reinterpret_cast<void *>(loader_magic)), host_(host), connection_(connection), kind_(kind),
      parent_(parent), batches_(nullptr), batch_(nullptr), memories_(nullptr), memory_(nullptr), idle_fence_(0u), idle_at_(~uint64_t{0})
      {
          AV_SCOPE("LoaderHandle::LoaderHandle");
      }

LoaderHandle::~LoaderHandle()
{
    AV_SCOPE("LoaderHandle::~LoaderHandle");
    if (batch_ != nullptr) static_cast<void>(batches_->destroy(batch_));
    if (memory_ != nullptr) static_cast<void>(memories_->destroy(memory_));
}

void LoaderHandle::record_into(ObjectPool<CommandBatch> &batches, CommandBatch &batch)
{
    AV_SCOPE("LoaderHandle::record_into");
    batches_ = &batches;
    batch_ = &batch;
}

CommandBatch *LoaderHandle::batch() const
{
    AV_SCOPE("LoaderHandle::batch");
    return batch_;
}

void LoaderHandle::allocate_from(ObjectPool<DeviceMemory> &memories, DeviceMemory &memory)
{
    AV_SCOPE("LoaderHandle::allocate_from");
    memories_ = &memories;
    memory_ = &memory;
}

DeviceMemory *LoaderHandle::device_memory() const
{
    AV_SCOPE("LoaderHandle::device_memory");
    return memory_;
}

void LoaderHandle::keep_idle_fence(uint64_t fence)
{
    AV_SCOPE("LoaderHandle::keep_idle_fence");
    idle_fence_ = fence;
}

uint64_t LoaderHandle::idle_fence() const
{
    AV_SCOPE("LoaderHandle::idle_fence");
    return idle_fence_;
}

void LoaderHandle::mark_idle(uint64_t submissions)
{
    AV_SCOPE("LoaderHandle::mark_idle");
    idle_at_ = submissions;
}

uint64_t LoaderHandle::idle_at() const
{
    AV_SCOPE("LoaderHandle::idle_at");
    return idle_at_;
}

LoaderHandle::Kind LoaderHandle::kind() const
{
    AV_SCOPE("LoaderHandle::kind");
    return kind_;
}
uint64_t           LoaderHandle::host() const
{
    AV_SCOPE("LoaderHandle::host");
    return host_;
}
Connection        *LoaderHandle::connection() const
{
    AV_SCOPE("LoaderHandle::connection");
    return connection_;
}
LoaderHandle      *LoaderHandle::parent() const
{
    AV_SCOPE("LoaderHandle::parent");
    return parent_;
}

bool LoaderHandle::names(Kind kind, uint64_t host, const Connection *connection) const
{
    AV_SCOPE("LoaderHandle::names");
    return kind_ == kind && host_ == host && connection_ == connection;
}

bool LoaderHandle::invariants_hold() const
{
    AV_SCOPE("LoaderHandle::invariants_hold");
    if (host_ == 0u || connection_ == nullptr) return false;
    if ((batch_ == nullptr) != (batches_ == nullptr)) return false;
    if (batch_ != nullptr && kind_ != Kind::command_buffer) return false;
    if ((memory_ == nullptr) != (memories_ == nullptr)) return false;
    if (memory_ != nullptr && kind_ != Kind::device) return false;
    if (kind_ == Kind::instance) return parent_ == nullptr;
    return parent_ != nullptr && parent_->kind_ < kind_ && parent_->connection_ == connection_;
}

}

#include "icd/vulkan/memory_allocate_chain.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

MemoryAllocateChain::MemoryAllocateChain()
    : info_{}, export_{}, dedicated_{}, flags_{}, last_(nullptr), exports_(false)
{
    AV_SCOPE("MemoryAllocateChain::MemoryAllocateChain");
}

Status MemoryAllocateChain::take(const VkMemoryAllocateInfo &info, const Win32HandleKind &kind, VkFlags share_type)
{
    AV_SCOPE("MemoryAllocateChain::take");
    clear();
    info_ = info;
    info_.pNext = nullptr;
    uint32_t        read = 0u;
    VkStructureType seen[carried_kinds] = {};
    for (const auto *node = static_cast<const VkBaseInStructure *>(info.pNext); node != nullptr;
         node = node->pNext) {
        const VkStructureType type = static_cast<VkStructureType>(foreign_enum(node->sType));
        for (uint32_t earlier = 0u; earlier < read; earlier++)
            if (seen[earlier] == type) return refusal(Status::malformed);
        if (type != VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO && type != export_memory_win32_handle_info &&
            type != VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO &&
            type != VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO)
            return refusal(Status::not_supported);
        seen[read++] = type;
        if (type == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO) {
            const auto *asked = reinterpret_cast<const VkExportMemoryAllocateInfo *>(node);
            if (!kind.answers(asked->handleTypes)) return refusal(Status::not_supported);
            export_.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
            export_.handleTypes = share_type;
            link(&export_);
            exports_ = true;
            continue;
        }
        if (type == export_memory_win32_handle_info) continue;
        if (type == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO) {
            dedicated_ = *reinterpret_cast<const VkMemoryDedicatedAllocateInfo *>(node);
            link(&dedicated_);
            continue;
        }
        flags_ = *reinterpret_cast<const VkMemoryAllocateFlagsInfo *>(node);
        link(&flags_);
    }
    return Status::ok;
}

void MemoryAllocateChain::clear()
{
    AV_SCOPE("MemoryAllocateChain::clear");
    info_ = VkMemoryAllocateInfo{};
    export_ = VkExportMemoryAllocateInfo{};
    dedicated_ = VkMemoryDedicatedAllocateInfo{};
    flags_ = VkMemoryAllocateFlagsInfo{};
    last_ = nullptr;
    exports_ = false;
}

Status MemoryAllocateChain::refusal(Status why)
{
    AV_SCOPE("MemoryAllocateChain::refusal");
    clear();
    return why;
}

void MemoryAllocateChain::link(void *node)
{
    AV_SCOPE("MemoryAllocateChain::link");
    auto *const added = static_cast<VkBaseOutStructure *>(node);
    added->pNext = nullptr;
    if (last_ == nullptr)
        info_.pNext = added;
    else
        last_->pNext = added;
    last_ = added;
}

const VkMemoryAllocateInfo *MemoryAllocateChain::forwarded() const
{
    AV_SCOPE("MemoryAllocateChain::forwarded");
    return &info_;
}

bool MemoryAllocateChain::exports() const
{
    AV_SCOPE("MemoryAllocateChain::exports");
    return exports_;
}

bool MemoryAllocateChain::invariants_hold() const
{
    AV_SCOPE("MemoryAllocateChain::invariants_hold");
    uint32_t nodes = 0u;
    for (const auto *node = static_cast<const VkBaseInStructure *>(info_.pNext); node != nullptr;
         node = node->pNext) {
        if (nodes == carried_kinds) return false;
        nodes++;
        const bool own = node == reinterpret_cast<const VkBaseInStructure *>(&export_) ||
                         node == reinterpret_cast<const VkBaseInStructure *>(&dedicated_) ||
                         node == reinterpret_cast<const VkBaseInStructure *>(&flags_);
        if (!own) return false;
    }
    return true;
}

}

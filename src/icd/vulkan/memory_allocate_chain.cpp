#include "icd/vulkan/memory_allocate_chain.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

MemoryAllocateChain::MemoryAllocateChain()
    : info_{}, export_{}, import_{}, dedicated_{}, flags_{}, last_(nullptr), exports_(false), imports_(false)
{
    AV_SCOPE("MemoryAllocateChain::MemoryAllocateChain");
}

Status MemoryAllocateChain::take(const VkMemoryAllocateInfo &info, const Win32HandleKind &kind,
                                 VkFlags share_type, int fd)
{
    AV_SCOPE("MemoryAllocateChain::take");
    clear();
    info_ = info;
    info_.pNext = nullptr;
    uint32_t        read = 0u;
    VkStructureType seen[carried_kinds] = {};
    for (const auto *node = static_cast<const VkBaseInStructure *>(info.pNext); node != nullptr;
         node = node->pNext) {
        /* The application wrote it, so a value outside the enumeration is a
         * value to refuse rather than to load as one. */
        const VkStructureType type = static_cast<VkStructureType>(foreign_enum(node->sType));
        for (uint32_t earlier = 0u; earlier < read; earlier++)
            if (seen[earlier] == type) return refusal(Status::malformed);
        if (type != VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO && type != import_memory_win32_handle_info &&
            type != export_memory_win32_handle_info && type != VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO &&
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
        if (type == import_memory_win32_handle_info) {
            if (fd < 0) return refusal(Status::invalid_parameter);
            import_.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
            import_.handleType = static_cast<VkExternalMemoryHandleTypeFlagBits>(share_type);
            import_.fd = fd;
            link(&import_);
            imports_ = true;
            continue;
        }
        /* A security descriptor and a name are what another process would open
         * the handle with, and both devices here are in one. */
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
    import_ = VkImportMemoryFdInfoKHR{};
    dedicated_ = VkMemoryDedicatedAllocateInfo{};
    flags_ = VkMemoryAllocateFlagsInfo{};
    last_ = nullptr;
    exports_ = false;
    imports_ = false;
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

bool MemoryAllocateChain::imports() const
{
    AV_SCOPE("MemoryAllocateChain::imports");
    return imports_;
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
                         node == reinterpret_cast<const VkBaseInStructure *>(&import_) ||
                         node == reinterpret_cast<const VkBaseInStructure *>(&dedicated_) ||
                         node == reinterpret_cast<const VkBaseInStructure *>(&flags_);
        if (!own) return false;
    }
    return true;
}

}  // namespace av::icd

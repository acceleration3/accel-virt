#include "icd/vulkan/semaphore_create_chain.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/semaphore_win32_handle_record.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

SemaphoreCreateChain::SemaphoreCreateChain() : info_{}, export_{}, typed_{}, last_(nullptr)
{
    AV_SCOPE("SemaphoreCreateChain::SemaphoreCreateChain");
}

Status SemaphoreCreateChain::take(const VkSemaphoreCreateInfo &info, const Win32HandleKind &kind, VkFlags share_type)
{
    AV_SCOPE("SemaphoreCreateChain::take");
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
        if (type != VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO && type != export_semaphore_win32_handle_info &&
            type != VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO)
            return refusal(Status::not_supported);
        seen[read++] = type;
        if (type == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO) {
            const auto *asked = reinterpret_cast<const VkExportSemaphoreCreateInfo *>(node);
            if (!kind.answers(asked->handleTypes)) return refusal(Status::not_supported);
            export_.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
            export_.handleTypes = share_type;
            link(&export_);
            continue;
        }
        if (type == export_semaphore_win32_handle_info) continue;
        typed_ = *reinterpret_cast<const VkSemaphoreTypeCreateInfo *>(node);
        link(&typed_);
    }
    return Status::ok;
}

void SemaphoreCreateChain::clear()
{
    AV_SCOPE("SemaphoreCreateChain::clear");
    info_ = VkSemaphoreCreateInfo{};
    export_ = VkExportSemaphoreCreateInfo{};
    typed_ = VkSemaphoreTypeCreateInfo{};
    last_ = nullptr;
}

Status SemaphoreCreateChain::refusal(Status why)
{
    AV_SCOPE("SemaphoreCreateChain::refusal");
    clear();
    return why;
}

void SemaphoreCreateChain::link(void *node)
{
    AV_SCOPE("SemaphoreCreateChain::link");
    auto *const added = static_cast<VkBaseOutStructure *>(node);
    added->pNext = nullptr;
    if (last_ == nullptr)
        info_.pNext = added;
    else
        last_->pNext = added;
    last_ = added;
}

const VkSemaphoreCreateInfo *SemaphoreCreateChain::forwarded() const
{
    AV_SCOPE("SemaphoreCreateChain::forwarded");
    return &info_;
}

bool SemaphoreCreateChain::invariants_hold() const
{
    AV_SCOPE("SemaphoreCreateChain::invariants_hold");
    uint32_t nodes = 0u;
    for (const auto *node = static_cast<const VkBaseInStructure *>(info_.pNext); node != nullptr;
         node = node->pNext) {
        if (nodes == carried_kinds) return false;
        nodes++;
        const bool own = node == reinterpret_cast<const VkBaseInStructure *>(&export_) ||
                         node == reinterpret_cast<const VkBaseInStructure *>(&typed_);
        if (!own) return false;
    }
    return true;
}

}

#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/semaphore_create_chain.hpp"
#include "icd/vulkan/semaphore_win32_handle_record.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"

#include <cstdint>

namespace av::icd {
struct SemaphoreCreateChainProbe {
    static void a_node_of_someone_else_s(SemaphoreCreateChain &chain, const void *node)
    { chain.info_.pNext = node; }
};

}

using av::Status;
using av::icd::SemaphoreCreateChain;
using av::icd::SemaphoreCreateChainProbe;
using av::icd::Win32HandleKind;

namespace {
const void *node_of(const VkSemaphoreCreateInfo &info, VkStructureType type)
{ for (const auto *node = static_cast<const VkBaseInStructure *>(info.pNext); node != nullptr;
       node = node->pNext)
      if (node->sType == type) return node;
  return nullptr; }

std::uint32_t nodes_of(const VkSemaphoreCreateInfo &info)
{ std::uint32_t count = 0u;
  for (const auto *node = static_cast<const VkBaseInStructure *>(info.pNext); node != nullptr;
       node = node->pNext)
      count++;
  return count; }

VkSemaphoreCreateInfo creation(const void *chain)
{ VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = chain;
  return info; }

constexpr VkFlags shared_as = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

Win32HandleKind semaphore_kind()
{ return Win32HandleKind(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                         VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT); }

}

TEST(semaphore_create_chain, an_export_of_a_win32_handle_is_sent_as_an_opaque_fd)
{ VkSemaphoreTypeCreateInfo timeline = {};
  timeline.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  timeline.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  timeline.initialValue = 7u;
  struct {
      VkStructureType type_;
      const void     *next_;
      const void     *attributes_;
      VkFlags         access_;
      const void     *name_;
  } win32 = {av::icd::export_semaphore_win32_handle_info, &timeline, nullptr, 0u, nullptr};
  VkExportSemaphoreCreateInfo asked = {};
  asked.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  asked.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  asked.pNext = &win32;
  const VkSemaphoreCreateInfo info = creation(&asked);
  const Win32HandleKind       kind = semaphore_kind();
  SemaphoreCreateChain        chain;
  CHECK(chain.invariants_hold());
  CHECK_STATUS(chain.take(info, kind, shared_as), Status::ok);
  CHECK(chain.invariants_hold());

  const VkSemaphoreCreateInfo &sent = *chain.forwarded();
  CHECK_EQ(nodes_of(sent), 2u);
  const auto *const exported = static_cast<const VkExportSemaphoreCreateInfo *>(
      node_of(sent, VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO));
  CHECK(exported != nullptr);
  CHECK_EQ(exported->handleTypes, static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT));
  const auto *const kept = static_cast<const VkSemaphoreTypeCreateInfo *>(
      node_of(sent, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO));
  CHECK(kept != nullptr);
  CHECK_EQ(kept->semaphoreType, VK_SEMAPHORE_TYPE_TIMELINE);
  CHECK_EQ(kept->initialValue, 7u);
  CHECK(node_of(sent, av::icd::export_semaphore_win32_handle_info) == nullptr); }

TEST(semaphore_create_chain, a_chain_it_cannot_carry_whole_is_refused)
{ const Win32HandleKind  kind = semaphore_kind();
  SemaphoreCreateChain   chain;

  VkExportSemaphoreCreateInfo other = {};
  other.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  other.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
  CHECK_STATUS(chain.take(creation(&other), kind, shared_as), Status::not_supported);
  CHECK_EQ(nodes_of(*chain.forwarded()), 0u);
  CHECK(chain.invariants_hold());

  VkExportSemaphoreCreateInfo twice = {};
  twice.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  twice.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  VkExportSemaphoreCreateInfo again = twice;
  twice.pNext = &again;
  CHECK_STATUS(chain.take(creation(&twice), kind, shared_as), Status::malformed);
  CHECK_EQ(nodes_of(*chain.forwarded()), 0u);

  VkBaseInStructure stranger = {};
  stranger.sType = VK_STRUCTURE_TYPE_MAX_ENUM;
  CHECK_STATUS(chain.take(creation(&stranger), kind, shared_as), Status::not_supported); }

TEST(semaphore_create_chain, a_node_that_is_not_its_own_breaks_the_invariant)
{ SemaphoreCreateChain chain;
  VkBaseInStructure    stranger = {};
  SemaphoreCreateChainProbe::a_node_of_someone_else_s(chain, &stranger);
  CHECK(!chain.invariants_hold()); }

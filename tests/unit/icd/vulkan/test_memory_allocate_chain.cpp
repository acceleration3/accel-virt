/* The chain the host is sent for an allocation that shares memory (feature
 * 047, US3). The host's driver knows no Win32 handle type, so an export naming
 * one is said as its opaque fd and an import as a fresh fd of the remembered
 * object; the other nodes an allocation carries are passed on whole, and one
 * this cannot carry is refused rather than quietly lost. */
#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/memory_allocate_chain.hpp"
#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"

#include <cstdint>

using av::Status;
using av::icd::ImportMemoryWin32HandleRecord;
using av::icd::MemoryAllocateChain;
using av::icd::Win32HandleKind;

namespace {

/* The nodes the chain answers, by type: what a caller would look for. */
const void *node_of(const VkMemoryAllocateInfo &info, VkStructureType type)
{ for (const auto *node = static_cast<const VkBaseInStructure *>(info.pNext); node != nullptr;
       node = node->pNext)
      if (node->sType == type) return node;
  return nullptr; }

std::uint32_t nodes_of(const VkMemoryAllocateInfo &info)
{ std::uint32_t count = 0u;
  for (const auto *node = static_cast<const VkBaseInStructure *>(info.pNext); node != nullptr;
       node = node->pNext)
      count++;
  return count; }

VkMemoryAllocateInfo allocation(const void *chain)
{ VkMemoryAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.pNext = chain;
  info.allocationSize = 4096u;
  info.memoryTypeIndex = 3u;
  return info; }

Win32HandleKind memory_kind()
{ return Win32HandleKind(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                         VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT); }

}  // namespace

TEST(memory_allocate_chain, an_export_of_a_win32_handle_is_sent_as_an_opaque_fd)
{ VkExportMemoryAllocateInfo asked = {};
  asked.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  asked.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  /* What DXVK sends with it: the security attributes another process would
   * open the handle with, which one process has no use for. */
  struct {
      VkStructureType type_;
      const void     *next_;
      const void     *attributes_;
      VkFlags         access_;
      const void     *name_;
  } win32 = {av::icd::export_memory_win32_handle_info, nullptr, nullptr, 0u, nullptr};
  asked.pNext = &win32;
  VkMemoryDedicatedAllocateInfo dedicated = {};
  dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicated.image = reinterpret_cast<VkImage>(0x3300u);
  win32.next_ = &dedicated;
  const VkMemoryAllocateInfo   info = allocation(&asked);
  const Win32HandleKind        kind = memory_kind();
  MemoryAllocateChain          chain;
  CHECK(chain.invariants_hold());
  CHECK_STATUS(chain.take(info, kind, -1), Status::ok);
  CHECK(chain.invariants_hold());
  CHECK(chain.exports());
  CHECK(!chain.imports());

  const VkMemoryAllocateInfo &sent = *chain.forwarded();
  CHECK_EQ(sent.allocationSize, info.allocationSize);
  CHECK_EQ(sent.memoryTypeIndex, info.memoryTypeIndex);
  /* The export as the host understands it, the dedicated allocation whole, and
   * the Win32 node gone. */
  CHECK_EQ(nodes_of(sent), 2u);
  const auto *const exported =
      static_cast<const VkExportMemoryAllocateInfo *>(node_of(sent, VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO));
  CHECK(exported != nullptr);
  CHECK_EQ(exported->handleTypes, static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));
  const auto *const kept = static_cast<const VkMemoryDedicatedAllocateInfo *>(
      node_of(sent, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO));
  CHECK(kept != nullptr);
  CHECK(kept->image == dedicated.image);
  CHECK(node_of(sent, av::icd::export_memory_win32_handle_info) == nullptr); }

TEST(memory_allocate_chain, an_import_of_a_handle_is_sent_as_the_fd_it_was_resolved_to)
{ ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = reinterpret_cast<void *>(0x4400u);
  VkMemoryAllocateFlagsInfo flags = {};
  flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
  flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
  importing.next_ = &flags;
  const VkMemoryAllocateInfo info = allocation(&importing);
  const Win32HandleKind      kind = memory_kind();
  MemoryAllocateChain        chain;
  CHECK_STATUS(chain.take(info, kind, 7), Status::ok);
  CHECK(chain.invariants_hold());
  CHECK(chain.imports());
  CHECK(!chain.exports());
  const VkMemoryAllocateInfo &sent = *chain.forwarded();
  CHECK_EQ(nodes_of(sent), 2u);
  const auto *const imported =
      static_cast<const VkImportMemoryFdInfoKHR *>(node_of(sent, VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR));
  CHECK(imported != nullptr);
  CHECK_EQ(imported->fd, 7);
  CHECK_EQ(static_cast<VkFlags>(imported->handleType),
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));
  const auto *const kept =
      static_cast<const VkMemoryAllocateFlagsInfo *>(node_of(sent, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO));
  CHECK(kept != nullptr);
  CHECK_EQ(kept->flags, static_cast<VkFlags>(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT));
  /* An import with no fd resolved for it is nothing to send. */
  MemoryAllocateChain nothing;
  CHECK_STATUS(nothing.take(info, kind, -1), Status::invalid_parameter);
  CHECK_EQ(nodes_of(*nothing.forwarded()), 0u);
  CHECK(!nothing.imports());
  CHECK(nothing.invariants_hold()); }

/* Everything this cannot carry whole, and what a refusal leaves behind: an
 * allocation refused here is one the ICD answers with an error, so the chain
 * holds nothing rather than half of what was asked for. */
TEST(memory_allocate_chain, a_chain_it_cannot_carry_whole_is_refused)
{ const Win32HandleKind kind = memory_kind();
  MemoryAllocateChain   chain;

  /* An export of a handle type this ICD does not answer - a KMT handle, a
   * Direct3D resource, or the host's own fd, which the guest cannot name. */
  VkExportMemoryAllocateInfo other = {};
  other.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  other.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
  CHECK_STATUS(chain.take(allocation(&other), kind, -1), Status::not_supported);
  CHECK(!chain.exports());
  other.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
                      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
  CHECK_STATUS(chain.take(allocation(&other), kind, -1), Status::not_supported);

  /* A node nobody taught it: dropping it would be memory the application
   * asked for and did not get. */
  VkMemoryOpaqueCaptureAddressAllocateInfo captured = {};
  captured.sType = VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO;
  CHECK_STATUS(chain.take(allocation(&captured), kind, -1), Status::not_supported);
  /* And a type that is no structure type at all. */
  VkBaseInStructure nonsense = {};
  nonsense.sType = static_cast<VkStructureType>(0x7FFFFFFF);
  CHECK_STATUS(chain.take(allocation(&nonsense), kind, -1), Status::not_supported);

  /* The same node twice, which is what a cycle looks like one step in. */
  VkMemoryDedicatedAllocateInfo first = {};
  first.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  VkMemoryDedicatedAllocateInfo second = first;
  first.pNext = &second;
  CHECK_STATUS(chain.take(allocation(&first), kind, -1), Status::malformed);

  /* A chain that never ends repeats a type one step in, which is how the walk
   * ends at all. */
  VkMemoryDedicatedAllocateInfo ring = {};
  ring.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  ring.pNext = &ring;
  CHECK_STATUS(chain.take(allocation(&ring), kind, -1), Status::malformed);
  CHECK_EQ(nodes_of(*chain.forwarded()), 0u);
  CHECK(chain.invariants_hold());

  /* An allocation with no chain at all is carried as it is. */
  CHECK_STATUS(chain.take(allocation(nullptr), kind, -1), Status::ok);
  CHECK_EQ(nodes_of(*chain.forwarded()), 0u);
  CHECK(!chain.exports());
  CHECK(!chain.imports()); }

#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/memory_allocate_chain.hpp"
#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"

#include <cstdint>

namespace av::icd {
struct MemoryAllocateChainProbe {
    static void a_node_of_someone_else_s(MemoryAllocateChain &chain, const void *node)
    { chain.info_.pNext = node; }
    static void a_chain_that_never_ends(MemoryAllocateChain &chain)
    { chain.dedicated_.pNext = &chain.dedicated_; }
};

}

using av::Status;
using av::icd::ImportMemoryWin32HandleRecord;
using av::icd::MemoryAllocateChain;
using av::icd::MemoryAllocateChainProbe;
using av::icd::Win32HandleKind;

namespace {
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

constexpr VkFlags shared_as = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

Win32HandleKind memory_kind()
{ return Win32HandleKind(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                         VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT); }

}

TEST(memory_allocate_chain, an_export_of_a_win32_handle_is_sent_as_an_opaque_fd)
{ VkExportMemoryAllocateInfo asked = {};
  asked.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  asked.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
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
  CHECK_STATUS(chain.take(info, kind, shared_as, -1), Status::ok);
  CHECK(chain.invariants_hold());
  CHECK(chain.exports());
  CHECK(!chain.imports());

  const VkMemoryAllocateInfo &sent = *chain.forwarded();
  CHECK_EQ(sent.allocationSize, info.allocationSize);
  CHECK_EQ(sent.memoryTypeIndex, info.memoryTypeIndex);
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
  CHECK_STATUS(chain.take(info, kind, shared_as, 7), Status::ok);
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
  MemoryAllocateChain nothing;
  CHECK_STATUS(nothing.take(info, kind, shared_as, -1), Status::invalid_parameter);
  CHECK_EQ(nodes_of(*nothing.forwarded()), 0u);
  CHECK(!nothing.imports());
  CHECK(nothing.invariants_hold()); }

TEST(memory_allocate_chain, a_chain_it_cannot_carry_whole_is_refused)
{ const Win32HandleKind kind = memory_kind();
  MemoryAllocateChain   chain;

  VkExportMemoryAllocateInfo other = {};
  other.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  other.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
  CHECK_STATUS(chain.take(allocation(&other), kind, shared_as, -1), Status::not_supported);
  CHECK(!chain.exports());
  other.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
                      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
  CHECK_STATUS(chain.take(allocation(&other), kind, shared_as, -1), Status::not_supported);

  VkMemoryOpaqueCaptureAddressAllocateInfo captured = {};
  captured.sType = VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO;
  CHECK_STATUS(chain.take(allocation(&captured), kind, shared_as, -1), Status::not_supported);
  VkBaseInStructure nonsense = {};
  nonsense.sType = static_cast<VkStructureType>(0x7FFFFFFF);
  CHECK_STATUS(chain.take(allocation(&nonsense), kind, shared_as, -1), Status::not_supported);

  VkMemoryDedicatedAllocateInfo first = {};
  first.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  VkMemoryDedicatedAllocateInfo second = first;
  first.pNext = &second;
  CHECK_STATUS(chain.take(allocation(&first), kind, shared_as, -1), Status::malformed);

  VkMemoryDedicatedAllocateInfo ring = {};
  ring.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  ring.pNext = &ring;
  CHECK_STATUS(chain.take(allocation(&ring), kind, shared_as, -1), Status::malformed);
  CHECK_EQ(nodes_of(*chain.forwarded()), 0u);
  CHECK(chain.invariants_hold());

  CHECK_STATUS(chain.take(allocation(nullptr), kind, shared_as, -1), Status::ok);
  CHECK_EQ(nodes_of(*chain.forwarded()), 0u);
  CHECK(!chain.exports());
  CHECK(!chain.imports()); }

TEST(memory_allocate_chain, a_chain_of_nodes_it_does_not_own_is_named)
{ const Win32HandleKind kind = memory_kind();
  MemoryAllocateChain   chain;
  VkMemoryDedicatedAllocateInfo dedicated = {};
  dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  CHECK_STATUS(chain.take(allocation(&dedicated), kind, shared_as, -1), Status::ok);
  CHECK(chain.invariants_hold());
  MemoryAllocateChainProbe::a_chain_that_never_ends(chain);
  CHECK(!chain.invariants_hold());

  MemoryAllocateChain other;
  CHECK_STATUS(other.take(allocation(&dedicated), kind, shared_as, -1), Status::ok);
  MemoryAllocateChainProbe::a_node_of_someone_else_s(other, &dedicated);
  CHECK(!other.invariants_hold()); }

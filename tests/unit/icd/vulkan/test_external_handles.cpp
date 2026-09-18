/* The Win32 external objects through the ICD (feature 047, US3,
 * contracts/external_objects.md, research.md §10).
 *
 * A guest device is a host device, so two guest devices sharing an image share
 * it between two host devices - which the host's own fd extensions do, inside
 * the one process both live in. So a handle here is this ICD's own, an import
 * asks the host for a fresh fd of the object the handle names and forwards the
 * host's own import in the Win32 structure's place, and a handle this process
 * did not export names nothing at all.
 */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/semaphore_win32_handle_record.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::WireReader;
using av::icd::ImportMemoryWin32HandleRecord;
using av::icd::ImportSemaphoreWin32HandleRecord;
using av::icd::MemoryGetWin32HandleRecord;
using av::icd::SemaphoreGetWin32HandleRecord;
using namespace av::vkproto;

namespace {

using ExportMemory = VkResult (*)(VkDevice, const MemoryGetWin32HandleRecord *, void **);
using ExportSemaphore = VkResult (*)(VkDevice, const SemaphoreGetWin32HandleRecord *, void **);
using ImportSemaphore = VkResult (*)(VkDevice, const ImportSemaphoreWin32HandleRecord *);
using HandleProperties = VkResult (*)(VkDevice, VkFlags, void *, void *);
using Allocate = VkResult (*)(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *,
                              VkDeviceMemory *);
using DestroySemaphore = void (*)(VkDevice, VkSemaphore, const VkAllocationCallbacks *);
using BufferProperties = void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalBufferInfo *,
                                  VkExternalBufferProperties *);
using SemaphorePropertiesEntry = void (*)(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *,
                                          VkExternalSemaphoreProperties *);

constexpr std::uint64_t host_memory = 0x5500u;
constexpr std::uint64_t host_semaphore = 0x6600u;
/* The fd the host answers for a fresh export of a remembered object: a number
 * the guest only carries, since the fd is the host process's own. */
constexpr int shared_fd = 9;

/* A device on the scripted host, and the entry points sharing needs. */
class SharingRig {
public:
    SharingRig() : instance_(rig_.create_instance()), physical_device_(rig_.physical_device(instance_)),
                   device_(VK_NULL_HANDLE)
    {
        rig_.host().vk_handle(0, ScriptedHost::vk_device);
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        const char *const enabled[] = {"VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32"};
        info.enabledExtensionCount = 2u;
        info.ppEnabledExtensionNames = enabled;
        CHECK_EQ(rig_.entry<VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *,
                                         const VkAllocationCallbacks *, VkDevice *)>(
                     instance_, "vkCreateDevice")(physical_device_, &info, nullptr, &device_),
                 VK_SUCCESS);
    }
    ~SharingRig()
    {
        if (device_ != VK_NULL_HANDLE)
            rig_.entry<void (*)(VkDevice, const VkAllocationCallbacks *)>(instance_, "vkDestroyDevice")(device_,
                                                                                                        nullptr);
        rig_.destroy_instance(instance_);
    }
    SharingRig(const SharingRig &) = delete;
    SharingRig &operator=(const SharingRig &) = delete;

    IcdRig          &rig() { return rig_; }
    VkDevice         device() const { return device_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }

    template <typename T>
    T entry(const char *name)
    { return rig_.entry<T>(instance_, name); }

    /* A handle for the host's memory, exported through the ICD. */
    void *exported_memory()
    { MemoryGetWin32HandleRecord request = {};
      request.type_ = av::icd::memory_get_win32_handle_info;
      request.memory_ = reinterpret_cast<VkDeviceMemory>(host_memory);
      request.handle_type_ = av::icd::opaque_win32_memory;
      void *handle = nullptr;
      CHECK_EQ(entry<ExportMemory>("vkGetMemoryWin32HandleKHR")(device_, &request, &handle), VK_SUCCESS);
      CHECK(handle != nullptr);
      return handle; }

    void *exported_semaphore()
    { SemaphoreGetWin32HandleRecord request = {};
      request.type_ = av::icd::semaphore_get_win32_handle_info;
      request.semaphore_ = reinterpret_cast<VkSemaphore>(host_semaphore);
      request.handle_type_ = av::icd::opaque_win32_semaphore;
      void *handle = nullptr;
      CHECK_EQ(entry<ExportSemaphore>("vkGetSemaphoreWin32HandleKHR")(device_, &request, &handle), VK_SUCCESS);
      CHECK(handle != nullptr);
      return handle; }

    /* The last command the ICD sent, by id. */
    std::uint32_t last_command()
    { return rig_.kmt().command_id(rig_.kmt().commands() - 1u); }

    /* The body of the last command, past its device handle. */
    WireReader last_body()
    { const std::uint32_t sent = rig_.kmt().commands() - 1u;
      const std::uint8_t *bytes = rig_.kmt().command_bytes(sent);
      WireReader          body(bytes + CommandHeader::encoded_size,
                               MockKmt::journal_bytes - CommandHeader::encoded_size);
      CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
      return body; }

private:
    IcdRig           rig_;
    VkInstance       instance_;
    VkPhysicalDevice physical_device_;
    VkDevice         device_;
};

VkPhysicalDeviceExternalBufferInfo buffer_info(VkExternalMemoryHandleTypeFlagBits type)
{ VkPhysicalDeviceExternalBufferInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
  info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  info.handleType = type;
  return info; }

VkPhysicalDeviceExternalSemaphoreInfo semaphore_info(VkExternalSemaphoreHandleTypeFlagBits type)
{ VkPhysicalDeviceExternalSemaphoreInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
  info.handleType = type;
  return info; }

}  // namespace

/* An export answers a handle without asking the host anything - the fd it
 * shares by is asked for at the import, because a driver takes ownership of
 * one it imports - and an import sends the host its own structure naming that
 * fd. */
TEST(icd_external_handles, a_handle_exported_is_imported_as_a_fresh_fd_of_what_it_names)
{ SharingRig rig;
  const std::uint32_t before = rig.rig().kmt().commands();
  void *const         handle = rig.exported_memory();
  CHECK_EQ(rig.rig().kmt().commands(), before);

  /* The import: the host answers a fresh fd of the memory, then the
   * allocation. */
  rig.rig().host().vk_fd(0, shared_fd);
  rig.rig().host().vk_handle(0, 0x7700u);
  ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = handle;
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &importing;
  allocate.allocationSize = 4096u;
  allocate.memoryTypeIndex = 0u;
  VkDeviceMemory imported = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &imported), VK_SUCCESS);
  CHECK(imported != VK_NULL_HANDLE);

  /* What went on the wire: the host's fd export of the remembered memory, then
   * an allocation whose chain names that fd. */
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkAllocateMemory));
  WireReader body = rig.last_body();
  VkMemoryAllocateInfo sent = {};
  std::uint8_t         block[1024] = {};
  av::WireArena        arena(block, sizeof block);
  CHECK(deserialize_VkMemoryAllocateInfo(body, &sent, arena));
  CHECK_EQ(sent.allocationSize, 4096u);
  const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
  CHECK(node != nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(node->sType),
           static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR));
  const auto *const fd_node = reinterpret_cast<const VkImportMemoryFdInfoKHR *>(node);
  CHECK_EQ(fd_node->fd, shared_fd);
  CHECK_EQ(static_cast<std::uint32_t>(fd_node->handleType),
           static_cast<std::uint32_t>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)); }

/* A semaphore's handle is imported the same way, through the host's own
 * vkImportSemaphoreFdKHR, and a semaphore destroyed takes the handle that
 * named it with it. */
TEST(icd_external_handles, a_semaphore_handle_becomes_the_host_s_own_fd_import)
{ SharingRig  rig;
  void *const handle = rig.exported_semaphore();
  rig.rig().host().vk_fd(0, shared_fd);
  rig.rig().host().vk_result(0);
  ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.flags_ = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
  importing.handle_ = handle;
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing), VK_SUCCESS);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkImportSemaphoreFdKHR));
  WireReader                 body = rig.last_body();
  VkImportSemaphoreFdInfoKHR sent = {};
  std::uint8_t               block[1024] = {};
  av::WireArena              arena(block, sizeof block);
  CHECK(deserialize_VkImportSemaphoreFdInfoKHR(body, &sent, arena));
  CHECK_EQ(sent.fd, shared_fd);
  CHECK_EQ(static_cast<std::uint32_t>(sent.handleType),
           static_cast<std::uint32_t>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT));
  CHECK_EQ(sent.flags, static_cast<VkFlags>(VK_SEMAPHORE_IMPORT_TEMPORARY_BIT));
  CHECK(sent.semaphore == reinterpret_cast<VkSemaphore>(0x8800u));

  /* Destroyed, the handle it was exported for names nothing: the import is
   * refused before the host is asked for an fd of a semaphore that is gone. */
  rig.entry<DestroySemaphore>("vkDestroySemaphore")(rig.device(),
                                                    reinterpret_cast<VkSemaphore>(host_semaphore), nullptr);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkDestroySemaphore)); }

/* A handle this process did not export names nothing, on either path, and a
 * handle from outside has no memory types to answer. */
TEST(icd_external_handles, a_handle_it_did_not_export_is_refused_before_the_host)
{ SharingRig rig;
  ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = reinterpret_cast<void *>(0xDEADu);
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &importing;
  allocate.allocationSize = 4096u;
  const std::uint32_t before = rig.rig().kmt().commands();
  VkDeviceMemory      imported = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &imported),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK(imported == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);

  ImportSemaphoreWin32HandleRecord semaphore = {};
  semaphore.type_ = av::icd::import_semaphore_win32_handle_info;
  semaphore.handle_type_ = av::icd::opaque_win32_semaphore;
  semaphore.handle_ = reinterpret_cast<void *>(0xDEADu);
  semaphore.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &semaphore),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);

  /* The properties of a handle from outside: nothing, because a handle this
   * ICD did not export names no object it could answer for. */
  CHECK_EQ(rig.entry<HandleProperties>("vkGetMemoryWin32HandlePropertiesKHR")(
               rig.device(), av::icd::opaque_win32_memory, reinterpret_cast<void *>(0xDEADu), nullptr),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);

  /* A device this ICD did not make answers nothing on any of them. */
  int      stack = 0;
  VkDevice foreign = reinterpret_cast<VkDevice>(&stack);
  MemoryGetWin32HandleRecord request = {};
  request.type_ = av::icd::memory_get_win32_handle_info;
  request.memory_ = reinterpret_cast<VkDeviceMemory>(host_memory);
  request.handle_type_ = av::icd::opaque_win32_memory;
  void *handle = nullptr;
  CHECK_EQ(rig.entry<ExportMemory>("vkGetMemoryWin32HandleKHR")(foreign, &request, &handle),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  SemaphoreGetWin32HandleRecord getting = {};
  getting.type_ = av::icd::semaphore_get_win32_handle_info;
  getting.semaphore_ = reinterpret_cast<VkSemaphore>(host_semaphore);
  getting.handle_type_ = av::icd::opaque_win32_semaphore;
  CHECK_EQ(rig.entry<ExportSemaphore>("vkGetSemaphoreWin32HandleKHR")(foreign, &getting, &handle),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(foreign, &semaphore),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.entry<HandleProperties>("vkGetMemoryWin32HandlePropertiesKHR")(
               foreign, av::icd::opaque_win32_memory, reinterpret_cast<void *>(0xDEADu), nullptr),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);

  /* And a request that is not there at all names no object either, on every
   * one of the four. */
  av::icd::IcdRoot &root = rig.rig().root();
  CHECK_EQ(root.export_memory_handle(rig.device(), nullptr, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(root.export_semaphore_handle(rig.device(), nullptr, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(root.import_semaphore_handle(rig.device(), nullptr), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(root.memory_handle_properties(rig.device(), nullptr), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);
  /* Destroying a semaphore nobody exported a handle for forgets nothing and
   * still reaches the host, as vkDestroySemaphore must. */
  root.destroy_semaphore(rig.device(), VK_NULL_HANDLE, nullptr);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkDestroySemaphore)); }

/* What the host says of its own opaque fd, said in Win32's spelling; every
 * other Win32 type is answered as absent, because nothing here turns a
 * Direct3D object into a host one. */
TEST(icd_external_handles, the_external_properties_of_a_win32_handle_are_the_host_s_of_an_fd)
{ SharingRig rig;
  rig.rig().host().vk_external_buffer_properties(
      VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
  const VkPhysicalDeviceExternalBufferInfo asked =
      buffer_info(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);
  VkExternalBufferProperties answered = {};
  answered.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
  rig.entry<BufferProperties>("vkGetPhysicalDeviceExternalBufferProperties")(rig.physical_device(), &asked,
                                                                             &answered);
  CHECK_EQ(answered.externalMemoryProperties.externalMemoryFeatures,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT |
                                VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT));
  /* Said in Win32's spelling, and the host's own types dropped: a guest could
   * not name an fd or a dma-buf. */
  CHECK_EQ(answered.externalMemoryProperties.exportFromImportedHandleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK_EQ(answered.externalMemoryProperties.compatibleHandleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkGetPhysicalDeviceExternalBufferProperties));

  /* A KMT handle, and a Direct3D resource: nothing, and the host is not asked
   * about them at all. */
  const std::uint32_t                         after = rig.rig().kmt().commands();
  const VkExternalMemoryHandleTypeFlagBits    others[] = {
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
  for (const VkExternalMemoryHandleTypeFlagBits type : others) {
      const VkPhysicalDeviceExternalBufferInfo other = buffer_info(type);
      VkExternalBufferProperties               absent = {};
      absent.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
      absent.externalMemoryProperties.compatibleHandleTypes = 0xFFFFFFFFu;
      rig.entry<BufferProperties>("vkGetPhysicalDeviceExternalBufferProperties")(rig.physical_device(), &other,
                                                                                 &absent);
      CHECK_EQ(absent.externalMemoryProperties.compatibleHandleTypes, 0u);
      CHECK_EQ(absent.externalMemoryProperties.externalMemoryFeatures, 0u);
  }
  CHECK_EQ(rig.rig().kmt().commands(), after);

  /* The semaphore query is the same story on its own bits. */
  rig.rig().host().vk_external_semaphore_properties(VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT,
                                                    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
                                                    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
  const VkPhysicalDeviceExternalSemaphoreInfo semaphore =
      semaphore_info(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT);
  VkExternalSemaphoreProperties said = {};
  said.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
  rig.entry<SemaphorePropertiesEntry>("vkGetPhysicalDeviceExternalSemaphoreProperties")(rig.physical_device(),
                                                                                        &semaphore, &said);
  CHECK_EQ(said.externalSemaphoreFeatures,
           static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT));
  CHECK_EQ(said.compatibleHandleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK_EQ(said.exportFromImportedHandleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  const VkPhysicalDeviceExternalSemaphoreInfo fence =
      semaphore_info(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT);
  VkExternalSemaphoreProperties nothing = {};
  nothing.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
  nothing.compatibleHandleTypes = 0xFFFFFFFFu;
  rig.entry<SemaphorePropertiesEntry>("vkGetPhysicalDeviceExternalSemaphoreProperties")(rig.physical_device(),
                                                                                        &fence, &nothing);
  CHECK_EQ(nothing.compatibleHandleTypes, 0u);

  /* Neither query answers for a physical device this ICD did not make, and
   * neither reads a request that is not there. */
  int stack = 0;
  VkExternalBufferProperties untouched = {};
  untouched.externalMemoryProperties.compatibleHandleTypes = 0xFFFFFFFFu;
  rig.entry<BufferProperties>("vkGetPhysicalDeviceExternalBufferProperties")(
      reinterpret_cast<VkPhysicalDevice>(&stack), &asked, &untouched);
  CHECK_EQ(untouched.externalMemoryProperties.compatibleHandleTypes, 0xFFFFFFFFu);
  rig.entry<BufferProperties>("vkGetPhysicalDeviceExternalBufferProperties")(rig.physical_device(), nullptr,
                                                                             &untouched);
  rig.entry<BufferProperties>("vkGetPhysicalDeviceExternalBufferProperties")(rig.physical_device(), &asked,
                                                                             nullptr);
  CHECK_EQ(untouched.externalMemoryProperties.compatibleHandleTypes, 0xFFFFFFFFu);
  VkExternalSemaphoreProperties kept = {};
  kept.compatibleHandleTypes = 0xFFFFFFFFu;
  rig.entry<SemaphorePropertiesEntry>("vkGetPhysicalDeviceExternalSemaphoreProperties")(
      reinterpret_cast<VkPhysicalDevice>(&stack), &semaphore, &kept);
  rig.entry<SemaphorePropertiesEntry>("vkGetPhysicalDeviceExternalSemaphoreProperties")(rig.physical_device(),
                                                                                        nullptr, &kept);
  rig.entry<SemaphorePropertiesEntry>("vkGetPhysicalDeviceExternalSemaphoreProperties")(rig.physical_device(),
                                                                                        &semaphore, nullptr);
  CHECK_EQ(kept.compatibleHandleTypes, 0xFFFFFFFFu); }

/* An allocation the application means to share is the host's whole - never a
 * chunk of this process's RAM - and the export goes to the host in the only
 * spelling it understands. A chain this ICD cannot carry whole is refused
 * rather than sent without it. */
TEST(icd_external_handles, an_allocation_made_to_be_shared_is_exported_as_an_fd)
{ SharingRig rig;
  VkExportMemoryAllocateInfo exporting = {};
  exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  VkMemoryDedicatedAllocateInfo dedicated = {};
  dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  exporting.pNext = &dedicated;
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &exporting;
  allocate.allocationSize = 8192u;
  /* A host-visible type, which without the export would have been served from
   * a chunk of this process's RAM: what is shared is the host's own. */
  allocate.memoryTypeIndex = 1u;
  rig.rig().host().vk_handle(0, 0x7700u);
  VkDeviceMemory shared = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &shared), VK_SUCCESS);
  CHECK(shared != VK_NULL_HANDLE);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkAllocateMemory));
  WireReader           body = rig.last_body();
  VkMemoryAllocateInfo sent = {};
  std::uint8_t         block[1024] = {};
  av::WireArena        arena(block, sizeof block);
  CHECK(deserialize_VkMemoryAllocateInfo(body, &sent, arena));
  CHECK_EQ(sent.allocationSize, 8192u);
  const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
  CHECK(node != nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(node->sType),
           static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO));
  CHECK_EQ(reinterpret_cast<const VkExportMemoryAllocateInfo *>(node)->handleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));

  /* A node this ICD does not carry: refused, and nothing sent. */
  VkMemoryOpaqueCaptureAddressAllocateInfo captured = {};
  captured.sType = VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO;
  dedicated.pNext = &captured;
  const std::uint32_t before = rig.rig().kmt().commands();
  VkDeviceMemory      refused = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &refused),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK(refused == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);

  /* An export of a handle type this ICD does not answer is not a shared
   * allocation at all, and goes the way every other allocation goes. */
  dedicated.pNext = nullptr;
  exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
  rig.rig().host().vk_memory_properties(ScriptedHost::vk_host_memory_types, ScriptedHost::vk_host_visible_types);
  rig.rig().host().vk_handle(0, 0x7800u);
  VkDeviceMemory other = VK_NULL_HANDLE;
  allocate.memoryTypeIndex = 0u;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &other), VK_SUCCESS);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkAllocateMemory)); }

/* What a chain longer than this driver reads means, and what a host that
 * refuses the fd leaves: neither is answered with a share that did not
 * happen. */
TEST(icd_external_handles, a_chain_longer_than_it_reads_and_a_host_that_refuses_the_fd)
{ SharingRig rig;
  /* A chain of nodes with no Win32 marker in the part this driver reads is not
   * a shared allocation, and goes the way every other allocation goes. */
  constexpr std::uint32_t            beyond = 33u;
  VkMemoryDedicatedAllocateInfo      many[beyond] = {};
  for (std::uint32_t index = 0u; index < beyond; index++) {
      many[index].sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
      many[index].pNext = index != 0u ? &many[index - 1u] : nullptr;
  }
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &many[beyond - 1u];
  allocate.allocationSize = 4096u;
  rig.rig().host().vk_memory_properties(ScriptedHost::vk_host_memory_types, ScriptedHost::vk_host_visible_types);
  rig.rig().host().vk_handle(0, 0x7900u);
  VkDeviceMemory plain = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &plain), VK_SUCCESS);
  CHECK(plain != VK_NULL_HANDLE);

  /* The Win32 export structure alone still says the allocation is shared -
   * it is what DXVK chains for a named handle - and then the chain is longer
   * than this driver reads, which is refused rather than sent short. */
  struct {
      VkStructureType type_;
      const void     *next_;
      const void     *attributes_;
      VkFlags         access_;
      const void     *name_;
  } win32 = {av::icd::export_memory_win32_handle_info, &many[beyond - 1u], nullptr, 0u, nullptr};
  allocate.pNext = &win32;
  const std::uint32_t before = rig.rig().kmt().commands();
  VkDeviceMemory      refused = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &refused),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK(refused == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);

  /* A host that refuses a fresh fd of the remembered memory: the import is
   * that refusal, not an allocation of memory nothing shares. */
  void *const handle = rig.exported_memory();
  av::icd::ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = handle;
  VkMemoryAllocateInfo sharing = {};
  sharing.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  sharing.pNext = &importing;
  sharing.allocationSize = 4096u;
  rig.rig().host().vk_fd(VK_ERROR_TOO_MANY_OBJECTS, -1);
  VkDeviceMemory none = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &sharing, nullptr, &none),
           VK_ERROR_TOO_MANY_OBJECTS);
  CHECK(none == VK_NULL_HANDLE);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkGetMemoryFdKHR)); }

/* A lost device answers the loss rather than the handle: the import goes
 * nowhere, and neither does a host asked for an fd it will not answer. */
TEST(icd_external_handles, a_lost_device_and_a_refused_semaphore_fd_are_both_the_answer)
{ SharingRig rig;
  void *const handle = rig.exported_semaphore();
  av::icd::ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.handle_ = handle;

  /* The host refuses a fresh fd of the remembered semaphore. */
  rig.rig().host().vk_fd(VK_ERROR_TOO_MANY_OBJECTS, -1);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing),
           VK_ERROR_TOO_MANY_OBJECTS);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkGetSemaphoreFdKHR));

  /* And once the context is faulted, the import is the loss. */
  const std::uint32_t before = rig.rig().kmt().commands();
  rig.rig().kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.entry<VkResult (*)(VkPhysicalDevice)>("vkRvkForceContextFault")(rig.physical_device()),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().kmt().commands(), before + 1u); }

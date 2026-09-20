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
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

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
/* Whole pages, which is what the wire demands of shared memory. */
constexpr std::uint64_t shared_size = 4096u;

/* A device on the scripted host, and the entry points sharing needs. */
class SharingRig {
public:
    /* `host_dma_buf` asks the host for its device extensions first, answering
     * a list that holds VK_EXT_external_memory_dma_buf - which is the one
     * moment the ICD decides what a shared allocation is exported as
     * (feature 053, T031). Without it the host was never asked, and the
     * opaque fd is what every host shared by before 053. */
    explicit SharingRig(bool host_dma_buf = false)
        : instance_(rig_.create_instance()), physical_device_(rig_.physical_device(instance_)),
          device_(VK_NULL_HANDLE)
    {
        /* The device create asks the host for its list before it makes the
         * device, because what a shared allocation is exported as has to be
         * settled first (feature 053). Two answers, always. */
        list_host_extensions(host_dma_buf);
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

    /* The host's own extension list, queued for the one enumeration the create
     * makes: the count, then the names. */
    void list_host_extensions(bool with_dma_buf)
    { const char *const listed[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
      const std::uint32_t count = with_dma_buf ? 2u : 1u;
      rig_.host().vk_count(0, count);
      std::uint8_t answer[MockKmt::max_answer] = {};
      av::WireWriter writer(answer, sizeof answer);
      writer.write_pod<std::int32_t>(0);
      writer.write_pod<std::uint32_t>(count);
      for (std::uint32_t index = 0u; index < count; index++) {
          VkExtensionProperties entry = {};
          std::strncpy(entry.extensionName, listed[index], sizeof entry.extensionName - 1u);
          entry.specVersion = 1u;
          serialize_VkExtensionProperties(writer, entry);
      }
      CHECK_STATUS(writer.status(), av::Status::ok);
      rig_.kmt().queue_answer(answer, static_cast<std::uint32_t>(writer.offset())); }

    IcdRig          &rig() { return rig_; }
    VkDevice         device() const { return device_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }

    template <typename T>
    T entry(const char *name)
    { return rig_.entry<T>(instance_, name); }

    /* Memory an application declared exportable: the host allocates it and the
     * device publishes it, which is where the identity is minted (feature
     * 053). Two answers are scripted because two calls go out. */
    VkDeviceMemory allocate_shared(std::uint64_t bytes = shared_size,
                                   std::uint64_t answered = host_memory)
    { rig_.host().vk_handle(0, answered);
      rig_.host().shared_object(0u, 0u);
      VkExportMemoryAllocateInfo exporting = {};
      exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exporting;
      allocate.allocationSize = bytes;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory made = VK_NULL_HANDLE;
      CHECK_EQ(entry<Allocate>("vkAllocateMemory")(device_, &allocate, nullptr, &made), VK_SUCCESS);
      CHECK(made != VK_NULL_HANDLE);
      return made; }

    /* The NT handle for memory already published: nothing goes on the wire,
     * because the identity and the handle were made at the allocate. */
    void *handle_of(VkDeviceMemory memory)
    { MemoryGetWin32HandleRecord request = {};
      request.type_ = av::icd::memory_get_win32_handle_info;
      request.memory_ = memory;
      request.handle_type_ = av::icd::opaque_win32_memory;
      void *handle = nullptr;
      CHECK_EQ(entry<ExportMemory>("vkGetMemoryWin32HandleKHR")(device_, &request, &handle), VK_SUCCESS);
      CHECK(handle != nullptr);
      return handle; }

    void *exported_memory() { return handle_of(allocate_shared()); }

    /* A handle another process minted: an allocation on a second guest device
     * of the same kernel, shared there, which this ICD never published and so
     * must adopt and open rather than find in its own table (feature 053). */
    void *foreign_handle(std::uint64_t bytes = shared_size)
    { std::uint32_t adapter = 0u;
      std::uint32_t count = 0u;
      CHECK_STATUS(rig_.kmt().enumerate_adapters(&adapter, 1u, count), av::Status::ok);
      std::uint32_t elsewhere = 0u;
      CHECK_STATUS(rig_.kmt().create_device(adapter, elsewhere), av::Status::ok);
      const av::AllocationDescriptor descriptor(av::AllocationKind::client, true, av::SurfaceLayout(),
                                                av::SharedObjectRecord::page_size, av::ScanoutTarget());
      std::uint8_t bytes_of[av::AllocationDescriptor::encoded_size +
                            av::SharedObjectRecord::encoded_size] = {};
      CHECK_STATUS(descriptor.encode(bytes_of, av::AllocationDescriptor::encoded_size), av::Status::ok);
      const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, bytes);
      CHECK_STATUS(asking.encode(bytes_of + av::AllocationDescriptor::encoded_size,
                                 av::SharedObjectRecord::encoded_size),
                   av::Status::ok);
      std::uint32_t allocation = 0u;
      CHECK_STATUS(rig_.kmt().create_shared_allocation(elsewhere, bytes_of, sizeof bytes_of, allocation),
                   av::Status::ok);
      std::uint64_t handle = 0u;
      CHECK_STATUS(rig_.kmt().share_object(elsewhere, allocation, handle), av::Status::ok);
      return reinterpret_cast<void *>(static_cast<std::uintptr_t>(handle)); }

    /* A semaphore is published the first time a handle to it is asked for:
     * nothing in vkCreateSemaphore says it will ever be shared. */
    void *exported_semaphore()
    { rig_.host().shared_object(0u, 0u);
      SemaphoreGetWin32HandleRecord request = {};
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

    /* The body of the command before the last, past its device handle: an
     * allocation that is published sends two, and the one worth reading is the
     * first (feature 053). */
    WireReader body_before_last()
    { const std::uint32_t sent = rig_.kmt().commands() - 2u;
      const std::uint8_t *bytes = rig_.kmt().command_bytes(sent);
      WireReader          body(bytes + CommandHeader::encoded_size,
                               MockKmt::journal_bytes - CommandHeader::encoded_size);
      CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
      return body; }

    /* The body of the last command as it stands. A transport command names no
     * device, unlike every Vulkan one. */
    WireReader last_body_whole()
    { const std::uint32_t sent = rig_.kmt().commands() - 1u;
      const std::uint8_t *bytes = rig_.kmt().command_bytes(sent);
      return WireReader(bytes + CommandHeader::encoded_size,
                        MockKmt::journal_bytes - CommandHeader::encoded_size); }

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
/* An export publishes the memory when it is allocated, so asking for its handle
 * afterwards sends nothing; an import opens the object the handle names and
 * the handle the device answers *is* the VkDeviceMemory (feature 053). */
TEST(icd_external_handles, a_handle_exported_is_opened_by_the_device_on_import)
{ SharingRig            rig;
  const VkDeviceMemory  shared = rig.allocate_shared();
  const std::uint32_t   before = rig.rig().kmt().commands();
  CHECK(rig.handle_of(shared) != nullptr);
  CHECK_EQ(rig.rig().kmt().commands(), before);
  /* The handle another process sent, which this ICD never published. */
  void *const           handle = rig.foreign_handle();

  /* The import: one OPEN_SHARED_OBJECT, answered with the memory the device
   * made of it. Nothing is allocated on the host for it. */
  rig.rig().host().shared_object(0u, 0x7700u);
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
  CHECK(imported == reinterpret_cast<VkDeviceMemory>(0x7700u));

  /* What went on the wire: the open, naming the object and not the handle -
   * an NT handle means nothing to the device. */
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);
  WireReader     body = rig.last_body_whole();
  const std::uint64_t object_id = body.read_u64();
  CHECK(object_id != 0u);
  CHECK_EQ(body.read_u32(), std::uint32_t{av::SharedObjectKind::memory});
  CHECK_EQ(body.read_u32(), 0u);
  CHECK_EQ(body.read_u64(), 0u);
  CHECK_EQ(body.read_u64(), shared_size);

  /* Freed, it is released rather than freed on the host: the device made it
   * and the device destroys it. */
  rig.rig().host().shared_object(0u, 0u);
  rig.entry<void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)>("vkFreeMemory")(
      rig.device(), imported, nullptr);
  CHECK_EQ(rig.last_command(), av::CommandHeader::release_shared_object_id); }

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
   * refused before the host is asked for an fd of a semaphore that is gone.
   * The destroy releases the object first, which is one answer on the wire. */
  rig.rig().host().shared_object(0u, 0u);
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
  rig.rig().host().shared_object(0u, 0u);
  VkDeviceMemory shared = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &shared), VK_SUCCESS);
  CHECK(shared != VK_NULL_HANDLE);
  /* The publish follows the allocation, so the last command is the share and
   * the allocation is the one before it. */
  CHECK_EQ(rig.last_command(), av::CommandHeader::share_object_id);
  WireReader           body = rig.body_before_last();
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

  /* A device that will not open the object: the import is that refusal, not an
   * allocation of memory nothing shares. */
  void *const handle = rig.foreign_handle();
  av::icd::ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = handle;
  VkMemoryAllocateInfo sharing = {};
  sharing.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  sharing.pNext = &importing;
  sharing.allocationSize = shared_size;
  rig.rig().host().shared_object(av::SharedObjectAnswer::mismatched_object, 0u);
  VkDeviceMemory none = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &sharing, nullptr, &none),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK(none == VK_NULL_HANDLE);
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);

  /* An allocation of no size at all shares nothing, and is refused before the
   * kernel is asked for a carrier: the wire has no body for it. */
  sharing.allocationSize = 0u;
  const std::uint32_t quiet = rig.rig().kmt().commands();
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &sharing, nullptr, &none),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), quiet); }

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

  /* The host refuses a fresh fd of the semaphore this process published. */
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

/* Feature 053, T031. A host with VK_EXT_external_memory_dma_buf shares by
 * dma-buf and not by its opaque fd, because a dma-buf is the only kind the
 * *device* can import for another process: vkGetMemoryFdPropertiesKHR refuses
 * an opaque fd, whose memory type is implicit and known only to whoever
 * allocated it. The extension is asked of the host as well, since an export
 * naming that type on a device made without it is refused. */
TEST(icd_external_handles, a_host_with_the_dma_buf_extension_shares_by_it_and_asks_for_it)
{ SharingRig rig(true);
  const VkDeviceMemory shared = rig.allocate_shared();
  CHECK(shared != VK_NULL_HANDLE);
  WireReader           body = rig.body_before_last();
  VkMemoryAllocateInfo sent = {};
  std::uint8_t         block[1024] = {};
  av::WireArena        arena(block, sizeof block);
  CHECK(deserialize_VkMemoryAllocateInfo(body, &sent, arena));
  const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
  CHECK(node != nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(node->sType),
           static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO));
  CHECK_EQ(reinterpret_cast<const VkExportMemoryAllocateInfo *>(node)->handleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT));

  /* And an import of a handle this process published asks the host for a
   * descriptor of the same kind: the two must agree, or the fd names a type
   * the import does not. */
  void *const handle = rig.handle_of(shared);
  av::icd::ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = handle;
  VkMemoryAllocateInfo sharing = {};
  sharing.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  sharing.pNext = &importing;
  sharing.allocationSize = shared_size;
  rig.rig().host().vk_fd(0, shared_fd);
  rig.rig().host().vk_handle(0, 0x7900u);
  VkDeviceMemory imported = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &sharing, nullptr, &imported), VK_SUCCESS);
  WireReader           second = rig.last_body();
  VkMemoryAllocateInfo taken = {};
  std::uint8_t         more[1024] = {};
  av::WireArena        arena_again(more, sizeof more);
  CHECK(deserialize_VkMemoryAllocateInfo(second, &taken, arena_again));
  const auto *const import_node = static_cast<const VkBaseInStructure *>(taken.pNext);
  CHECK(import_node != nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(import_node->sType),
           static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR));
  CHECK_EQ(static_cast<std::uint32_t>(
               reinterpret_cast<const VkImportMemoryFdInfoKHR *>(import_node)->handleType),
           static_cast<std::uint32_t>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)); }

/* A host without it shares by its opaque fd, which is what every host did
 * before 053 and what works inside one process. */
TEST(icd_external_handles, a_host_without_the_dma_buf_extension_shares_by_its_opaque_fd)
{ SharingRig rig;
  CHECK(rig.allocate_shared() != VK_NULL_HANDLE);
  WireReader           body = rig.body_before_last();
  VkMemoryAllocateInfo sent = {};
  std::uint8_t         block[1024] = {};
  av::WireArena        arena(block, sizeof block);
  CHECK(deserialize_VkMemoryAllocateInfo(body, &sent, arena));
  const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
  CHECK(node != nullptr);
  CHECK_EQ(reinterpret_cast<const VkExportMemoryAllocateInfo *>(node)->handleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)); }

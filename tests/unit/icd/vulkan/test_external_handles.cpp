#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/common/opened_objects.hpp"
#include "icd/vulkan/external_objects.hpp"
#include "icd/vulkan/host_extension_list.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
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

HandleIdentity identity_translation;

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
constexpr std::uint64_t shared_size = 4096u;
constexpr std::uint32_t max_host_extensions = 1024u;

std::uint32_t command_back(MockKmt &kmt, std::uint32_t back)
{
    for (std::uint32_t index = kmt.commands(); index-- > 0u;) {
        if (kmt.command_id(index) == CommandHeader::channel_stats_id) continue;
        if (back-- == 0u) return index;
    }
    return 0u;
}

class SharingRig {
public:
    struct HostExtensions {
        std::int32_t  counted;
        std::uint32_t count;
        bool          dma_buf;
        std::int32_t  listed;
    };

    explicit SharingRig(bool host_dma_buf = false)
        : SharingRig(HostExtensions{0, host_dma_buf ? 2u : 1u, host_dma_buf, 0})
    {}

    explicit SharingRig(const HostExtensions &host)
        : instance_(rig_.create_instance()), physical_device_(rig_.physical_device(instance_)),
          device_(VK_NULL_HANDLE)
    {
        list_host_extensions(host);
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

    void list_host_extensions(const HostExtensions &host)
    { const char *const names[] = {"VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf"};
      rig_.host().vk_count(host.counted, host.count);
      if (host.counted != 0 || host.count == 0u || host.count > max_host_extensions) return;
      std::uint8_t   answer[MockKmt::max_answer] = {};
      av::WireWriter writer(answer, sizeof answer);
      writer.write_pod<std::int32_t>(host.listed);
      writer.write_pod<std::uint32_t>(host.count);
      for (std::uint32_t index = 0u; index < host.count; index++) {
          VkExtensionProperties entry = {};
          const char *const     name = host.dma_buf ? names[index % 2u] : names[0];
          std::strncpy(entry.extensionName, name, sizeof entry.extensionName - 1u);
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

    std::uint32_t last_command()
    { return rig_.kmt().command_id(command_back(rig_.kmt(), 0u)); }

    WireReader body_before_last()
    { const std::uint32_t sent = command_back(rig_.kmt(), 1u);
      const std::uint8_t *bytes = rig_.kmt().command_bytes(sent);
      WireReader          body(bytes + CommandHeader::encoded_size,
                               MockKmt::journal_bytes - CommandHeader::encoded_size);
      CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
      return body; }

    WireReader last_body_whole()
    { const std::uint32_t sent = command_back(rig_.kmt(), 0u);
      const std::uint8_t *bytes = rig_.kmt().command_bytes(sent);
      return WireReader(bytes + CommandHeader::encoded_size,
                        MockKmt::journal_bytes - CommandHeader::encoded_size); }

    WireReader last_body()
    { const std::uint32_t sent = command_back(rig_.kmt(), 0u);
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

}

TEST(icd_external_handles, a_handle_exported_is_opened_by_the_device_on_import)
{ SharingRig            rig;
  const VkDeviceMemory  shared = rig.allocate_shared();
  const std::uint32_t   before = rig.rig().kmt().commands();
  CHECK(rig.handle_of(shared) != nullptr);
  CHECK_EQ(rig.rig().kmt().commands(), before);
  void *const           handle = rig.foreign_handle();

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

  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);
  WireReader     body = rig.last_body_whole();
  const std::uint64_t object_id = body.read_u64();
  CHECK(object_id != 0u);
  CHECK_EQ(body.read_u32(), std::uint32_t{av::SharedObjectKind::memory});
  CHECK_EQ(body.read_u32(), 0u);
  CHECK_EQ(body.read_u64(), ScriptedHost::vk_device);
  CHECK_EQ(body.read_u64(), shared_size);

  rig.rig().host().shared_object(0u, 0u);
  rig.entry<void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)>("vkFreeMemory")(
      rig.device(), imported, nullptr);
  CHECK_EQ(rig.last_command(), av::CommandHeader::release_shared_object_id); }

TEST(icd_external_handles, freeing_an_exported_allocation_gives_the_carrier_back_as_a_resource)
{ SharingRig           rig;
  const std::uint32_t  objects = rig.rig().kmt().open_objects();
  const VkDeviceMemory shared = rig.allocate_shared();
  CHECK(rig.handle_of(shared) != nullptr);
  CHECK(rig.rig().kmt().open_objects() > objects);
  rig.rig().host().shared_object(0u, 0u);
  rig.entry<void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)>("vkFreeMemory")(
      rig.device(), shared, nullptr);
  CHECK_EQ(rig.rig().kmt().open_objects(), objects);
  CHECK_EQ(rig.rig().kmt().calls(MockKmt::Call::destroy_shared_allocation), 1u);
  CHECK_EQ(rig.rig().kmt().calls(MockKmt::Call::destroy_allocation), 0u); }

TEST(icd_external_handles, a_semaphore_handle_is_opened_by_the_device_into_the_semaphore_it_names)
{ SharingRig  rig;
  void *const handle = rig.exported_semaphore();
  rig.rig().host().shared_object(0u, 0x8800u);
  ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.flags_ = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
  importing.handle_ = handle;
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing), VK_SUCCESS);
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);
  WireReader          body = rig.last_body_whole();
  CHECK(body.read_u64() != 0u);
  CHECK_EQ(body.read_u32(), std::uint32_t{av::SharedObjectKind::monitored_fence});
  CHECK_EQ(body.read_u32(), 0u);
  CHECK_EQ(body.read_u64(), 0x8800u);
  CHECK_EQ(body.read_u64(), 0u);
  av::icd::OpenedObjects &opened =
      rig.rig().root().connection_of(rig.device(), av::icd::LoaderHandle::Kind::device)->opened_objects();
  CHECK_EQ(opened.live(), 1u);
  CHECK(opened.answered(av::SharedObjectKind::monitored_fence, 0x8800u));

  rig.rig().host().shared_object(0u, 0u);
  rig.rig().host().shared_object(0u, 0x8800u);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing), VK_SUCCESS);
  CHECK_EQ(opened.live(), 1u);
  rig.rig().host().shared_object(0u, 0u);
  rig.entry<DestroySemaphore>("vkDestroySemaphore")(rig.device(), reinterpret_cast<VkSemaphore>(0x8800u), nullptr);
  CHECK_EQ(opened.live(), 0u);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkDestroySemaphore));
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_external_handles, a_semaphore_import_that_answers_another_semaphore_is_given_back)
{ SharingRig  rig;
  void *const handle = rig.exported_semaphore();
  rig.rig().host().shared_object(0u, 0x8801u);
  rig.rig().host().shared_object(0u, 0u);
  ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.handle_ = handle;
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.last_command(), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(rig.rig()
               .root()
               .connection_of(rig.device(), av::icd::LoaderHandle::Kind::device)
               ->opened_objects()
               .live(),
           0u);
  CHECK(rig.rig().root().invariants_hold()); }

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

  CHECK_EQ(rig.entry<HandleProperties>("vkGetMemoryWin32HandlePropertiesKHR")(
               rig.device(), av::icd::opaque_win32_memory, reinterpret_cast<void *>(0xDEADu), nullptr),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);

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

  av::icd::IcdRoot &root = rig.rig().root();
  CHECK_EQ(root.export_memory_handle(rig.device(), nullptr, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(root.export_semaphore_handle(rig.device(), nullptr, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(root.import_semaphore_handle(rig.device(), nullptr), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(root.memory_handle_properties(rig.device(), nullptr), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);
  root.destroy_semaphore(rig.device(), VK_NULL_HANDLE, nullptr);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkDestroySemaphore)); }

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
  CHECK_EQ(answered.externalMemoryProperties.exportFromImportedHandleTypes, 0u);
  CHECK_EQ(answered.externalMemoryProperties.compatibleHandleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkGetPhysicalDeviceExternalBufferProperties));

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
  allocate.memoryTypeIndex = 1u;
  rig.rig().host().vk_handle(0, 0x7700u);
  rig.rig().host().shared_object(0u, 0u);
  VkDeviceMemory shared = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &shared), VK_SUCCESS);
  CHECK(shared != VK_NULL_HANDLE);
  CHECK_EQ(rig.last_command(), av::CommandHeader::share_object_id);
  WireReader           body = rig.body_before_last();
  VkMemoryAllocateInfo sent = {};
  std::uint8_t         block[1024] = {};
  av::WireArena        arena(block, sizeof block);
  CHECK(deserialize_VkMemoryAllocateInfo(body, identity_translation, &sent, arena));
  CHECK_EQ(sent.allocationSize, 8192u);
  const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
  CHECK(node != nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(node->sType),
           static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO));
  CHECK_EQ(reinterpret_cast<const VkExportMemoryAllocateInfo *>(node)->handleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));

  VkMemoryOpaqueCaptureAddressAllocateInfo captured = {};
  captured.sType = VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO;
  dedicated.pNext = &captured;
  const std::uint32_t before = rig.rig().kmt().commands();
  VkDeviceMemory      refused = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &refused),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK(refused == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before);

  dedicated.pNext = nullptr;
  exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
  rig.rig().host().vk_memory_properties(ScriptedHost::vk_host_memory_types, ScriptedHost::vk_host_visible_types);
  rig.rig().host().vk_handle(0, 0x7800u);
  VkDeviceMemory other = VK_NULL_HANDLE;
  allocate.memoryTypeIndex = 0u;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &other), VK_SUCCESS);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkAllocateMemory)); }

TEST(icd_external_handles, a_chain_longer_than_it_reads_and_a_host_that_refuses_the_fd)
{ SharingRig rig;
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

  sharing.allocationSize = 0u;
  const std::uint32_t quiet = rig.rig().kmt().commands();
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &sharing, nullptr, &none),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), quiet); }

TEST(icd_external_handles, a_lost_device_and_a_refused_semaphore_open_are_both_the_answer)
{ SharingRig rig;
  void *const handle = rig.exported_semaphore();
  av::icd::ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.handle_ = handle;

  rig.rig().host().shared_object(av::SharedObjectAnswer::unknown_object, 0u);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);

  const std::uint32_t before = rig.rig().kmt().commands();
  rig.rig().kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.entry<VkResult (*)(VkDevice)>("vkAvForceContextFault")(rig.device()), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().kmt().commands(), before + 1u); }

TEST(icd_external_handles, a_host_with_the_dma_buf_extension_shares_by_it_and_asks_for_it)
{ SharingRig rig(true);
  const VkDeviceMemory shared = rig.allocate_shared();
  CHECK(shared != VK_NULL_HANDLE);
  WireReader           body = rig.body_before_last();
  VkMemoryAllocateInfo sent = {};
  std::uint8_t         block[1024] = {};
  av::WireArena        arena(block, sizeof block);
  CHECK(deserialize_VkMemoryAllocateInfo(body, identity_translation, &sent, arena));
  const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
  CHECK(node != nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(node->sType),
           static_cast<std::uint32_t>(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO));
  CHECK_EQ(reinterpret_cast<const VkExportMemoryAllocateInfo *>(node)->handleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT));

  void *const handle = rig.handle_of(shared);
  av::icd::ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = handle;
  VkMemoryAllocateInfo sharing = {};
  sharing.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  sharing.pNext = &importing;
  sharing.allocationSize = shared_size;
  rig.rig().host().shared_object(0u, 0x7900u);
  const std::uint32_t before = rig.rig().kmt().commands();
  VkDeviceMemory imported = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &sharing, nullptr, &imported), VK_SUCCESS);
  CHECK(imported == reinterpret_cast<VkDeviceMemory>(0x7900u));
  CHECK(rig.rig().kmt().commands() > before);
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id); }

TEST(icd_external_handles, a_host_without_the_dma_buf_extension_shares_by_its_opaque_fd)
{ SharingRig rig;
  CHECK(rig.allocate_shared() != VK_NULL_HANDLE);
  WireReader           body = rig.body_before_last();
  VkMemoryAllocateInfo sent = {};
  std::uint8_t         block[1024] = {};
  av::WireArena        arena(block, sizeof block);
  CHECK(deserialize_VkMemoryAllocateInfo(body, identity_translation, &sent, arena));
  const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
  CHECK(node != nullptr);
  CHECK_EQ(reinterpret_cast<const VkExportMemoryAllocateInfo *>(node)->handleTypes,
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)); }

TEST(icd_external_handles, the_drivers_last_kernel_refusal_is_readable_from_outside)
{ SharingRig    rig;
  std::uint32_t status = 9u;
  CHECK_EQ(rig.rig().root().last_kernel_status(rig.device(), &status), VK_SUCCESS);
  CHECK_EQ(status, 0u);

  rig.rig().kmt().fail_at(MockKmt::Call::create_shared_allocation, 1u, MockKmt::invalid_parameter);
  rig.rig().host().vk_handle(0, host_memory);
  VkExportMemoryAllocateInfo exporting = {};
  exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &exporting;
  allocate.allocationSize = shared_size;
  VkDeviceMemory refused = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &refused),
           VK_ERROR_TOO_MANY_OBJECTS);
  CHECK(rig.rig().kmt().last_code() != MockKmt::invalid_parameter);
  CHECK_EQ(rig.rig().root().last_kernel_status(rig.device(), &status), VK_SUCCESS);
  CHECK_EQ(status, MockKmt::invalid_parameter);

  CHECK(rig.rig().root().last_kernel_status(rig.device(), nullptr) != VK_SUCCESS);
  CHECK(rig.rig().root().last_kernel_status(reinterpret_cast<VkDevice>(0xDEADu), &status) != VK_SUCCESS);
  CHECK_EQ(status, 0u);
  CHECK(rig.rig().root().last_kernel_status(reinterpret_cast<VkDevice>(rig.physical_device()), &status) !=
        VK_SUCCESS);

  using LastStatus = VkResult (*)(VkDevice, std::uint32_t *);
  const auto entry = reinterpret_cast<LastStatus>(rig.rig().root().device_proc_addr(rig.device(), "vkAvLastKernelStatus"));
  CHECK(entry != nullptr);
  CHECK(rig.rig().root().physical_device_proc_addr("vkAvLastKernelStatus") == nullptr);
  CHECK_EQ(entry(rig.device(), &status), VK_SUCCESS);
  CHECK_EQ(status, MockKmt::invalid_parameter); }

TEST(icd_external_handles, a_malformed_export_request_is_refused_on_each_of_its_parts)
{ SharingRig rig;
  av::icd::IcdRoot   &root = rig.rig().root();
  const std::uint32_t before = rig.rig().kmt().commands();
  void               *handle = nullptr;

  MemoryGetWin32HandleRecord memory = {};
  memory.type_ = av::icd::memory_get_win32_handle_info;
  memory.memory_ = reinterpret_cast<VkDeviceMemory>(host_memory);
  memory.handle_type_ = av::icd::opaque_win32_memory;
  memory.type_ = av::icd::semaphore_get_win32_handle_info;
  CHECK_EQ(root.export_memory_handle(rig.device(), &memory, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  memory.type_ = av::icd::memory_get_win32_handle_info;
  memory.handle_type_ = static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT);
  CHECK_EQ(root.export_memory_handle(rig.device(), &memory, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  memory.handle_type_ = av::icd::opaque_win32_memory;
  CHECK_EQ(root.export_memory_handle(rig.device(), &memory, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);

  SemaphoreGetWin32HandleRecord semaphore = {};
  semaphore.semaphore_ = reinterpret_cast<VkSemaphore>(host_semaphore);
  semaphore.handle_type_ = av::icd::opaque_win32_semaphore;
  semaphore.type_ = av::icd::memory_get_win32_handle_info;
  CHECK_EQ(root.export_semaphore_handle(rig.device(), &semaphore, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  semaphore.type_ = av::icd::semaphore_get_win32_handle_info;
  semaphore.handle_type_ = static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT);
  CHECK_EQ(root.export_semaphore_handle(rig.device(), &semaphore, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  semaphore.handle_type_ = av::icd::opaque_win32_semaphore;
  semaphore.semaphore_ = VK_NULL_HANDLE;
  CHECK_EQ(root.export_semaphore_handle(rig.device(), &semaphore, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before); }

TEST(icd_external_handles, a_semaphore_is_published_once_and_the_second_ask_sends_nothing)
{ SharingRig rig;
  void *const         first = rig.exported_semaphore();
  const std::uint32_t after = rig.rig().kmt().commands();
  SemaphoreGetWin32HandleRecord request = {};
  request.type_ = av::icd::semaphore_get_win32_handle_info;
  request.semaphore_ = reinterpret_cast<VkSemaphore>(host_semaphore);
  request.handle_type_ = av::icd::opaque_win32_semaphore;
  void *second = nullptr;
  CHECK_EQ(rig.entry<ExportSemaphore>("vkGetSemaphoreWin32HandleKHR")(rig.device(), &request, &second),
           VK_SUCCESS);
  CHECK(second == first);
  CHECK_EQ(rig.rig().kmt().commands(), after); }

TEST(icd_external_handles, a_malformed_semaphore_import_is_refused_on_each_of_its_parts)
{ SharingRig rig;
  av::icd::IcdRoot &root = rig.rig().root();
  void *const       exported = rig.exported_semaphore();
  ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.handle_ = exported;
  importing.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  int stack = 0;
  importing.name_ = &stack;
  CHECK_EQ(root.import_semaphore_handle(rig.device(), &importing), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  importing.name_ = nullptr;
  importing.type_ = av::icd::import_memory_win32_handle_info;
  CHECK_EQ(root.import_semaphore_handle(rig.device(), &importing), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.handle_type_ = static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT);
  CHECK_EQ(root.import_semaphore_handle(rig.device(), &importing), VK_ERROR_INVALID_EXTERNAL_HANDLE); }

TEST(icd_external_handles, a_semaphore_import_of_memory_or_of_another_process_is_refused)
{ SharingRig rig;
  av::icd::IcdRoot                &root = rig.rig().root();
  ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  importing.handle_ = rig.exported_memory();
  rig.rig().host().shared_object(av::SharedObjectAnswer::mismatched_object, 0u);
  CHECK_EQ(root.import_semaphore_handle(rig.device(), &importing), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);
  importing.handle_ = rig.foreign_handle();
  rig.rig().host().shared_object(av::SharedObjectAnswer::mismatched_object, 0u);
  CHECK_EQ(root.import_semaphore_handle(rig.device(), &importing), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);
  CHECK_EQ(root.connection_of(rig.device(), av::icd::LoaderHandle::Kind::device)->opened_objects().live(), 0u); }

TEST(icd_external_handles, destroying_a_semaphore_of_a_foreign_device_sends_nothing)
{ SharingRig rig;
  void *const         held = rig.exported_semaphore();
  const std::uint32_t before = rig.rig().kmt().commands();
  int                 stack = 0;
  av::icd::IcdRoot   &root = rig.rig().root();
  root.destroy_semaphore(reinterpret_cast<VkDevice>(&stack), reinterpret_cast<VkSemaphore>(host_semaphore),
                         nullptr);
  CHECK_EQ(rig.rig().kmt().commands(), before);
  MemoryGetWin32HandleRecord unused = {};
  static_cast<void>(unused);
  CHECK(held != nullptr);
  SemaphoreGetWin32HandleRecord asking = {};
  asking.type_ = av::icd::semaphore_get_win32_handle_info;
  asking.semaphore_ = reinterpret_cast<VkSemaphore>(host_semaphore);
  asking.handle_type_ = av::icd::opaque_win32_semaphore;
  void *again = nullptr;
  CHECK_EQ(rig.entry<ExportSemaphore>("vkGetSemaphoreWin32HandleKHR")(rig.device(), &asking, &again),
           VK_SUCCESS);
  CHECK(again == held); }

TEST(icd_external_handles, each_way_a_publish_can_fail_gives_the_carrier_back)
{ VkExportMemoryAllocateInfo exporting = {};
  exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &exporting;
  allocate.allocationSize = shared_size;
  allocate.memoryTypeIndex = 0u;

  {
    SharingRig           rig;
    VkMemoryAllocateInfo huge = allocate;
    huge.allocationSize = ~std::uint64_t{0};
    const std::uint32_t  before = rig.rig().kmt().commands();
    VkDeviceMemory       memory = VK_NULL_HANDLE;
    CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &huge, nullptr, &memory),
             VK_ERROR_INVALID_EXTERNAL_HANDLE);
    CHECK(memory == VK_NULL_HANDLE);
    CHECK_EQ(rig.rig().kmt().commands(), before); }

  {
    SharingRig          rig;
    const std::uint32_t objects = rig.rig().kmt().open_objects();
    rig.rig().host().vk_handle(0, 0u);
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    CHECK(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory) != VK_SUCCESS);
    CHECK_EQ(rig.rig().kmt().open_objects(), objects); }

  {
    SharingRig          rig;
    rig.rig().host().vk_handle(0, host_memory);
    const std::uint32_t objects = rig.rig().kmt().open_objects();
    rig.rig().kmt().zero_next(MockKmt::Call::create_shared_allocation);
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
             VK_ERROR_INVALID_EXTERNAL_HANDLE);
    CHECK(memory == VK_NULL_HANDLE);
    CHECK_EQ(rig.rig().kmt().open_objects(), objects); }

  {
    SharingRig          rig;
    rig.rig().host().vk_handle(0, host_memory);
    const std::uint32_t objects = rig.rig().kmt().open_objects();
    rig.rig().kmt().scribble_the_next_record();
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
             VK_ERROR_INVALID_EXTERNAL_HANDLE);
    CHECK(memory == VK_NULL_HANDLE);
    CHECK_EQ(rig.rig().kmt().open_objects(), objects); }

  {
    SharingRig          rig;
    rig.rig().host().vk_handle(0, host_memory);
    const std::uint32_t objects = rig.rig().kmt().open_objects();
    rig.rig().host().shared_object(1u, 0u);
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
             VK_ERROR_TOO_MANY_OBJECTS);
    CHECK(memory == VK_NULL_HANDLE);
    CHECK_EQ(rig.rig().kmt().open_objects(), objects); }

  {
    SharingRig          rig;
    rig.rig().host().vk_handle(0, host_memory);
    rig.rig().host().shared_object(0u, 0u);
    const std::uint32_t objects = rig.rig().kmt().open_objects();
    rig.rig().kmt().fail_at(MockKmt::Call::share_object, 1u, 0xC000009Au);
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
             VK_ERROR_TOO_MANY_OBJECTS);
    CHECK(memory == VK_NULL_HANDLE);
    CHECK_EQ(rig.rig().kmt().open_objects(), objects);
    CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkFreeMemory)); } }

TEST(icd_external_handles, no_memory_anywhere_in_a_shared_allocation_gives_back_what_was_made_for_it)
{ bool refused_somewhere = false;
  bool succeeded_somewhere = false;
  for (std::uint32_t failing = 1u; failing <= 12u; failing++) {
      SharingRig rig;
      rig.rig().host().vk_handle(0, host_memory);
      rig.rig().host().shared_object(0u, 0u);
      const std::uint32_t objects = rig.rig().kmt().open_objects();
      VkExportMemoryAllocateInfo exporting = {};
      exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exporting;
      allocate.allocationSize = shared_size;
      allocate.memoryTypeIndex = 0u;
      VkDeviceMemory memory = VK_NULL_HANDLE;
      rig.rig().platform().fail_allocation_at(failing);
      const VkResult result = rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory);
      rig.rig().platform().fail_allocation_at(0u);
      if (result == VK_SUCCESS) {
          succeeded_somewhere = true;
          CHECK(memory != VK_NULL_HANDLE);
          CHECK(rig.rig().root().invariants_hold());
          continue;
      }
      refused_somewhere = true;
      CHECK_EQ(result, VK_ERROR_OUT_OF_HOST_MEMORY);
      CHECK(memory == VK_NULL_HANDLE);
      CHECK_EQ(rig.rig().kmt().open_objects(), objects);
      CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(kCmd_vkFreeMemory));
      CHECK(rig.rig().root().invariants_hold());
  }
  CHECK(refused_somewhere);
  CHECK(succeeded_somewhere); }

TEST(icd_external_handles, no_memory_anywhere_in_an_import_keeps_nothing_and_answers_out_of_host_memory)
{ bool refused_somewhere = false;
  for (std::uint32_t failing = 1u; failing <= 12u; failing++) {
      SharingRig  rig;
      void *const handle = rig.foreign_handle();
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
      rig.rig().platform().fail_allocation_at(failing);
      const VkResult result = rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &imported);
      rig.rig().platform().fail_allocation_at(0u);
      if (result == VK_SUCCESS) {
          CHECK(imported != VK_NULL_HANDLE);
          continue;
      }
      refused_somewhere = true;
      CHECK_EQ(result, VK_ERROR_OUT_OF_HOST_MEMORY);
      CHECK(imported == VK_NULL_HANDLE);
      CHECK(rig.rig().root().invariants_hold());
  }
  CHECK(refused_somewhere); }

TEST(icd_external_handles, no_memory_to_keep_a_win32_handle_is_logged_and_sends_nothing)
{ SharingRig          rig;
  void *const         handle = rig.foreign_handle();
  const std::uint32_t sent = rig.rig().kmt().commands();
  rig.rig().platform().fail_allocation_at(1u);
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
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &imported),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(imported == VK_NULL_HANDLE);
  ImportSemaphoreWin32HandleRecord fence = {};
  fence.type_ = av::icd::import_semaphore_win32_handle_info;
  fence.handle_type_ = av::icd::opaque_win32_semaphore;
  fence.handle_ = handle;
  fence.semaphore_ = reinterpret_cast<VkSemaphore>(0x8800u);
  rig.rig().platform().fail_allocation_at(1u);
  CHECK_EQ(rig.rig().root().import_semaphore_handle(rig.device(), &fence), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.rig().kmt().commands(), sent);
  CHECK_EQ(rig.rig().log().holding("avvk: no memory to keep a Win32 handle"), 2u); }

TEST(icd_external_handles, a_host_that_cannot_be_asked_what_it_shares_by_keeps_the_opaque_fd)
{ const SharingRig::HostExtensions cases[] = {
      {VK_ERROR_OUT_OF_HOST_MEMORY, 2u, true, 0},
      {0, 0u, true, 0},
      {0, max_host_extensions + 1u, true, 0},
      {0, 2u, true, VK_ERROR_OUT_OF_HOST_MEMORY},
      {0, 2u, false, VK_INCOMPLETE},
  };
  for (const SharingRig::HostExtensions &host : cases) {
      SharingRig rig(host);
      VkExportMemoryAllocateInfo exporting = {};
      exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &exporting;
      allocate.allocationSize = shared_size;
      allocate.memoryTypeIndex = 0u;
      rig.rig().host().vk_handle(0, host_memory);
      rig.rig().host().shared_object(0u, 0u);
      VkDeviceMemory memory = VK_NULL_HANDLE;
      CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory), VK_SUCCESS);
      WireReader           body = rig.body_before_last();
      VkMemoryAllocateInfo sent = {};
      std::uint8_t         block[1024] = {};
      av::WireArena        arena(block, sizeof block);
      CHECK(deserialize_VkMemoryAllocateInfo(body, identity_translation, &sent, arena));
      const auto *const node = static_cast<const VkBaseInStructure *>(sent.pNext);
      CHECK(node != nullptr);
      CHECK_EQ(reinterpret_cast<const VkExportMemoryAllocateInfo *>(node)->handleTypes,
               static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));
  } }

TEST(icd_external_handles, a_malformed_memory_import_is_refused_on_each_of_its_parts)
{ SharingRig rig;
  void *const         exported = rig.exported_memory();
  const std::uint32_t before = rig.rig().kmt().commands();
  ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = exported;
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &importing;
  allocate.allocationSize = shared_size;
  int stack = 0;
  importing.name_ = &stack;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  importing.name_ = nullptr;
  importing.handle_type_ = static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT);
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK(memory == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), before); }

TEST(icd_external_handles, an_export_with_nowhere_to_answer_is_refused_before_anything_else)
{ SharingRig rig;
  av::icd::IcdRoot &root = rig.rig().root();
  MemoryGetWin32HandleRecord memory = {};
  memory.type_ = av::icd::memory_get_win32_handle_info;
  memory.memory_ = reinterpret_cast<VkDeviceMemory>(host_memory);
  memory.handle_type_ = av::icd::opaque_win32_memory;
  CHECK_EQ(root.export_memory_handle(rig.device(), &memory, nullptr), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  SemaphoreGetWin32HandleRecord semaphore = {};
  semaphore.type_ = av::icd::semaphore_get_win32_handle_info;
  semaphore.semaphore_ = reinterpret_cast<VkSemaphore>(host_semaphore);
  semaphore.handle_type_ = av::icd::opaque_win32_semaphore;
  CHECK_EQ(root.export_semaphore_handle(rig.device(), &semaphore, nullptr), VK_ERROR_INVALID_EXTERNAL_HANDLE); }

TEST(icd_external_handles, a_memory_import_of_a_semaphores_handle_goes_to_the_device_and_is_refused)
{ SharingRig rig;
  ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = rig.exported_semaphore();
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &importing;
  allocate.allocationSize = shared_size;
  rig.rig().host().shared_object(1u, 0u);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK(memory == VK_NULL_HANDLE);
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id); }

TEST(icd_external_handles, a_host_that_refuses_an_exportable_allocation_mints_no_identity)
{ SharingRig rig;
  VkExportMemoryAllocateInfo exporting = {};
  exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &exporting;
  allocate.allocationSize = shared_size;
  const std::uint32_t objects = rig.rig().kmt().open_objects();
  rig.rig().host().vk_handle(VK_ERROR_OUT_OF_DEVICE_MEMORY, 0u);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
           VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK(memory == VK_NULL_HANDLE);
  CHECK_EQ(rig.rig().kmt().open_objects(), objects); }

TEST(icd_external_handles, a_semaphore_export_says_a_lost_device_and_a_refused_publish_apart)
{ SemaphoreGetWin32HandleRecord request = {};
  request.type_ = av::icd::semaphore_get_win32_handle_info;
  request.semaphore_ = reinterpret_cast<VkSemaphore>(host_semaphore);
  request.handle_type_ = av::icd::opaque_win32_semaphore;

  {
    SharingRig rig;
    rig.rig().kmt().fail_at(MockKmt::Call::create_shared_allocation, 1u, 0xC000009Au);
    void *handle = nullptr;
    CHECK_EQ(rig.entry<ExportSemaphore>("vkGetSemaphoreWin32HandleKHR")(rig.device(), &request, &handle),
             VK_ERROR_TOO_MANY_OBJECTS);
    CHECK(handle == nullptr); }

  {
    SharingRig rig;
    rig.rig().kmt().withhold_answer_at(1u);
    using GetFenceStatus = VkResult (*)(VkDevice, VkFence);
    CHECK_EQ(rig.entry<GetFenceStatus>("vkGetFenceStatus")(rig.device(), reinterpret_cast<VkFence>(0x99u)),
             VK_ERROR_DEVICE_LOST);
    const std::uint32_t before = rig.rig().kmt().commands();
    void               *handle = nullptr;
    CHECK_EQ(rig.entry<ExportSemaphore>("vkGetSemaphoreWin32HandleKHR")(rig.device(), &request, &handle),
             VK_ERROR_DEVICE_LOST);
    CHECK(handle == nullptr);
    CHECK_EQ(rig.rig().kmt().commands(), before); } }

TEST(icd_external_handles, an_import_the_device_refuses_or_finds_already_opened_is_refused_and_keeps_nothing)
{ const std::uint32_t answers[] = {av::SharedObjectAnswer::unknown_object, av::SharedObjectAnswer::mismatched_object,
                                   av::SharedObjectAnswer::conflict};
  for (const std::uint32_t answer : answers) {
      SharingRig rig;
      ImportMemoryWin32HandleRecord importing = {};
      importing.type_ = av::icd::import_memory_win32_handle_info;
      importing.handle_type_ = av::icd::opaque_win32_memory;
      importing.handle_ = rig.exported_memory();
      VkMemoryAllocateInfo allocate = {};
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = &importing;
      allocate.allocationSize = shared_size;
      rig.rig().host().shared_object(answer, 0u);
      VkDeviceMemory memory = VK_NULL_HANDLE;
      CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory),
               VK_ERROR_INVALID_EXTERNAL_HANDLE);
      CHECK(memory == VK_NULL_HANDLE);
      CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);
      CHECK_EQ(rig.rig()
                   .root()
                   .connection_of(rig.device(), av::icd::LoaderHandle::Kind::device)
                   ->opened_objects()
                   .live(),
               0u);
      CHECK(rig.rig().root().invariants_hold());
  } }

TEST(icd_external_handles, an_import_the_device_opens_is_kept_under_the_name_it_answered_and_given_back_at_free)
{ SharingRig rig;
  ImportMemoryWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_memory_win32_handle_info;
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.handle_ = rig.exported_memory();
  VkMemoryAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate.pNext = &importing;
  allocate.allocationSize = shared_size;
  rig.rig().host().shared_object(0u, 0x7900u);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<Allocate>("vkAllocateMemory")(rig.device(), &allocate, nullptr, &memory), VK_SUCCESS);
  CHECK(memory == reinterpret_cast<VkDeviceMemory>(0x7900u));
  CHECK_EQ(rig.last_command(), av::CommandHeader::open_shared_object_id);
  av::icd::OpenedObjects &opened =
      rig.rig().root().connection_of(rig.device(), av::icd::LoaderHandle::Kind::device)->opened_objects();
  CHECK_EQ(opened.live(), 1u);
  CHECK(opened.answered(av::SharedObjectKind::memory, 0x7900u));
  rig.rig().host().shared_object(0u, 0u);
  rig.entry<void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)>("vkFreeMemory")(rig.device(), memory,
                                                                                                nullptr);
  CHECK_EQ(opened.live(), 0u);
  CHECK_EQ(rig.last_command(), av::CommandHeader::release_shared_object_id);
  CHECK(rig.rig().root().invariants_hold()); }


TEST(icd_external_handles, a_semaphore_import_naming_no_semaphore_is_an_invalid_handle_and_sends_nothing)
{ SharingRig  rig;
  void *const handle = rig.exported_semaphore();
  ImportSemaphoreWin32HandleRecord importing = {};
  importing.type_ = av::icd::import_semaphore_win32_handle_info;
  importing.semaphore_ = VK_NULL_HANDLE;
  importing.handle_type_ = av::icd::opaque_win32_semaphore;
  importing.flags_ = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
  importing.handle_ = handle;
  const std::uint32_t sent = rig.rig().kmt().commands();
  CHECK_EQ(rig.entry<ImportSemaphore>("vkImportSemaphoreWin32HandleKHR")(rig.device(), &importing),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(rig.rig().kmt().commands(), sent);
  CHECK(rig.rig().root().invariants_hold()); }

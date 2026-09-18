/* The Win32 handles this process exported, and what an import may follow
 * (feature 047, US3, contracts/external_objects.md).
 *
 * A handle here is the ICD's own, so every refusal matters: a handle this
 * process did not export, a named one - a name is what another process would
 * open - and a handle type this path does not answer. The records the
 * application fills in are Windows structures portable code cannot declare, so
 * they are read as layouts, and their fields are hostile.
 */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vulkan/external_objects.hpp"

#include <cstdint>

namespace av::icd {

/* The states no export can leave a slot in, which is what makes each clause of
 * the invariant worth stating. */
struct ExternalObjectsProbe {
    static ExternalObjectSlot &slot(ExternalObjects &objects, std::uint32_t index)
    {
        return objects.slots_[index];
    }
    static void both_kinds_at_once(ExternalObjects &objects)
    {
        slot(objects, 0u).semaphore_ = reinterpret_cast<VkSemaphore>(0x9999u);
    }
    static void neither_kind(ExternalObjects &objects) { slot(objects, 0u).memory_ = VK_NULL_HANDLE; }
    static void an_object_in_an_empty_slot(ExternalObjects &objects)
    {
        slot(objects, 1u).memory_ = reinterpret_cast<VkDeviceMemory>(0x9998u);
    }
    static void a_semaphore_in_an_empty_slot(ExternalObjects &objects)
    {
        slot(objects, 1u).semaphore_ = reinterpret_cast<VkSemaphore>(0x9997u);
    }
    static void the_same_memory_twice(ExternalObjects &objects) { slot(objects, 1u) = slot(objects, 0u); }
};

}  // namespace av::icd

using av::LockTrace;
using av::RecordingThreadLock;
using av::icd::ExternalObjects;
using av::icd::ExternalObjectsProbe;
using av::icd::ImportMemoryWin32HandleRecord;
using av::icd::ImportSemaphoreWin32HandleRecord;
using av::icd::MemoryGetWin32HandleRecord;
using av::icd::SemaphoreGetWin32HandleRecord;

namespace {

VkDevice const       device = reinterpret_cast<VkDevice>(0x9010u);
VkDevice const       second_device = reinterpret_cast<VkDevice>(0x9020u);
VkDeviceMemory const memory = reinterpret_cast<VkDeviceMemory>(0x9110u);
VkDeviceMemory const other_memory = reinterpret_cast<VkDeviceMemory>(0x9120u);
VkSemaphore const    semaphore = reinterpret_cast<VkSemaphore>(0x9210u);

MemoryGetWin32HandleRecord memory_export(VkDeviceMemory which)
{
    MemoryGetWin32HandleRecord request = {};
    request.type_ = av::icd::memory_get_win32_handle_info;
    request.memory_ = which;
    request.handle_type_ = av::icd::opaque_win32_memory;
    return request;
}

ImportMemoryWin32HandleRecord memory_import(void *handle)
{
    ImportMemoryWin32HandleRecord request = {};
    request.type_ = av::icd::import_memory_win32_handle_info;
    request.handle_type_ = av::icd::opaque_win32_memory;
    request.handle_ = handle;
    return request;
}

SemaphoreGetWin32HandleRecord semaphore_export(VkSemaphore which)
{
    SemaphoreGetWin32HandleRecord request = {};
    request.type_ = av::icd::semaphore_get_win32_handle_info;
    request.semaphore_ = which;
    request.handle_type_ = av::icd::opaque_win32_semaphore;
    return request;
}

ImportSemaphoreWin32HandleRecord semaphore_import(void *handle)
{
    ImportSemaphoreWin32HandleRecord request = {};
    request.type_ = av::icd::import_semaphore_win32_handle_info;
    request.handle_type_ = av::icd::opaque_win32_semaphore;
    request.handle_ = handle;
    return request;
}

class Exported {
public:
    Exported() : lock_(trace_, 'X'), objects_(lock_) {}
    Exported(const Exported &) = delete;
    Exported &operator=(const Exported &) = delete;

    ExternalObjects     &objects() { return objects_; }
    RecordingThreadLock &lock() { return lock_; }

private:
    LockTrace           trace_;
    RecordingThreadLock lock_;
    ExternalObjects     objects_;
};

}  // namespace

/* US3 scenario 1, the guest half: a handle exported for one device is the
 * memory another device in the same process imports. */
TEST(external_objects, a_handle_exported_names_the_memory_an_import_follows)
{ Exported held;
  CHECK(held.objects().invariants_hold());
  CHECK_EQ(held.objects().live(), 0u);
  void *handle = nullptr;
  const MemoryGetWin32HandleRecord request = memory_export(memory);
  CHECK_EQ(held.objects().export_memory(device, &request, &handle), VK_SUCCESS);
  CHECK(handle != nullptr);
  CHECK_EQ(held.objects().live(), 1u);
  CHECK(held.objects().invariants_hold());
  /* The second device imports it and gets the same memory. */
  const ImportMemoryWin32HandleRecord importing = memory_import(handle);
  CHECK(held.objects().imported_memory(&importing) == memory);
  /* The same memory exported again is the same handle: one object, one
   * handle, as Windows has it. */
  void *again = nullptr;
  CHECK_EQ(held.objects().export_memory(second_device, &request, &again), VK_SUCCESS);
  CHECK(again == handle);
  CHECK_EQ(held.objects().live(), 1u);
  /* Every answer took the lock and gave it back. */
  CHECK(!held.lock().held());
  CHECK(held.lock().acquisitions() > 0u); }

/* A semaphore is the same story, and the two kinds do not answer for each
 * other: a memory handle imported as a semaphore is nothing. */
TEST(external_objects, a_semaphore_handle_is_a_semaphore_and_not_a_memory)
{ Exported held;
  void    *memory_handle = nullptr;
  void    *semaphore_handle = nullptr;
  const MemoryGetWin32HandleRecord    exporting_memory = memory_export(memory);
  const SemaphoreGetWin32HandleRecord exporting_semaphore = semaphore_export(semaphore);
  CHECK_EQ(held.objects().export_memory(device, &exporting_memory, &memory_handle), VK_SUCCESS);
  CHECK_EQ(held.objects().export_semaphore(device, &exporting_semaphore, &semaphore_handle), VK_SUCCESS);
  CHECK(memory_handle != semaphore_handle);
  CHECK_EQ(held.objects().live(), 2u);
  const ImportSemaphoreWin32HandleRecord importing_semaphore = semaphore_import(semaphore_handle);
  CHECK(held.objects().imported_semaphore(&importing_semaphore) == semaphore);
  const ImportMemoryWin32HandleRecord importing_memory = memory_import(memory_handle);
  CHECK(held.objects().imported_memory(&importing_memory) == memory);
  /* Crossed: a semaphore's handle names no memory, and the other way round. */
  const ImportMemoryWin32HandleRecord memory_from_semaphore = memory_import(semaphore_handle);
  CHECK(held.objects().imported_memory(&memory_from_semaphore) == VK_NULL_HANDLE);
  const ImportSemaphoreWin32HandleRecord semaphore_from_memory = semaphore_import(memory_handle);
  CHECK(held.objects().imported_semaphore(&semaphore_from_memory) == VK_NULL_HANDLE);
  CHECK(held.objects().invariants_hold()); }

/* Every refusal of contracts/external_objects.md, on the export side. */
TEST(external_objects, an_export_it_cannot_answer_is_refused)
{ Exported held;
  void    *handle = reinterpret_cast<void *>(1);
  MemoryGetWin32HandleRecord request = memory_export(memory);
  /* A record that is not one: another structure's type in the same place. */
  request.type_ = static_cast<VkStructureType>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  CHECK_EQ(held.objects().export_memory(device, &request, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  /* A type field that is not a structure type at all. */
  request.type_ = static_cast<VkStructureType>(0x7FFFFFFF);
  CHECK_EQ(held.objects().export_memory(device, &request, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  request = memory_export(memory);
  /* Handle types this path does not answer: the KMT one, a D3D texture, none
   * at all, and two at once. */
  const VkFlags refused[] = {0x00000004u, 0x00000008u, 0u,
                             av::icd::opaque_win32_memory | 0x00000004u, 0xFFFFFFFFu};
  for (const VkFlags type : refused) {
      request.handle_type_ = type;
      CHECK_EQ(held.objects().export_memory(device, &request, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  }
  request.handle_type_ = av::icd::opaque_win32_memory;
  /* Nothing to export, nowhere to put the handle, no device. */
  request.memory_ = VK_NULL_HANDLE;
  CHECK_EQ(held.objects().export_memory(device, &request, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  request.memory_ = memory;
  CHECK_EQ(held.objects().export_memory(device, &request, nullptr), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(held.objects().export_memory(device, nullptr, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(held.objects().export_memory(VK_NULL_HANDLE, &request, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  /* The same for a semaphore's export, refusal by refusal. */
  SemaphoreGetWin32HandleRecord semaphore_request = semaphore_export(semaphore);
  semaphore_request.handle_type_ = 0x00000004u;
  CHECK_EQ(held.objects().export_semaphore(device, &semaphore_request, &handle),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  semaphore_request = semaphore_export(semaphore);
  semaphore_request.type_ = static_cast<VkStructureType>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  CHECK_EQ(held.objects().export_semaphore(device, &semaphore_request, &handle),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  semaphore_request = semaphore_export(VK_NULL_HANDLE);
  CHECK_EQ(held.objects().export_semaphore(device, &semaphore_request, &handle),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  semaphore_request = semaphore_export(semaphore);
  CHECK_EQ(held.objects().export_semaphore(device, &semaphore_request, nullptr),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(held.objects().export_semaphore(VK_NULL_HANDLE, &semaphore_request, &handle),
           VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(held.objects().export_semaphore(device, nullptr, &handle), VK_ERROR_INVALID_EXTERNAL_HANDLE);
  CHECK_EQ(held.objects().live(), 0u);
  CHECK(held.objects().invariants_hold()); }

/* And on the import side, where a wrong answer would hand one application
 * another's memory. */
TEST(external_objects, an_import_it_did_not_export_follows_nothing)
{ Exported held;
  void    *handle = nullptr;
  const MemoryGetWin32HandleRecord exporting = memory_export(memory);
  CHECK_EQ(held.objects().export_memory(device, &exporting, &handle), VK_SUCCESS);
  ImportMemoryWin32HandleRecord importing = memory_import(handle);
  /* A handle this process did not export. */
  importing.handle_ = reinterpret_cast<void *>(0x1234u);
  CHECK(held.objects().imported_memory(&importing) == VK_NULL_HANDLE);
  importing.handle_ = nullptr;
  CHECK(held.objects().imported_memory(&importing) == VK_NULL_HANDLE);
  importing.handle_ = handle;
  /* A named handle: a name is what another process would open, and
   * cross-process identity is feature 053's. */
  const char name[] = "Local\\rvk";
  importing.name_ = name;
  CHECK(held.objects().imported_memory(&importing) == VK_NULL_HANDLE);
  importing.name_ = nullptr;
  /* A handle type or a structure type this path does not answer. */
  importing.handle_type_ = 0x00000004u;
  CHECK(held.objects().imported_memory(&importing) == VK_NULL_HANDLE);
  importing.handle_type_ = av::icd::opaque_win32_memory;
  importing.type_ = static_cast<VkStructureType>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  CHECK(held.objects().imported_memory(&importing) == VK_NULL_HANDLE);
  CHECK(held.objects().imported_memory(nullptr) == VK_NULL_HANDLE);
  /* The same refusals for a semaphore. */
  void *semaphore_handle = nullptr;
  const SemaphoreGetWin32HandleRecord exporting_semaphore = semaphore_export(semaphore);
  CHECK_EQ(held.objects().export_semaphore(device, &exporting_semaphore, &semaphore_handle), VK_SUCCESS);
  ImportSemaphoreWin32HandleRecord importing_semaphore = semaphore_import(semaphore_handle);
  importing_semaphore.name_ = name;
  CHECK(held.objects().imported_semaphore(&importing_semaphore) == VK_NULL_HANDLE);
  importing_semaphore.name_ = nullptr;
  importing_semaphore.handle_type_ = 0u;
  CHECK(held.objects().imported_semaphore(&importing_semaphore) == VK_NULL_HANDLE);
  importing_semaphore.handle_type_ = av::icd::opaque_win32_semaphore;
  importing_semaphore.type_ = static_cast<VkStructureType>(0x7FFFFFFF);
  CHECK(held.objects().imported_semaphore(&importing_semaphore) == VK_NULL_HANDLE);
  CHECK(held.objects().imported_semaphore(nullptr) == VK_NULL_HANDLE);
  /* A well-formed import of a handle this table never gave out: the lookup
   * finds no slot at all rather than the nearest one. */
  ImportSemaphoreWin32HandleRecord stranger = semaphore_import(reinterpret_cast<void *>(0x4321u));
  CHECK(held.objects().imported_semaphore(&stranger) == VK_NULL_HANDLE);
  /* What was exported is still there after all of that. */
  const ImportMemoryWin32HandleRecord good = memory_import(handle);
  CHECK(held.objects().imported_memory(&good) == memory); }

/* An object destroyed takes its handle with it, and so does its device: a
 * handle that outlived what it names is one an import could still follow. */
TEST(external_objects, what_is_destroyed_takes_its_handle_with_it)
{ Exported held;
  void    *first = nullptr;
  void    *second = nullptr;
  void    *third = nullptr;
  const MemoryGetWin32HandleRecord    one = memory_export(memory);
  const MemoryGetWin32HandleRecord    two = memory_export(other_memory);
  const SemaphoreGetWin32HandleRecord three = semaphore_export(semaphore);
  CHECK_EQ(held.objects().export_memory(device, &one, &first), VK_SUCCESS);
  CHECK_EQ(held.objects().export_memory(second_device, &two, &second), VK_SUCCESS);
  CHECK_EQ(held.objects().export_semaphore(device, &three, &third), VK_SUCCESS);
  CHECK_EQ(held.objects().live(), 3u);

  held.objects().forget_memory(memory);
  const ImportMemoryWin32HandleRecord gone = memory_import(first);
  CHECK(held.objects().imported_memory(&gone) == VK_NULL_HANDLE);
  CHECK_EQ(held.objects().live(), 2u);
  /* One it never had, and none at all: neither takes a slot away. */
  held.objects().forget_memory(memory);
  held.objects().forget_memory(VK_NULL_HANDLE);
  held.objects().forget_semaphore(VK_NULL_HANDLE);
  held.objects().forget_device(VK_NULL_HANDLE);
  CHECK_EQ(held.objects().live(), 2u);

  held.objects().forget_semaphore(semaphore);
  CHECK_EQ(held.objects().live(), 1u);
  held.objects().forget_device(second_device);
  CHECK_EQ(held.objects().live(), 0u);
  CHECK(held.objects().invariants_hold()); }

/* Every slot taken: an export is refused rather than one handle quietly
 * replacing another, which would hand an importer the wrong object. */
TEST(external_objects, a_table_with_no_room_refuses_an_export)
{ Exported held;
  for (std::uint32_t index = 0u; index < ExternalObjects::max_objects; index++) {
      void                                  *handle = nullptr;
      const MemoryGetWin32HandleRecord request =
          memory_export(reinterpret_cast<VkDeviceMemory>(0xA000u + index));
      CHECK_EQ(held.objects().export_memory(device, &request, &handle), VK_SUCCESS);
  }
  CHECK_EQ(held.objects().live(), ExternalObjects::max_objects);
  void                                  *refused = nullptr;
  const MemoryGetWin32HandleRecord one_more = memory_export(reinterpret_cast<VkDeviceMemory>(0xB000u));
  CHECK_EQ(held.objects().export_memory(device, &one_more, &refused), VK_ERROR_TOO_MANY_OBJECTS);
  CHECK(refused == nullptr);
  void                                  *semaphore_refused = nullptr;
  const SemaphoreGetWin32HandleRecord waiting = semaphore_export(semaphore);
  CHECK_EQ(held.objects().export_semaphore(device, &waiting, &semaphore_refused), VK_ERROR_TOO_MANY_OBJECTS);
  /* What it already held is intact. */
  CHECK_EQ(held.objects().live(), ExternalObjects::max_objects);
  CHECK(held.objects().invariants_hold()); }

/* Each clause of the invariant refuses the state it names: a slot with both
 * kinds of object or neither, an object in a slot no device owns, and the same
 * memory in two slots - which is what would hand two importers one object
 * through two handles. */
TEST(external_objects, an_invariant_refuses_each_state_a_slot_cannot_be_in)
{ const MemoryGetWin32HandleRecord exporting = memory_export(memory);
  { Exported held;
    void    *handle = nullptr;
    CHECK_EQ(held.objects().export_memory(device, &exporting, &handle), VK_SUCCESS);
    ExternalObjectsProbe::both_kinds_at_once(held.objects());
    CHECK(!held.objects().invariants_hold()); }
  { Exported held;
    void    *handle = nullptr;
    CHECK_EQ(held.objects().export_memory(device, &exporting, &handle), VK_SUCCESS);
    ExternalObjectsProbe::neither_kind(held.objects());
    CHECK(!held.objects().invariants_hold()); }
  { Exported held;
    ExternalObjectsProbe::an_object_in_an_empty_slot(held.objects());
    CHECK(!held.objects().invariants_hold()); }
  { Exported held;
    ExternalObjectsProbe::a_semaphore_in_an_empty_slot(held.objects());
    CHECK(!held.objects().invariants_hold()); }
  { Exported held;
    void    *handle = nullptr;
    CHECK_EQ(held.objects().export_memory(device, &exporting, &handle), VK_SUCCESS);
    ExternalObjectsProbe::the_same_memory_twice(held.objects());
    CHECK(!held.objects().invariants_hold()); }
  { Exported held;
    void    *handle = nullptr;
    const SemaphoreGetWin32HandleRecord exporting_semaphore = semaphore_export(semaphore);
    CHECK_EQ(held.objects().export_semaphore(device, &exporting_semaphore, &handle), VK_SUCCESS);
    /* A semaphore in two slots is the same mistake the other way round. */
    ExternalObjectsProbe::the_same_memory_twice(held.objects());
    CHECK(!held.objects().invariants_hold()); } }

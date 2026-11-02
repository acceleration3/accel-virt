#include "harness/framework/test_registry.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "icd/vulkan/presentation.hpp"
#include "icd/vulkan/surface_table.hpp"
#include "icd/vulkan/win32_surface_create_record.hpp"

#include <cstdint>

namespace av::icd {
struct SurfaceTableProbe {
    static void orphan(SurfaceTable &table) { table.slots_[0].window_ = reinterpret_cast<void *>(0x99u); }
};

}

using av::IcdRig;
using av::ScriptedVulkanDevice;
using av::icd::Presentation;
using av::icd::SurfaceSlot;
using av::icd::SurfaceTableProbe;
using av::icd::SurfaceTable;
using av::icd::Win32SurfaceCreateRecord;
using av::icd::win32_surface_create_info;

namespace {

VkInstance as_instance(std::uintptr_t number) { return reinterpret_cast<VkInstance>(number); }
void      *as_window(std::uintptr_t number) { return reinterpret_cast<void *>(number); }

Win32SurfaceCreateRecord surface_of(void *window)
{ Win32SurfaceCreateRecord record = {};
  record.type_ = win32_surface_create_info;
  record.module_ = as_window(0x100u);
  record.window_ = window;
  return record; }

class WsiRig {
public:
    WsiRig() : wsi_(rig_.platform(), rig_.kmt(), rig_.windows(), rig_.chains(), rig_.log(), rig_.root(), host_) {}

    IcdRig       &rig() { return rig_; }
    Presentation &wsi() { return wsi_; }

private:
    IcdRig               rig_;
    ScriptedVulkanDevice host_;
    Presentation         wsi_;
};

}

TEST(surface_table, a_surface_is_its_window_until_it_is_destroyed)
{ SurfaceTable surfaces;
  CHECK(surfaces.invariants_hold());
  CHECK_EQ(surfaces.live(), 0u);
  VkSurfaceKHR first = VK_NULL_HANDLE;
  VkSurfaceKHR second = VK_NULL_HANDLE;
  CHECK_EQ(surfaces.create(as_instance(1u), as_window(0x10u), first), VK_SUCCESS);
  CHECK(surfaces.invariants_hold());
  CHECK_EQ(surfaces.create(as_instance(1u), as_window(0x20u), second), VK_SUCCESS);
  CHECK(first != VK_NULL_HANDLE);
  CHECK(first != second);
  CHECK_EQ(surfaces.live(), 2u);
  CHECK(surfaces.window_of(first) == as_window(0x10u));
  CHECK(surfaces.window_of(second) == as_window(0x20u));
  const SurfaceSlot *const slot = reinterpret_cast<const SurfaceSlot *>(first);
  CHECK(slot->window_ == as_window(0x10u));
  CHECK(slot->instance_ == as_instance(1u));
  CHECK(surfaces.window_of(reinterpret_cast<VkSurfaceKHR>(0x30u)) == nullptr);
  CHECK(surfaces.window_of(VK_NULL_HANDLE) == nullptr);
  surfaces.destroy(as_instance(1u), first);
  CHECK(surfaces.invariants_hold());
  CHECK_EQ(surfaces.live(), 1u);
  CHECK(surfaces.window_of(first) == nullptr);
  surfaces.destroy(as_instance(1u), first);
  surfaces.destroy(as_instance(1u), reinterpret_cast<VkSurfaceKHR>(0x40u));
  CHECK_EQ(surfaces.live(), 1u);
  CHECK(surfaces.invariants_hold()); }

TEST(surface_table, only_the_instance_that_made_a_surface_destroys_or_forgets_it)
{ SurfaceTable surfaces;
  VkSurfaceKHR mine = VK_NULL_HANDLE;
  VkSurfaceKHR theirs = VK_NULL_HANDLE;
  CHECK_EQ(surfaces.create(as_instance(1u), as_window(0x10u), mine), VK_SUCCESS);
  CHECK_EQ(surfaces.create(as_instance(2u), as_window(0x20u), theirs), VK_SUCCESS);
  surfaces.destroy(as_instance(2u), mine);
  CHECK(surfaces.window_of(mine) == as_window(0x10u));
  surfaces.forget(as_instance(2u));
  CHECK(surfaces.invariants_hold());
  CHECK_EQ(surfaces.live(), 1u);
  CHECK(surfaces.window_of(mine) == as_window(0x10u));
  CHECK(surfaces.window_of(theirs) == nullptr);
  surfaces.forget(as_instance(1u));
  CHECK_EQ(surfaces.live(), 0u);
  CHECK(surfaces.invariants_hold()); }

TEST(surface_table, a_surface_without_a_window_is_refused_and_a_full_table_is_out_of_memory)
{ SurfaceTable surfaces;
  VkSurfaceKHR surface = reinterpret_cast<VkSurfaceKHR>(0x1u);
  CHECK_EQ(surfaces.create(as_instance(1u), nullptr, surface), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(surface == VK_NULL_HANDLE);
  CHECK_EQ(surfaces.live(), 0u);
  VkSurfaceKHR made[SurfaceTable::max_surfaces] = {};
  for (std::uint32_t index = 0u; index < SurfaceTable::max_surfaces; index++)
      CHECK_EQ(surfaces.create(as_instance(1u), as_window(0x100u + index), made[index]), VK_SUCCESS);
  CHECK_EQ(surfaces.live(), SurfaceTable::max_surfaces);
  VkSurfaceKHR past = reinterpret_cast<VkSurfaceKHR>(0x1u);
  CHECK_EQ(surfaces.create(as_instance(1u), as_window(0x900u), past), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(past == VK_NULL_HANDLE);
  CHECK(surfaces.invariants_hold());
  surfaces.destroy(as_instance(1u), made[3u]);
  VkSurfaceKHR again = VK_NULL_HANDLE;
  CHECK_EQ(surfaces.create(as_instance(1u), as_window(0x900u), again), VK_SUCCESS);
  CHECK(again == made[3u]);
  CHECK_EQ(surfaces.live(), SurfaceTable::max_surfaces);
  CHECK(surfaces.invariants_hold()); }

TEST(icd_presentation, a_win32_surface_is_made_from_its_window_and_nothing_else)
{ WsiRig rig;
  const Win32SurfaceCreateRecord good = surface_of(as_window(0x10u));
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  CHECK_EQ(rig.wsi().create_surface(as_instance(1u), &good, &surface), VK_SUCCESS);
  CHECK(surface != VK_NULL_HANDLE);
  CHECK_EQ(rig.wsi().surfaces(), 1u);
  CHECK(rig.wsi().invariants_hold());
  Win32SurfaceCreateRecord wrong_type = good;
  wrong_type.type_ = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  Win32SurfaceCreateRecord no_window = good;
  no_window.window_ = nullptr;
  VkSurfaceKHR refused = reinterpret_cast<VkSurfaceKHR>(0x1u);
  CHECK_EQ(rig.wsi().create_surface(as_instance(1u), nullptr, &refused), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(refused == VK_NULL_HANDLE);
  refused = reinterpret_cast<VkSurfaceKHR>(0x1u);
  CHECK_EQ(rig.wsi().create_surface(as_instance(1u), &wrong_type, &refused), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(refused == VK_NULL_HANDLE);
  CHECK_EQ(rig.wsi().create_surface(as_instance(1u), &no_window, &refused), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.wsi().surfaces(), 1u);
  rig.wsi().destroy_surface(as_instance(1u), surface);
  CHECK_EQ(rig.wsi().surfaces(), 0u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, every_queue_supports_a_live_surface_and_none_supports_a_lost_one)
{ WsiRig rig;
  const Win32SurfaceCreateRecord info = surface_of(as_window(0x10u));
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  CHECK_EQ(rig.wsi().create_surface(as_instance(1u), &info, &surface), VK_SUCCESS);
  VkBool32 supported = VK_FALSE;
  CHECK_EQ(rig.wsi().surface_support(surface, &supported), VK_SUCCESS);
  CHECK_EQ(supported, VK_TRUE);
  rig.wsi().destroy_surface(as_instance(1u), surface);
  supported = VK_TRUE;
  CHECK_EQ(rig.wsi().surface_support(surface, &supported), VK_ERROR_SURFACE_LOST_KHR);
  CHECK_EQ(supported, VK_FALSE); }

TEST(icd_presentation, an_instance_that_goes_takes_its_surfaces)
{ WsiRig rig;
  const Win32SurfaceCreateRecord info = surface_of(as_window(0x10u));
  VkSurfaceKHR mine = VK_NULL_HANDLE;
  VkSurfaceKHR theirs = VK_NULL_HANDLE;
  CHECK_EQ(rig.wsi().create_surface(as_instance(1u), &info, &mine), VK_SUCCESS);
  CHECK_EQ(rig.wsi().create_surface(as_instance(2u), &info, &theirs), VK_SUCCESS);
  CHECK_EQ(rig.wsi().surfaces(), 2u);
  rig.wsi().forget_instance(as_instance(2u));
  CHECK_EQ(rig.wsi().surfaces(), 1u);
  VkBool32 supported = VK_FALSE;
  CHECK_EQ(rig.wsi().surface_support(mine, &supported), VK_SUCCESS);
  CHECK_EQ(rig.wsi().surface_support(theirs, &supported), VK_ERROR_SURFACE_LOST_KHR);
  CHECK(rig.wsi().invariants_hold()); }

TEST(surface_table, its_invariants_refuse_a_window_no_instance_owns)
{ SurfaceTable surfaces;
  CHECK(surfaces.invariants_hold());
  SurfaceTableProbe::orphan(surfaces);
  CHECK(!surfaces.invariants_hold()); }

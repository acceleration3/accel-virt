#include "executor/d3d/d3d12_heap_flags.hpp"
#include "executor/d3d/d3d12_resource_flags.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12HeapFlags;
using av::D3d12ResourceFlags;

namespace {

constexpr std::uint32_t shared = 0x1u;
constexpr std::uint32_t deny_buffers = 0x4u;
constexpr std::uint32_t allow_display = 0x8u;
constexpr std::uint32_t shared_cross_adapter = 0x20u;
constexpr std::uint32_t deny_rt_ds = 0x40u;
constexpr std::uint32_t deny_non_rt_ds = 0x80u;
constexpr std::uint32_t hardware_protected = 0x100u;
constexpr std::uint32_t write_watch = 0x200u;
constexpr std::uint32_t shader_atomics = 0x400u;
constexpr std::uint32_t not_resident = 0x800u;
constexpr std::uint32_t not_zeroed = 0x1000u;
constexpr std::uint32_t manual_write_tracking = 0x2000u;

}

TEST(d3d12_heap_flags, the_flags_that_only_say_what_a_heap_may_hold_are_kept)
{ CHECK_EQ(D3d12HeapFlags(shared).kept(), shared);
  CHECK_EQ(D3d12HeapFlags(deny_buffers | allow_display).kept(), deny_buffers | allow_display);
  CHECK_EQ(D3d12HeapFlags(deny_rt_ds | deny_non_rt_ds).kept(), deny_rt_ds | deny_non_rt_ds);
  CHECK_EQ(D3d12HeapFlags(0u).kept(), 0u); }

TEST(d3d12_heap_flags, a_guest_cannot_ask_for_a_heap_that_is_not_cleared)
{ CHECK_EQ(D3d12HeapFlags(not_zeroed).kept(), 0u);
  CHECK_EQ(D3d12HeapFlags(not_zeroed | shared | deny_buffers).kept(), shared | deny_buffers);
  CHECK_EQ(D3d12HeapFlags(0xFFFFFFFFu).kept() & not_zeroed, 0u); }

TEST(d3d12_heap_flags, every_other_flag_and_every_undefined_bit_is_dropped)
{ const std::uint32_t dropped[] = {shared_cross_adapter, hardware_protected, write_watch, shader_atomics,
                                   not_resident,         not_zeroed,         manual_write_tracking,
                                   0x4000u,              0x80000000u,        0x2u,                  0x10u};
  for (const std::uint32_t flag : dropped) CHECK_EQ(D3d12HeapFlags(flag).kept(), 0u);
  CHECK_EQ(D3d12HeapFlags(0xFFFFFFFFu).kept(), shared | deny_buffers | allow_display | deny_rt_ds | deny_non_rt_ds); }

TEST(d3d12_resource_flags, the_flags_that_say_how_a_resource_is_used_are_kept)
{ CHECK_EQ(D3d12ResourceFlags(0x1u | 0x2u | 0x4u | 0x8u | 0x20u).kept(), 0x1u | 0x2u | 0x4u | 0x8u | 0x20u);
  CHECK_EQ(D3d12ResourceFlags(0u).kept(), 0u); }

TEST(d3d12_resource_flags, cross_adapter_video_raytracing_and_undefined_bits_are_dropped)
{ const std::uint32_t dropped[] = {0x10u, 0x40u, 0x80u, 0x100u, 0x200u, 0x80000000u};
  for (const std::uint32_t flag : dropped) CHECK_EQ(D3d12ResourceFlags(flag).kept(), 0u);
  CHECK_EQ(D3d12ResourceFlags(0xFFFFFFFFu).kept(), 0x1u | 0x2u | 0x4u | 0x8u | 0x20u); }

#include "harness/framework/test_registry.hpp"
#include "icd/vulkan/win32_handle_kind.hpp"

using av::icd::Win32HandleKind;

TEST(win32_handle_kind, the_opaque_win32_type_is_asked_about_as_an_opaque_fd)
{ Win32HandleKind memory(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                         VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
  CHECK(memory.invariants_hold());
  CHECK(memory.answers(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK_EQ(memory.host_type(), static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));

  CHECK(!memory.answers(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT));
  CHECK(!memory.answers(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT));
  CHECK(!memory.answers(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT));
  CHECK(!memory.answers(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));
  CHECK(!memory.answers(0u));
  CHECK(!memory.answers(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
                        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT));

  CHECK_EQ(memory.reported(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT |
                           VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT),
           static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK_EQ(memory.reported(VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT), 0u);
  CHECK_EQ(memory.reported(0u), 0u); }

TEST(win32_handle_kind, a_semaphore_kind_is_the_same_arithmetic_on_its_own_bits)
{ Win32HandleKind semaphore(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                            VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
  CHECK(semaphore.invariants_hold());
  CHECK(semaphore.answers(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK(!semaphore.answers(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT));
  CHECK(!semaphore.answers(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT));
  CHECK_EQ(semaphore.reported(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT |
                              VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT),
           static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT));
  CHECK_EQ(semaphore.reported(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT), 0u); }

TEST(win32_handle_kind, a_kind_of_two_bits_or_one_bit_twice_is_named)
{ Win32HandleKind twice(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
  CHECK(!twice.invariants_hold());
  Win32HandleKind pair(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
                           VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
                       VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
  CHECK(!pair.invariants_hold());
  Win32HandleKind none(0u, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
  CHECK(!none.invariants_hold());
  Win32HandleKind nothing(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT, 0u);
  CHECK(!nothing.invariants_hold());
  Win32HandleKind two_hosts(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT |
                                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
  CHECK(!two_hosts.invariants_hold()); }

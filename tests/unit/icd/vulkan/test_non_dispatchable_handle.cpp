#include "icd/vulkan/non_dispatchable_handle.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace {

using Buffer = av::NonDispatchableHandle<struct BufferTag>;
using Image = av::NonDispatchableHandle<struct ImageTag>;

/* What lets it stand where the ABI has a uint64_t: the same size and
 * alignment, and copied as bytes. And what it is for: two handles are two
 * types, which one uint64_t for both is not. */
static_assert(sizeof(Buffer) == sizeof(std::uint64_t));
static_assert(alignof(Buffer) == alignof(std::uint64_t));
static_assert(__is_trivially_copyable(Buffer));
static_assert(__is_standard_layout(Buffer));
static_assert(!__is_same(Buffer, Image));

}  // namespace

TEST(non_dispatchable_handle, a_default_or_null_handle_holds_zero)
{ const Buffer none;
  CHECK(none == nullptr);
  CHECK(!static_cast<bool>(none));
  CHECK_EQ(static_cast<std::uint64_t>(none), 0u);
  const Buffer null = VK_NULL_HANDLE;
  CHECK(null == none);
  CHECK_EQ(static_cast<std::uint64_t>(null), 0u); }

TEST(non_dispatchable_handle, a_handle_holds_all_64_bits_it_was_made_from)
{ const Buffer high(0x8000000000000001ull);
  CHECK_EQ(static_cast<std::uint64_t>(high), 0x8000000000000001ull);
  CHECK(static_cast<bool>(high));
  CHECK(!(high == nullptr));
  CHECK(high == Buffer(0x8000000000000001ull));
  CHECK(high != Buffer(1u));
  CHECK(high != Buffer()); }

TEST(non_dispatchable_handle, its_bytes_are_the_integers_bytes)
{ const Buffer handle(0x0123456789ABCDEFull);
  std::uint64_t bits = 0u;
  __builtin_memcpy(&bits, &handle, sizeof handle);
  CHECK_EQ(bits, 0x0123456789ABCDEFull);
  Buffer copied;
  __builtin_memcpy(&copied, &bits, sizeof bits);
  CHECK(copied == handle); }

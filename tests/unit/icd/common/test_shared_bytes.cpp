/* The bytes a memory object is shared as (SharedBytes): whole pages, never
 * fewer than asked, and nothing for a size that names no object - none at all,
 * or one rounding up would wrap past the top of 64 bits into a tiny size. */
#include "harness/framework/test_registry.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstdint>

using av::SharedBytes;

namespace {

constexpr std::uint64_t page = av::SharedObjectRecord::page_size;
constexpr std::uint64_t top = ~std::uint64_t{0};

}  // namespace

TEST(shared_bytes, a_size_rounds_up_to_whole_pages)
{ CHECK_EQ(SharedBytes(1u).pages(), page);
  CHECK_EQ(SharedBytes(page).pages(), page);
  CHECK_EQ(SharedBytes(page + 1u).pages(), 2u * page);
  CHECK_EQ(SharedBytes(5u * page - 1u).pages(), 5u * page); }

TEST(shared_bytes, nothing_and_a_size_that_would_wrap_name_no_object)
{ CHECK_EQ(SharedBytes(0u).pages(), 0u);
  CHECK_EQ(SharedBytes(top).pages(), 0u);
  CHECK_EQ(SharedBytes(top - (page - 2u)).pages(), 0u);
  /* The largest that still rounds: exactly the last whole page. */
  CHECK_EQ(SharedBytes(top - (page - 1u)).pages(), top - (page - 1u)); }

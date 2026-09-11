#include "core/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "core/ddi/dxgk_pte_record.hpp"
#include "core/memory/page_table_update.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::DxgkBuildPagingBufferUpdatePageTableRecord;
using av::DxgkPteRecord;
using av::Irql;
using av::MockPlatform;
using av::PageTableUpdate;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint8_t poison = 0xEEu;
constexpr std::uint32_t repeat = 0x1u;

std::uint64_t u64(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint64_t value = 0u;
    for (unsigned byte = 0u; byte < 8u; byte++) value |= std::uint64_t{bytes[offset + byte]} << (8u * byte);
    return value;
}

/* A poisoned 4096-byte table with a guard, and three entries to write. */
class Update {
public:
    Update() : table_{}, entries_{}, record_{}
    {
        std::memset(table_, poison, sizeof table_);
        for (std::uint64_t index = 0u; index < 3u; index++) entries_[index] = {1u, 0x100u + index};
        record_.PageTableAddress = table_;
        record_.pPageTableEntries = entries_;
        record_.StartIndex = 7u;
        record_.NumPageTableEntries = 3u;
    }
    Status write()
    {
        MockPlatform platform;
        return PageTableUpdate(platform, record_).write();
    }
    DxgkBuildPagingBufferUpdatePageTableRecord &record() { return record_; }
    const std::uint8_t *table() const { return table_; }
    bool untouched_outside(std::size_t first, std::size_t end) const
    {
        for (std::size_t byte = 0u; byte < sizeof table_; byte++)
            if ((byte < first || byte >= end) && table_[byte] != poison) return false;
        return true;
    }

private:
    std::uint8_t                               table_[4096u + 64u];
    DxgkPteRecord                              entries_[3];
    DxgkBuildPagingBufferUpdatePageTableRecord record_;
};

}  // namespace

TEST(page_table_update_write, writes_each_entry_at_its_index)
{ Update update;
  CHECK_STATUS(update.write(), Status::ok);
  CHECK_EQ(u64(update.table(), 56u), (0x100ull << 12u) | 1u);
  CHECK_EQ(u64(update.table(), 64u), (0x101ull << 12u) | 1u);
  CHECK_EQ(u64(update.table(), 72u), (0x102ull << 12u) | 1u);
  CHECK(update.untouched_outside(56u, 80u)); }

TEST(page_table_update_write, repeat_writes_the_first_entry_to_all)
{ Update update;
  update.record().Flags = repeat;
  CHECK_STATUS(update.write(), Status::ok);
  CHECK_EQ(u64(update.table(), 72u), (0x100ull << 12u) | 1u); }

TEST(page_table_update_write, the_last_entries_of_a_table_fit)
{ Update update;
  update.record().StartIndex = 509u;
  CHECK_STATUS(update.write(), Status::ok);
  CHECK(update.untouched_outside(4072u, 4096u));
  update.record().NumPageTableEntries = 0u;
  update.record().StartIndex = 512u;
  CHECK_STATUS(update.write(), Status::ok); }

TEST(page_table_update_write, a_hostile_update_writes_nothing)
{ using Mutate = void (*)(DxgkBuildPagingBufferUpdatePageTableRecord &);
  const Mutate mutations[] = {
      [](DxgkBuildPagingBufferUpdatePageTableRecord &r) { r.StartIndex = 510u; },
      [](DxgkBuildPagingBufferUpdatePageTableRecord &r) { r.StartIndex = 0xFFFFFFFFu; },
      [](DxgkBuildPagingBufferUpdatePageTableRecord &r) { r.NumPageTableEntries = 0xFFFFFFFFu; },
      [](DxgkBuildPagingBufferUpdatePageTableRecord &r) { r.PageTableAddress = nullptr; },
      [](DxgkBuildPagingBufferUpdatePageTableRecord &r) { r.pPageTableEntries = nullptr; },
      [](DxgkBuildPagingBufferUpdatePageTableRecord &r) { r.UpdateMode = 1; },
      [](DxgkBuildPagingBufferUpdatePageTableRecord &r) { r.PageTableLevel = 4u; },
  };
  for (Mutate mutate : mutations) {
      Update update;
      mutate(update.record());
      CHECK_STATUS(update.write(), Status::invalid_parameter);
      CHECK(update.untouched_outside(0u, 0u));
  } }

TEST(page_table_update, is_passive_only)
{ MockPlatform platform;
  const DxgkBuildPagingBufferUpdatePageTableRecord record = {};
  platform.set_irql(Irql::dispatch);
  static_cast<void>(PageTableUpdate(platform, record).write());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

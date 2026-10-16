/* Keys a guest names, in any order and of any value, inserted, found, taken
 * and erased by key or by slot: the table agrees with a plain list of what is
 * live after every step, an entry never moves while it lives, its invariants
 * hold, and a failed allocation changes nothing. */
#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t step_bytes = 5u;
constexpr std::size_t most_live = 4096u;

using Table = av::KeyedSlots<std::uint32_t, std::uint32_t>;

class Model {
public:
    std::size_t find(std::uint32_t key) const
    {
        for (std::size_t index = 0u; index < count_; index++)
            if (keys_[index] == key) return index;
        return count_;
    }
    void add(std::uint32_t key, std::uint32_t slot, const std::uint32_t *address)
    {
        keys_[count_] = key;
        slots_[count_] = slot;
        addresses_[count_++] = address;
    }
    void drop(std::size_t index)
    {
        count_--;
        keys_[index] = keys_[count_];
        slots_[index] = slots_[count_];
        addresses_[index] = addresses_[count_];
    }
    std::size_t          count() const { return count_; }
    std::uint32_t        key(std::size_t index) const { return keys_[index]; }
    std::uint32_t        slot(std::size_t index) const { return slots_[index]; }
    const std::uint32_t *address(std::size_t index) const { return addresses_[index]; }

private:
    std::uint32_t        keys_[most_live] = {};
    std::uint32_t        slots_[most_live] = {};
    const std::uint32_t *addresses_[most_live] = {};
    std::size_t          count_ = 0u;
};

bool agrees(const Table &table, const Model &model)
{
    if (table.count() != model.count()) return false;
    for (std::size_t index = 0u; index < model.count(); index++) {
        const std::uint32_t *const found = table.find(model.key(index));
        if (found != model.address(index) || *found != (model.key(index) ^ 0x5A5A5A5Au)) return false;
        if (table.slot_of(model.key(index)) != model.slot(index) || table.at(model.slot(index)) != found)
            return false;
    }
    return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform platform;
    Table            table(platform, av::Pool::paged);
    static Model     model;
    model = Model{};
    for (std::size_t offset = 0u; offset + step_bytes <= size; offset += step_bytes) {
        std::uint32_t key = 0u;
        std::memcpy(&key, data + offset + 1u, sizeof key);
        const std::uint8_t op = data[offset];
        const std::size_t  modelled = model.find(key);
        const bool         live = modelled != model.count();
        switch (op % 5u) {
        case 0u: {
            if (model.count() == most_live) break;
            if (op & 0x80u) platform.fail_allocation_at(1u);
            std::uint32_t    slot = Table::no_slot;
            const av::Status inserted = table.insert(key, key ^ 0x5A5A5A5Au, slot);
            platform.fail_allocation_at(0u);
            if (inserted == av::Status::ok) {
                if (live) __builtin_trap();
                model.add(key, slot, table.at(slot));
            } else if (inserted == av::Status::conflict) {
                if (!live) __builtin_trap();
            } else if (inserted != av::Status::no_memory) {
                __builtin_trap();
            }
            break;
        }
        case 1u:
            if (table.erase(key) != live) __builtin_trap();
            if (live) model.drop(modelled);
            break;
        case 2u: {
            std::uint32_t taken = 0u;
            if (table.take(key, taken) != live) __builtin_trap();
            if (live && taken != (key ^ 0x5A5A5A5Au)) __builtin_trap();
            if (live) model.drop(modelled);
            break;
        }
        case 3u: {
            /* The key as a slot: a hostile slot erases only what is in it. */
            const std::uint32_t slot = key % 8192u;
            for (std::size_t index = 0u; index < model.count(); index++)
                if (model.slot(index) == slot) {
                    model.drop(index);
                    break;
                }
            table.erase_slot(slot);
            break;
        }
        default:
            if (table.reserve(op >> 3u) != av::Status::ok) __builtin_trap();
            break;
        }
        if (!agrees(table, model) || !table.invariants_hold()) __builtin_trap();
    }
    return 0;
}

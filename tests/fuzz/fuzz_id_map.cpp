/* Ids a guest names, in any order and of any value, inserted, found, taken and
 * removed: the map agrees with a plain list of what is live after every step,
 * its invariants hold, and a failed allocation changes nothing. */
#include "core/support/id_map.hpp"
#include "core/support/status.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t step_bytes = 5u;
constexpr std::size_t most_live = 4096u;

class Model {
public:
    std::uint32_t *find(std::uint32_t key)
    {
        for (std::size_t index = 0u; index < count_; index++)
            if (keys_[index] == key) return &values_[index];
        return nullptr;
    }
    void add(std::uint32_t key, std::uint32_t value)
    {
        keys_[count_] = key;
        values_[count_++] = value;
    }
    void drop(std::uint32_t key)
    {
        for (std::size_t index = 0u; index < count_; index++)
            if (keys_[index] == key) {
                keys_[index] = keys_[--count_];
                values_[index] = values_[count_];
                return;
            }
    }
    std::size_t count() const { return count_; }

private:
    std::uint32_t keys_[most_live] = {};
    std::uint32_t values_[most_live] = {};
    std::size_t   count_ = 0u;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform                        platform;
    av::IdMap<std::uint32_t, std::uint32_t> map(platform, av::Pool::paged);
    static Model                            model;
    model = Model{};
    for (std::size_t offset = 0u; offset + step_bytes <= size; offset += step_bytes) {
        std::uint32_t key = 0u;
        std::memcpy(&key, data + offset + 1u, sizeof key);
        const std::uint8_t   op = data[offset];
        std::uint32_t *const modelled = model.find(key);
        const std::uint32_t *found = map.find(key);
        if ((found == nullptr) != (modelled == nullptr) || (found != nullptr && *found != *modelled)) __builtin_trap();
        switch (op % 4u) {
        case 0u: {
            if (model.count() == most_live) break;
            if (op & 0x80u) platform.fail_allocation_at(1u);
            const av::Status inserted = map.insert(key, key ^ 0x5A5A5A5Au);
            platform.fail_allocation_at(0u);
            if (inserted == av::Status::ok) {
                if (modelled != nullptr) __builtin_trap();
                model.add(key, key ^ 0x5A5A5A5Au);
            } else if (inserted == av::Status::conflict) {
                if (modelled == nullptr) __builtin_trap();
            } else if (inserted != av::Status::no_memory) {
                __builtin_trap();
            }
            break;
        }
        case 1u:
            if (map.remove(key) != (modelled != nullptr)) __builtin_trap();
            model.drop(key);
            break;
        case 2u: {
            std::uint32_t taken = 0u;
            if (map.take(key, taken) != (modelled != nullptr)) __builtin_trap();
            if (modelled != nullptr && taken != *modelled) __builtin_trap();
            model.drop(key);
            break;
        }
        default:
            if (map.reserve(op >> 2u) != av::Status::ok) __builtin_trap();
            break;
        }
        if (map.count() != model.count() || !map.invariants_hold()) __builtin_trap();
    }
    return 0;
}

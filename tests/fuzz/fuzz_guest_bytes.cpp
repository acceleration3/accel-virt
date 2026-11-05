#include "core/support/status.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t memories = 8u;
constexpr std::size_t   step = 17u;

struct Bound {
    bool          live;
    std::uint64_t base;
    std::uint64_t length;
};

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

std::uint8_t *memory_of(std::uint32_t id)
{
    return reinterpret_cast<std::uint8_t *>(std::uintptr_t{0x100000000u} + std::uintptr_t{id} * 0x10000000u);
}

std::uint64_t u64_at(const std::uint8_t *data, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, data + at, sizeof value);
    return value;
}

std::uint64_t slots_of(std::uint64_t length)
{
    return ((length - 1u) >> av::GuestBytes::slot_shift) + 1u;
}

const Bound *owner(const Bound *model, std::uint64_t offset)
{
    const std::uint64_t slot = offset >> av::GuestBytes::slot_shift;
    for (std::uint32_t id = 0u; id < memories; id++) {
        if (!model[id].live) continue;
        const std::uint64_t first = model[id].base >> av::GuestBytes::slot_shift;
        if (slot >= first && slot - first < slots_of(model[id].length)) return &model[id];
    }
    return nullptr;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::GuestBytes bytes;
    Bound          model[memories] = {};
    std::uint32_t  live = 0u;
    for (std::size_t at = 0u; at + step <= size; at += step) {
        const std::uint8_t  op = data[at] % 3u;
        const std::uint32_t id = data[at] / 3u % memories;
        const std::uint64_t one = u64_at(data, at + 1u);
        const std::uint64_t two = u64_at(data, at + 9u);
        if (op == 0u) {
            const std::uint64_t base = (one % (av::GuestBytes::slot_count + 2u)) << av::GuestBytes::slot_shift;
            const std::uint64_t length = two % 4u == 0u ? two : two % (av::GuestBytes::slot_bytes * 3u);
            bool expected = !model[id].live && length != 0u && base >> av::GuestBytes::slot_shift < av::GuestBytes::slot_count &&
                            length <= (std::uint64_t{av::GuestBytes::slot_count} - (base >> av::GuestBytes::slot_shift))
                                          << av::GuestBytes::slot_shift;
            for (std::uint32_t other = 0u; expected && other < memories; other++) {
                if (!model[other].live) continue;
                const std::uint64_t a = model[other].base >> av::GuestBytes::slot_shift;
                const std::uint64_t b = base >> av::GuestBytes::slot_shift;
                if (b < a + slots_of(model[other].length) && a < b + slots_of(length)) expected = false;
            }
            const av::Status made = bytes.bind(base, memory_of(id), length);
            require((made == av::Status::ok) == expected);
            if (expected) {
                model[id] = Bound{true, base, length};
                live++;
            }
        } else if (op == 1u) {
            const av::Status released = bytes.unbind(memory_of(id));
            require((released == av::Status::ok) == model[id].live);
            if (model[id].live) live--;
            model[id].live = false;
        } else {
            std::uint8_t *out = reinterpret_cast<std::uint8_t *>(1u);
            const av::Status found = bytes.resolve(one, two, out);
            const Bound *const held = owner(model, one);
            const std::uint64_t inside = held != nullptr ? one - held->base : 0u;
            const bool expected = held != nullptr && inside <= held->length && two <= held->length - inside;
            require((found == av::Status::ok) == expected);
            require(expected ? out == memory_of(static_cast<std::uint32_t>(held - model)) + inside : out == nullptr);
            require(bytes.at(one, two) == out);
        }
        require(bytes.invariants_hold());
        require(bytes.segments() == live);
    }
    return 0;
}

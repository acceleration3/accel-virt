#include "core/trace/trace_slot_view.hpp"
#include "core/trace/trace_snapshot.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::TraceSnapshot snapshot;
    const av::Status  status = snapshot.read(data, size);
    if (!snapshot.invariants_hold()) __builtin_trap();
    if (status != av::Status::ok) {
        if (snapshot.slots() != 0u) __builtin_trap();
        return 0;
    }
    if (std::strlen(snapshot.module()) >= 16u) __builtin_trap();
    std::uint64_t sum = snapshot.crash_address() + snapshot.crashed_at();
    for (std::uint32_t index = 0u; index <= snapshot.slots(); index++) {
        const av::TraceSlotView view = snapshot.slot(index);
        if (view.stored() > 32u) __builtin_trap();
        for (std::uint32_t frame = 0u; frame <= view.stored(); frame++)
            sum += view.name(frame) + view.level(frame) + view.entered(frame) + view.text(frame);
        sum += view.waiting_on() + view.holding() + view.entries();
    }
    static_cast<void>(sum);
    return 0;
}

#include "um/trace/interrupt_clock.hpp"

namespace av {

namespace {

/* KUSER_SHARED_DATA is mapped read-only at this address in every process;
 * InterruptTime is its KSYSTEM_TIME at offset 8 (ntddk.h, stable since NT 4). */
constexpr uintptr_t interrupt_time_address = 0x7FFE0008u;

}  // namespace

InterruptClock::InterruptClock() : time_(reinterpret_cast<const uint32_t *>(interrupt_time_address)) {}

/* The kernel updates the high part twice around the low part; a read between
 * those updates shows two different highs and is taken again. */
uint64_t InterruptClock::now() const
{
    for (;;) {
        const uint32_t high = time_[1];
        const uint32_t low = time_[0];
        if (high == time_[2]) return static_cast<uint64_t>(high) << 32u | low;
    }
}

}  // namespace av

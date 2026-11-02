#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/replay/recording_reader.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::RecordingReader reader(data, size);
    const av::Status    opened = reader.open();
    if (!reader.invariants_hold()) __builtin_trap();
    if (opened != av::Status::ok) {
        if (!reader.damaged() || reader.damage_offset() != 0u) __builtin_trap();
        return 0;
    }
    if (reader.damaged()) __builtin_trap();

    av::RecordHeader    record;
    const std::uint8_t *payload = nullptr;
    const std::uint64_t most = size / av::RecordHeader::encoded_size + 1u;
    for (std::uint64_t answered = 0u;; answered++) {
        if (answered > most) __builtin_trap();
        const av::Status status = reader.next(record, payload);
        if (status == av::Status::not_found) {
            if (reader.damaged() || reader.offset() != size) __builtin_trap();
            break;
        }
        if (status == av::Status::malformed) {
            if (!reader.damaged() || reader.damage_offset() >= size) __builtin_trap();
            if (payload != nullptr) __builtin_trap();
            if (reader.next(record, payload) != av::Status::malformed) __builtin_trap();
            break;
        }
        if (status != av::Status::ok || payload == nullptr) __builtin_trap();
        if (payload + record.length() > data + size) __builtin_trap();
        std::uint8_t sum = 0u;
        for (std::uint32_t index = 0u; index < record.length(); index++) sum ^= payload[index];
        __asm__ volatile("" : : "r"(sum));
        if (!reader.invariants_hold()) __builtin_trap();
    }
    if (!reader.invariants_hold()) __builtin_trap();
    return 0;
}

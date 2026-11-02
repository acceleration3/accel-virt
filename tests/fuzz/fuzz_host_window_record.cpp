#include "core/support/status.hpp"
#include "proto/escape/host_window_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    using Operation = av::HostWindowRecord::Operation;
    std::uint8_t again[av::HostWindowRecord::encoded_size] = {};
    const av::HostMemoryRequest upload(av::HostMemoryRequest::Kind::upload, 0u);
    const av::HostWindowRecord  requests[] = {av::HostWindowRecord(Operation::open, 0x10000u, 0u, upload),
                                              av::HostWindowRecord(Operation::close, 0u, 0x00100003u,
                                                                   av::HostMemoryRequest())};
    for (av::HostWindowRecord request : requests) {
        if (request.accept_answer(data, size) != av::Status::ok) continue;
        if (request.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    }
    av::HostWindowRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    if (!record.opens()) return 0;
    const av::HostWindowRecord answer = record.answered(record.resource_id() + 1u, record.bytes(), 0x1000u);
    if (answer.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    av::HostWindowRecord reread;
    if (reread.decode(again, sizeof again) == av::Status::ok) __builtin_trap();
    return 0;
}

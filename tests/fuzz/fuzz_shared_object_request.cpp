/* The body of the three shared-object transport commands, and the answer the
 * device writes back, fed attacker-chosen bytes (feature 053).
 *
 * The body is guest-authored and decoded inside QEMU; the answer is
 * device-authored and decoded inside the guest's ICD, and a hostile or older
 * device is as foreign to the ICD as a guest is to the device. Both directions
 * are fuzzed here because both are readers of bytes they did not write. */
#include "core/support/status.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/submission/shared_object_request.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    std::uint8_t again[av::SharedObjectRequest::encoded_size] = {};

    av::SharedObjectRequest request;
    if (request.decode(data, size) == av::Status::ok) {
        if (request.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
        if (request.object_id() == 0u || !request.kind().known()) __builtin_trap();
    }

    av::SharedObjectAnswer answer;
    if (answer.decode(data, size) == av::Status::ok) {
        std::uint8_t answered[av::SharedObjectAnswer::encoded_size] = {};
        if (answer.encode(answered, sizeof answered) != av::Status::ok) __builtin_trap();
        if (std::memcmp(answered, data, sizeof answered) != 0) __builtin_trap();
        if (!answer.succeeded() && answer.wire_handle() != 0u) __builtin_trap();
        if (answer.status() > av::SharedObjectAnswer::conflict) __builtin_trap();
    }
    return 0;
}

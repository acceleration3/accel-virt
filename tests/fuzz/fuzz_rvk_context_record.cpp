/* The private data a process puts on D3DKMTCreateContextVirtual, fed
 * attacker-chosen bytes (features 043 and 050). What decodes must re-encode to
 * exactly the bytes read and carry only the options its sub-API may - every
 * one for OpenGL, the context it shares with for OpenCL, none otherwise - and
 * those must be ones the contract allows. */
#include "core/support/status.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::RvkContextRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;
    std::uint8_t again[av::RvkContextRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    const av::GlContextOptions options = record.options();
    const bool fits = record.sub_api() == av::SubApi::opengl   ? options.valid()
                      : record.sub_api() == av::SubApi::opencl ? options.shares_only()
                                                               : options.defaults();
    if (!fits) __builtin_trap();
    if (record.reply_bytes() == 0u || record.reply_bytes() > av::RvkContextRecord::max_reply_bytes) __builtin_trap();
    if (options.major() > 4u || (options.flags() & 0xF0u) != 0u) __builtin_trap();
    return 0;
}

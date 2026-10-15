#include "executor/opencl/spirv_argument_kinds.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* The SPIR-V numbers this reads (the SPIR-V specification, 3.32 and 3.7). */
enum : uint32_t {
    op_entry_point = 15u,
    op_type_image = 25u,
    op_type_sampler = 26u,
    op_type_sampled_image = 27u,
    op_type_pointer = 32u,
    op_type_event = 34u,
    op_type_device_event = 35u,
    op_type_reserve_id = 36u,
    op_type_queue = 37u,
    op_type_pipe = 38u,
    op_function = 54u,
    op_function_parameter = 55u,
    op_function_end = 56u,
};

enum : uint32_t {
    storage_uniform_constant = 0u,
    storage_workgroup = 4u,
    storage_cross_workgroup = 5u,
};

constexpr uint32_t execution_model_kernel = 6u;

}  // namespace

SpirvArgumentKinds::SpirvArgumentKinds() : types_(), bound_(0u)
{
    AV_SCOPE("SpirvArgumentKinds::SpirvArgumentKinds");
}

SpirvArgumentKinds::Literal::Literal(const uint32_t *words, uint32_t count) : words_(words), count_(count)
{
    AV_SCOPE("SpirvArgumentKinds::Literal::Literal");
}

bool SpirvArgumentKinds::Literal::spells(const char *text) const
{
    AV_SCOPE("SpirvArgumentKinds::Literal::spells");
    if (text == nullptr) return false;
    for (uint32_t at = 0u; at < count_ * 4u; ++at) {
        const char byte = static_cast<char>((words_[at / 4u] >> (8u * (at % 4u))) & 0xFFu);
        if (byte != text[at]) return false;
        if (byte == '\0') return true;
    }
    return false;
}

void SpirvArgumentKinds::record_type(const uint32_t *instruction, uint32_t length)
{
    AV_SCOPE("SpirvArgumentKinds::record_type");
    const uint32_t opcode = instruction[0] & 0xFFFFu;
    clproto::KernelArgument kind = clproto::KernelArgument::data;
    switch (opcode) {
    case op_type_pointer: {
        if (length < 4u) return;
        const uint32_t storage = instruction[2];
        if (storage == storage_cross_workgroup || storage == storage_uniform_constant)
            kind = clproto::KernelArgument::memory;
        else if (storage == storage_workgroup)
            kind = clproto::KernelArgument::local;
        break;
    }
    case op_type_image: kind = clproto::KernelArgument::image; break;
    case op_type_sampler: kind = clproto::KernelArgument::sampler; break;
    case op_type_pipe: kind = clproto::KernelArgument::memory; break;
    /* Device-side enqueue's objects and a sampled image are nothing a guest
     * can set: whatever bytes it sends for one are refused. */
    case op_type_sampled_image:
    case op_type_event:
    case op_type_device_event:
    case op_type_reserve_id:
    case op_type_queue: kind = clproto::KernelArgument::unknown; break;
    default: return;
    }
    if (length < 2u || instruction[1] >= bound_) return;
    types_.bytes()[instruction[1]] = static_cast<uint8_t>(static_cast<uint8_t>(kind) + 1u);
}

bool SpirvArgumentKinds::kind(const uint32_t *words, size_t count, const char *kernel, uint32_t index,
                              clproto::KernelArgument &argument)
{
    AV_SCOPE("SpirvArgumentKinds::kind");
    if (words == nullptr || count < header_words || words[0] != magic || words[3] == 0u || words[3] > most_ids)
        return false;
    bound_ = words[3];
    if (!types_.reserve(bound_)) return false;
    __builtin_memset(types_.bytes(), 0, bound_);
    uint32_t function = 0u;
    bool     inside = false;
    uint32_t parameter = 0u;
    for (size_t at = header_words; at < count;) {
        const uint32_t length = words[at] >> 16u;
        const uint32_t opcode = words[at] & 0xFFFFu;
        if (length == 0u || length > count - at) return false;
        const uint32_t *const instruction = words + at;
        at += length;
        if (opcode == op_entry_point && length >= 4u && instruction[1] == execution_model_kernel &&
            Literal(instruction + 3, length - 3u).spells(kernel))
            function = instruction[2];
        else if (opcode == op_function && length >= 5u)
            inside = function != 0u && instruction[2] == function;
        else if (opcode == op_function_end)
            inside = false;
        else if (opcode == op_function_parameter && inside && length >= 3u && parameter++ == index) {
            if (instruction[1] >= bound_) return false;
            const uint8_t recorded = types_.bytes()[instruction[1]];
            argument = recorded == 0u ? clproto::KernelArgument::data
                                      : static_cast<clproto::KernelArgument>(recorded - 1u);
            return true;
        } else
            record_type(instruction, length);
    }
    return false;
}

}  // namespace av

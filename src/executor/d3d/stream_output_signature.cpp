#include "executor/d3d/stream_output_signature.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr uint32_t float_type = 3u;
}

StreamOutputSignature::StreamOutputSignature(const d3dproto::SignatureEntry *outputs, uint32_t output_count,
                                             const d3dproto::StreamOutEntry *streamed, uint32_t streamed_count)
    : count_(0u)
{
    AV_SCOPE("StreamOutputSignature::StreamOutputSignature");
    for (uint32_t index = 0u; index < output_count && count_ < most_entries; index++) entries_[count_++] = outputs[index];
    for (uint32_t index = 0u; index < streamed_count; index++) {
        const d3dproto::StreamOutEntry &entry = streamed[index];
        if (entry.register_index == no_register) continue;
        d3dproto::SignatureEntry *found = nullptr;
        for (uint32_t at = 0u; at < count_ && found == nullptr; at++)
            if (entries_[at].register_index == entry.register_index && entries_[at].stream == entry.stream)
                found = &entries_[at];
        if (found != nullptr) {
            found->mask |= entry.mask;
            continue;
        }
        if (count_ == most_entries) continue;
        d3dproto::SignatureEntry made = {0u, entry.register_index, entry.mask, float_type, 0u, entry.stream};
        for (uint32_t at = 0u; at < count_; at++)
            if (entries_[at].register_index == entry.register_index) {
                made.system_value = entries_[at].system_value;
                made.component_type = entries_[at].component_type;
                made.min_precision = entries_[at].min_precision;
                break;
            }
        entries_[count_++] = made;
    }
}

const d3dproto::SignatureEntry *StreamOutputSignature::entries() const
{
    AV_SCOPE("StreamOutputSignature::entries");
    return entries_;
}

uint32_t StreamOutputSignature::count() const
{
    AV_SCOPE("StreamOutputSignature::count");
    return count_;
}

}

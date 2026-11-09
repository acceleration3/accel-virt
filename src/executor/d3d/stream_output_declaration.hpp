#ifndef AV_STREAM_OUTPUT_DECLARATION_HPP
#define AV_STREAM_OUTPUT_DECLARATION_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

template <typename Entry>
class StreamOutputDeclaration {
public:
    static constexpr uint32_t most_entries = 128u;
    static constexpr uint32_t most_buffers = 4u;
    static constexpr uint32_t most_streams = 4u;
    static constexpr uint32_t most_stride = 2048u;
    static constexpr uint32_t no_stream = 0xFFFFFFFFu;

    StreamOutputDeclaration(const Entry *entries, uint32_t count, const uint32_t *strides, uint32_t stride_count,
                            uint32_t rasterized_stream)
        : entries_(entries), count_(count), strides_(strides), stride_count_(stride_count),
          rasterized_(rasterized_stream)
    {
        AV_SCOPE("StreamOutputDeclaration::StreamOutputDeclaration");
    }

    bool holds() const
    {
        AV_SCOPE("StreamOutputDeclaration::holds");
        if (count_ > most_entries || stride_count_ > most_buffers) return false;
        if (count_ != 0u && entries_ == nullptr) return false;
        if (stride_count_ != 0u && strides_ == nullptr) return false;
        if (rasterized_ != no_stream && rasterized_ >= most_streams) return false;
        for (uint32_t index = 0u; index < stride_count_; index++)
            if (strides_[index] == 0u || strides_[index] % 4u != 0u || strides_[index] > most_stride) return false;
        uint32_t stream_of_buffer[most_buffers] = {};
        uint32_t bytes_of_buffer[most_buffers] = {};
        bool     used[most_buffers] = {};
        for (uint32_t index = 0u; index < count_; index++) {
            const Entry &entry = entries_[index];
            if (entry.stream >= most_streams || entry.output_slot >= most_buffers) return false;
            if (stride_count_ == 0u ? entry.output_slot != 0u : entry.output_slot >= stride_count_) return false;
            if (!contiguous(entries_[index].mask)) return false;
            if (used[entry.output_slot] && stream_of_buffer[entry.output_slot] != entry.stream) return false;
            used[entry.output_slot] = true;
            stream_of_buffer[entry.output_slot] = entry.stream;
            bytes_of_buffer[entry.output_slot] += component_count(index) * 4u;
            if (stride_count_ != 0u && bytes_of_buffer[entry.output_slot] > strides_[entry.output_slot]) return false;
        }
        return true;
    }

    uint32_t rasterized() const
    {
        AV_SCOPE("StreamOutputDeclaration::rasterized");
        return rasterized_;
    }

    uint32_t start_component(uint32_t index) const
    {
        AV_SCOPE("StreamOutputDeclaration::start_component");
        const uint32_t mask = entries_[index].mask & 0xFu;
        return mask == 0u ? 0u : static_cast<uint32_t>(__builtin_ctz(mask));
    }

    uint32_t component_count(uint32_t index) const
    {
        AV_SCOPE("StreamOutputDeclaration::component_count");
        return static_cast<uint32_t>(__builtin_popcount(entries_[index].mask & 0xFu));
    }

private:
    bool contiguous(uint32_t mask) const
    {
        AV_SCOPE("StreamOutputDeclaration::contiguous");
        const uint32_t low = mask & 0xFu;
        if (low == 0u) return false;
        const uint32_t run = low >> __builtin_ctz(low);
        return (run & (run + 1u)) == 0u;
    }

    const Entry    *entries_;
    uint32_t        count_;
    const uint32_t *strides_;
    uint32_t        stride_count_;
    uint32_t        rasterized_;
};

}

#endif

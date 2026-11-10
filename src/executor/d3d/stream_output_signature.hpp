#ifndef AV_STREAM_OUTPUT_SIGNATURE_HPP
#define AV_STREAM_OUTPUT_SIGNATURE_HPP

#include "proto/generated/d3d/d3d_wire_types.h"

#include <stdint.h>

namespace av {

class StreamOutputSignature {
public:
    static constexpr uint32_t most_entries = 256u;
    static constexpr uint32_t no_register = 0xFFFFFFFFu;

    StreamOutputSignature(const d3dproto::SignatureEntry *outputs, uint32_t output_count,
                          const d3dproto::StreamOutEntry *streamed, uint32_t streamed_count);
    StreamOutputSignature(const StreamOutputSignature &) = delete;
    StreamOutputSignature &operator=(const StreamOutputSignature &) = delete;

    [[nodiscard]] const d3dproto::SignatureEntry *entries() const;
    [[nodiscard]] uint32_t                        count() const;

private:
    d3dproto::SignatureEntry entries_[most_entries];
    uint32_t                 count_;
};

}

#endif

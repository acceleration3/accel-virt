/* Which of a recording's records a reader is being shown: a context, a
 * sub-API, a fence range and a fragment of a command's name, each of which
 * keeps everything until it is set (contracts/rvkrecord.md). */
#ifndef AV_RECORDING_FILTER_HPP
#define AV_RECORDING_FILTER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RecordedCommand;
class RecordHeader;

struct RecordingFilterProbe;

class RecordingFilter {
public:
    static constexpr uint32_t any_context = 0xFFFFFFFFu;
    static constexpr uint16_t any_api = 0xFFFFu;
    static constexpr size_t   longest_fragment = 64u;

    RecordingFilter();

    void                 keep_context(uint32_t context);
    /* "transport", "vulkan", "opencl" or "opengl"; invalid_parameter for
     * anything else, leaving the filter alone. */
    [[nodiscard]] Status keep_api(const char *name);
    /* Records whose fence is within [first, last]; invalid_parameter for a
     * range that ends before it starts. */
    [[nodiscard]] Status keep_fences(uint64_t first, uint64_t last);
    /* Batches holding a command whose name contains `fragment`;
     * invalid_parameter for a fragment longer than any name. */
    [[nodiscard]] Status keep_command(const char *fragment);

    uint32_t context() const;
    uint16_t api() const;
    /* Whether any command name has to match for a batch to be shown. */
    bool     names_a_command() const;

    /* A record with no fence - a setup, a blob - is inside every fence range:
     * a range narrows the batches it can narrow and hides nothing else. */
    bool keeps(const RecordHeader &record) const;
    bool keeps(const RecordedCommand &command) const;

    bool invariants_hold() const;

private:
    uint32_t context_;
    uint16_t api_;
    uint64_t first_fence_;
    uint64_t last_fence_;
    char     fragment_[longest_fragment];

    friend struct RecordingFilterProbe;
};

}  // namespace av

#endif  // AV_RECORDING_FILTER_HPP

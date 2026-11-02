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
    [[nodiscard]] Status keep_api(const char *name);
    [[nodiscard]] Status keep_fences(uint64_t first, uint64_t last);
    [[nodiscard]] Status keep_command(const char *fragment);

    uint32_t context() const;
    uint16_t api() const;
    bool     names_a_command() const;

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

}

#endif

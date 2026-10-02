/* What `avrecord` was asked to do (contracts/avrecord.md), read from the
 * arguments it was started with: print a recording or replay it, which file,
 * which library, which GPU, how many payload bytes to show, and the filter the
 * flags narrow. Everything it does not understand is a refusal with a reason,
 * because a tool that guessed at its arguments would print the wrong
 * recording. */
#ifndef AV_RECORDING_TOOL_REQUEST_HPP
#define AV_RECORDING_TOOL_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/context/device_uuid.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RecordingFilter;

struct RecordingToolRequestProbe;

class RecordingToolRequest {
public:
    enum class Action : uint8_t { none, print, replay };

    static constexpr size_t uuid_bytes = DeviceUuid::bytes;
    static constexpr size_t uuid_digits = 2u * uuid_bytes;

    RecordingToolRequest();

    /* `arguments` as the shell gave them, `count` of them including the
     * program's own name; `filter` is narrowed by the flags that narrow it.
     * invalid_parameter, with reason() saying which argument and why. */
    [[nodiscard]] Status parse(int count, const char *const *arguments, RecordingFilter &filter);

    Action      action() const;
    const char *recording() const;
    /* The executor library `replay` was given, or null. */
    const char *executor() const;
    /* The sixteen bytes of --gpu, or null when it was not given. */
    const uint8_t *gpu() const;
    uint32_t       payload_bytes() const;
    /* Why it refused, or the empty string. */
    const char    *reason() const;

    bool invariants_hold() const;

private:
    /* One flag and its value: ok, or invalid_parameter with the reason set. */
    [[nodiscard]] Status take_flag(const char *flag, const char *value, RecordingFilter &filter);
    [[nodiscard]] Status take_fences(const char *value, RecordingFilter &filter);
    [[nodiscard]] Status take_gpu(const char *value);
    [[nodiscard]] Status refuse(const char *why);

    Action         action_;
    const char    *recording_;
    const char    *executor_;
    uint32_t       payload_bytes_;
    bool           gpu_given_;
    uint8_t        gpu_[uuid_bytes];
    const char    *reason_;

    friend struct RecordingToolRequestProbe;
};

}  // namespace av

#endif  // AV_RECORDING_TOOL_REQUEST_HPP

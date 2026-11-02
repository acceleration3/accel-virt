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

    [[nodiscard]] Status parse(int count, const char *const *arguments, RecordingFilter &filter);

    Action      action() const;
    const char *recording() const;
    const char *executor() const;
    const uint8_t *gpu() const;
    uint32_t       payload_bytes() const;
    const char    *reason() const;

    bool invariants_hold() const;

private:
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

}

#endif

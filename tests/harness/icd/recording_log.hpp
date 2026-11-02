#ifndef AV_RECORDING_LOG_HPP
#define AV_RECORDING_LOG_HPP

#include "core/log/log_sink.hpp"

#include <cstdint>

namespace av {

class RecordingLog final : public LogSink {
public:
    static constexpr std::uint32_t max_lines = 32u;
    static constexpr std::uint32_t line_bytes = 160u;

    RecordingLog();

    void write(const char *line) override;

    std::uint32_t lines() const;
    const char   *line(std::uint32_t index) const;
    std::uint32_t holding(const char *text) const;

private:
    std::uint32_t lines_;
    char          text_[max_lines][line_bytes];
};

}

#endif

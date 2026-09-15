/* The ICD's LogSink in a test: every line kept, up to a bound, so a test asserts
 * the refusal it provoked was written once and says what decided it. */
#ifndef AV_RECORDING_LOG_HPP
#define AV_RECORDING_LOG_HPP

#include "icd/vulkan/log_sink.hpp"

#include <cstdint>

namespace av {

class RecordingLog final : public icd::LogSink {
public:
    static constexpr std::uint32_t max_lines = 32u;
    static constexpr std::uint32_t line_bytes = 160u;

    RecordingLog();

    /* Past max_lines a line is counted but not kept; a longer line is cut. */
    void write(const char *line) override;

    std::uint32_t lines() const;
    /* The line at `index`, empty past what was kept. */
    const char   *line(std::uint32_t index) const;
    /* How many kept lines hold `text`. */
    std::uint32_t holding(const char *text) const;

private:
    std::uint32_t lines_;
    char          text_[max_lines][line_bytes];
};

}  // namespace av

#endif  // AV_RECORDING_LOG_HPP

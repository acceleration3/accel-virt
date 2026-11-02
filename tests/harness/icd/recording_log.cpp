#include "harness/icd/recording_log.hpp"

#include <cstring>

namespace av {

RecordingLog::RecordingLog() : lines_(0u), text_{} {}

void RecordingLog::write(const char *line)
{
    if (lines_ < max_lines) {
        std::strncpy(text_[lines_], line, line_bytes - 1u);
        text_[lines_][line_bytes - 1u] = '\0';
    }
    lines_++;
}

std::uint32_t RecordingLog::lines() const { return lines_; }

const char *RecordingLog::line(std::uint32_t index) const
{
    return index < max_lines ? text_[index] : "";
}

std::uint32_t RecordingLog::holding(const char *text) const
{
    std::uint32_t       found = 0u;
    const std::uint32_t kept = lines_ < max_lines ? lines_ : max_lines;
    for (std::uint32_t index = 0u; index < kept; index++)
        if (std::strstr(text_[index], text) != nullptr) found++;
    return found;
}

}

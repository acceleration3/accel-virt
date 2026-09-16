/* LogSink appending to C:\ProgramData\rvk\rvkvk-<pid>.log, which the guest
 * runner collects into diag/ (constitution IX). One file per process, so two
 * applications never interleave; each line is one append, which Windows keeps
 * whole between threads. A log that cannot be opened drops its lines: the ICD's
 * answers never depend on it. Translation only. */
#ifndef AV_UM_FILE_LOG_HPP
#define AV_UM_FILE_LOG_HPP

#include "icd/vulkan/log_sink.hpp"

namespace av {

class FileLog final : public icd::LogSink {
public:
    FileLog();
    ~FileLog();
    FileLog(const FileLog &) = delete;
    FileLog &operator=(const FileLog &) = delete;

    /* Creates the directory when it is missing. */
    void open();
    void write(const char *line) override;

private:
    void *file_;
};

}  // namespace av

#endif  // AV_UM_FILE_LOG_HPP

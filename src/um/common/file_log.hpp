/* LogSink appending to C:\ProgramData\av\<stem>-<pid>.log, which the guest
 * runner collects into diag/ (constitution IX). One file per process, so two
 * applications never interleave; each line is one append, which Windows keeps
 * whole between threads. A log that cannot be opened drops its lines: the ICD's
 * answers never depend on it. Translation only. */
#ifndef AV_UM_FILE_LOG_HPP
#define AV_UM_FILE_LOG_HPP

#include "core/log/log_sink.hpp"

namespace av {

class FileLog final : public LogSink {
public:
    /* `stem` names the ICD (avvk, avgl) and outlives the log. */
    explicit FileLog(const wchar_t *stem);
    ~FileLog();
    FileLog(const FileLog &) = delete;
    FileLog &operator=(const FileLog &) = delete;

    /* Creates the directory when it is missing. */
    void open();
    void write(const char *line) override;

private:
    const wchar_t *stem_;
    void          *file_;
};

}  // namespace av

#endif  // AV_UM_FILE_LOG_HPP

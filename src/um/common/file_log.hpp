#ifndef AV_UM_FILE_LOG_HPP
#define AV_UM_FILE_LOG_HPP

#include "core/log/log_sink.hpp"
#include "um/common/diagnostics_directory.hpp"

namespace av {

class FileLog final : public LogSink {
public:
    explicit FileLog(const wchar_t *stem);
    ~FileLog();
    FileLog(const FileLog &) = delete;
    FileLog &operator=(const FileLog &) = delete;

    void open();
    void write(const char *line) override;

private:
    DiagnosticsDirectory directory_;
    const wchar_t       *stem_;
    void                *file_;
};

}

#endif

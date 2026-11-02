#ifndef AV_LOG_SINK_HPP
#define AV_LOG_SINK_HPP

namespace av {

class LogSink {
public:
    LogSink(const LogSink &) = delete;
    LogSink &operator=(const LogSink &) = delete;

    virtual void write(const char *line) = 0;

protected:
    LogSink() = default;
    ~LogSink() = default;
};

}

#endif

#ifndef AV_REPORT_GATE_HPP
#define AV_REPORT_GATE_HPP

namespace av {

class StateWriter;
class ThreadLock;

class ReportGate {
public:
    ReportGate(ThreadLock &lock, StateWriter &writer, const char *section);
    ~ReportGate();
    ReportGate(const ReportGate &) = delete;
    ReportGate &operator=(const ReportGate &) = delete;

    bool held() const;

private:
    ThreadLock &lock_;
    bool        held_;
};

}

#endif

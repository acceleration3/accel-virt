/* A state dump's hold on the lock its root's state is read under. The dump
 * runs on the trace thread when the process may be hung with that lock held,
 * so it never waits: it opens its section, tries the lock, and says whether
 * the root was busy - a busy root's section closes with nothing more in it,
 * since what it guards could be mid-change. Every driver root's dump starts
 * this way (IcdRoot, GlIcdRoot, ClIcdRoot, UmdRoot), its section named after
 * its driver. */
#ifndef AV_REPORT_GATE_HPP
#define AV_REPORT_GATE_HPP

namespace av {

class StateWriter;
class ThreadLock;

class ReportGate {
public:
    /* `section` opened on `writer`, with busy=true and closed again when
     * `lock` is held elsewhere, busy=false and the lock held otherwise. */
    ReportGate(ThreadLock &lock, StateWriter &writer, const char *section);
    ~ReportGate();
    ReportGate(const ReportGate &) = delete;
    ReportGate &operator=(const ReportGate &) = delete;

    /* The lock is held: the root's state may be read. */
    bool held() const;

private:
    ThreadLock &lock_;
    bool        held_;
};

}  // namespace av

#endif  // AV_REPORT_GATE_HPP

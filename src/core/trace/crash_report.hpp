/* A crash file, one line at a time and without allocating, from inside the
 * crashing process's exception filter: the exception with the module it hit,
 * the process's own trace table through TraceText, then the faulting thread's
 * native return addresses, each with its module and offset. */
#ifndef AV_CRASH_REPORT_HPP
#define AV_CRASH_REPORT_HPP

#include "core/trace/crash_event.hpp"

#include <stdint.h>

namespace av {

class ModuleResolver;
struct CrashReportProbe;
class TextBuffer;
class TraceText;

class CrashReport {
public:
    static constexpr uint32_t max_frames = 48u;
    static constexpr uint32_t module_chars = 64u;

    /* `returns` holds `count` addresses, of which the first max_frames are
     * written. */
    CrashReport(const CrashEvent &event, TraceText &trace, ModuleResolver &modules, const uint64_t *returns,
                uint32_t count);
    CrashReport(const CrashReport &) = delete;
    CrashReport &operator=(const CrashReport &) = delete;

    /* The next line into `out`; false once every line was written. */
    bool next(TextBuffer &out);

    bool invariants_hold() const;

private:
    enum class Part : uint8_t { crash, trace, native, done };

    void located(TextBuffer &out, uint64_t address) const;

    CrashEvent      event_;
    TraceText      &trace_;
    ModuleResolver &modules_;
    const uint64_t *returns_;
    uint32_t        count_;
    Part            part_;
    uint32_t        frame_;

    friend struct CrashReportProbe;
};

}  // namespace av

#endif  // AV_CRASH_REPORT_HPP

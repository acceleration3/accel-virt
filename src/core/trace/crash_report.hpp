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

    CrashReport(const CrashEvent &event, TraceText &trace, ModuleResolver &modules, const uint64_t *returns,
                uint32_t count);
    CrashReport(const CrashReport &) = delete;
    CrashReport &operator=(const CrashReport &) = delete;

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

}

#endif

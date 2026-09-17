#include "core/trace/crash_report.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/module_resolver.hpp"
#include "core/trace/trace_text.hpp"

namespace av {

namespace {

constexpr uint32_t code_digits = 8u;
constexpr uint32_t address_digits = 16u;

}  // namespace

CrashReport::CrashReport(const CrashEvent &event, TraceText &trace, ModuleResolver &modules,
                         const uint64_t *returns, uint32_t count)
    : event_(event), trace_(trace), modules_(modules), returns_(returns),
      count_(returns == nullptr ? 0u : count < max_frames ? count : max_frames), part_(Part::crash), frame_(0u)
{
}

bool CrashReport::next(TextBuffer &out)
{
    if (part_ == Part::crash) {
        out.append("crash module=");
        out.append(event_.module());
        out.append(" pid=");
        out.append_decimal(event_.pid());
        out.append(" code=0x");
        out.append_hex(event_.code(), code_digits);
        out.append(" address=");
        located(out, event_.address());
        part_ = Part::trace;
        return true;
    }
    if (part_ == Part::trace) {
        if (trace_.next(out)) return true;
        part_ = Part::native;
    }
    if (part_ == Part::native && frame_ < count_) {
        out.append("native #");
        out.append_decimal(frame_);
        out.append(" ");
        located(out, returns_[frame_]);
        frame_++;
        return true;
    }
    part_ = Part::done;
    return false;
}

bool CrashReport::invariants_hold() const
{
    return count_ <= max_frames && frame_ <= count_ && (returns_ != nullptr || count_ == 0u);
}

/* `0x<address> (<module>+0x<offset>)`, or the address alone in no module. */
void CrashReport::located(TextBuffer &out, uint64_t address) const
{
    out.append("0x");
    out.append_hex(address, address_digits);
    char     module[module_chars] = {};
    uint64_t offset = 0u;
    if (!modules_.resolve(address, module, sizeof module, offset)) return;
    module[module_chars - 1u] = '\0';
    out.append(" (");
    out.append(module);
    out.append("+0x");
    out.append_hex(offset, address_digits);
    out.append(")");
}

}  // namespace av

#include "guest/suite/guest_report.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

namespace {

constexpr uint32_t code_digits = 8u;

}  // namespace

GuestReport::GuestReport(BlockSource &memory) : GuestReport(memory, "avtest")
{
    AV_SCOPE("GuestReport::GuestReport");
}

GuestReport::GuestReport(BlockSource &memory, const char *program)
    : program_(program), text_(memory, Pool::paged), passed_(0u), failed_(0u), last_scenario_(""), last_step_(""),
      finished_(false)
{
    AV_SCOPE("GuestReport::GuestReport");
}

/* Room for the whole line and the terminator first, so a line is kept whole
 * or not at all. */
void GuestReport::keep(const TextBuffer &written)
{
    AV_SCOPE("GuestReport::keep");
    const auto kept = static_cast<uint32_t>(length());
    const auto bytes = static_cast<uint32_t>(written.length());
    if (text_.reserve(kept + bytes + 1u) != Status::ok) return;
    text_.truncate(kept);
    for (uint32_t index = 0u; index < bytes; index++) static_cast<void>(text_.append(written.text()[index]));
    static_cast<void>(text_.append('\0'));
}

void GuestReport::check(const char *scenario, const char *step, Status status, uint32_t code)
{
    AV_SCOPE("GuestReport::check");
    line(scenario, step, status == Status::ok, status, code);
}

void GuestReport::check_refused(const char *scenario, const char *step, Status status, uint32_t code)
{
    AV_SCOPE("GuestReport::check_refused");
    line(scenario, step, status != Status::ok, status, code);
}

void GuestReport::measure(const char *scenario, const char *step, Status status, uint32_t measurement)
{
    AV_SCOPE("GuestReport::measure");
    const bool passed = status == Status::ok;
    last_scenario_ = scenario;
    last_step_ = step;
    char       bytes[line_bytes];
    TextBuffer written(bytes, sizeof bytes);
    written.append(passed ? "ok " : "FAIL ");
    written.append(scenario);
    written.append(".");
    written.append(step);
    if (!passed) {
        written.append(" status=");
        written.append_decimal(static_cast<uint64_t>(status));
    }
    written.append(" ");
    written.append_decimal(measurement);
    written.append("\n");
    keep(written);
    if (passed) passed_++;
    else failed_++;
}

void GuestReport::line(const char *scenario, const char *step, bool passed, Status status, uint32_t code)
{
    AV_SCOPE("GuestReport::line");
    last_scenario_ = scenario;
    last_step_ = step;
    char       bytes[line_bytes];
    TextBuffer written(bytes, sizeof bytes);
    written.append(passed ? "ok " : "FAIL ");
    written.append(scenario);
    written.append(".");
    written.append(step);
    if (!passed) {
        written.append(" status=");
        written.append_decimal(static_cast<uint64_t>(status));
        written.append(" code=0x");
        written.append_hex(code, code_digits);
    }
    written.append("\n");
    keep(written);
    if (passed) passed_++;
    else failed_++;
}

void GuestReport::note(const char *scenario, const char *key, const char *text)
{
    AV_SCOPE("GuestReport::note");
    char       bytes[line_bytes];
    TextBuffer written(bytes, sizeof bytes);
    written.append("note ");
    written.append(scenario);
    written.append(".");
    written.append(key);
    written.append(" ");
    if (text == nullptr) text = "(none)";
    char   kept[note_bytes + 1u];
    size_t length = 0u;
    while (length < note_bytes && text[length] != '\0' && text[length] != '\n' && text[length] != '\r') {
        kept[length] = text[length];
        length++;
    }
    kept[length] = '\0';
    written.append(kept);
    written.append("\n");
    keep(written);
}

void GuestReport::finish()
{
    AV_SCOPE("GuestReport::finish");
    finished_ = true;
    char       bytes[line_bytes];
    TextBuffer written(bytes, sizeof bytes);
    written.append(program_);
    written.append(": ");
    written.append_decimal(passed_);
    written.append(" passed, ");
    written.append_decimal(failed_);
    written.append(" failed\n");
    keep(written);
}

const char *GuestReport::text() const
{
    AV_SCOPE("GuestReport::text");
    return length() != 0u ? text_.begin() : "";
}
size_t      GuestReport::length() const
{
    AV_SCOPE("GuestReport::length");
    return text_.size() != 0u ? text_.size() - 1u : 0u;
}
uint32_t    GuestReport::passed() const
{
    AV_SCOPE("GuestReport::passed");
    return passed_;
}
uint32_t    GuestReport::failed() const
{
    AV_SCOPE("GuestReport::failed");
    return failed_;
}

void GuestReport::report(StateWriter &writer) const
{
    AV_SCOPE("GuestReport::report");
    writer.open("report", 0u);
    writer.text("program", program_);
    writer.number("passed", passed_);
    writer.number("failed", failed_);
    writer.text("last_scenario", last_scenario_);
    writer.text("last_step", last_step_);
    writer.flag("finished", finished_);
    writer.close();
}

bool GuestReport::invariants_hold() const
{
    AV_SCOPE("GuestReport::invariants_hold");
    if (!text_.invariants_hold()) return false;
    if (text_.size() == 0u) return true;
    for (uint32_t index = 0u; index + 1u < text_.size(); index++)
        if (text_.at(index) == '\0') return false;
    return text_.at(text_.size() - 1u) == '\0';
}

}  // namespace av

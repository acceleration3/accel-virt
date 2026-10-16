/* C:\ProgramData\av: where every guest driver and program leaves what the
 * guest runner collects into diag/ - logs, call traces, event rings - and
 * where it finds the flag files the runner leaves to ask for a trace
 * (docs/testing.md, "Knobs"). The one place the directory is spelled. */
#ifndef AV_UM_DIAGNOSTICS_DIRECTORY_HPP
#define AV_UM_DIAGNOSTICS_DIRECTORY_HPP

#include <stddef.h>

namespace av {

class DiagnosticsDirectory {
public:
    /* Characters of the directory and the separator after it, before a name. */
    static constexpr size_t prefix_chars = 18u;

    DiagnosticsDirectory();

    /* Makes the directory when it is missing. */
    void   make() const;
    /* Whether the runner left the flag file `name` there. */
    bool   flagged(const wchar_t *name) const;
    /* The path of `name` in the directory, written with its NUL into `path`
     * of `capacity` characters: its length, or 0 when it does not fit. */
    size_t place(const wchar_t *name, wchar_t *path, size_t capacity) const;

private:
    const wchar_t *root_;
};

}  // namespace av

#endif  // AV_UM_DIAGNOSTICS_DIRECTORY_HPP

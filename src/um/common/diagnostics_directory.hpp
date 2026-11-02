#ifndef AV_UM_DIAGNOSTICS_DIRECTORY_HPP
#define AV_UM_DIAGNOSTICS_DIRECTORY_HPP

#include <stddef.h>

namespace av {

class DiagnosticsDirectory {
public:
    static constexpr size_t prefix_chars = 18u;

    DiagnosticsDirectory();

    void   make() const;
    bool   flagged(const wchar_t *name) const;
    size_t place(const wchar_t *name, wchar_t *path, size_t capacity) const;

private:
    const wchar_t *root_;
};

}

#endif

#ifndef AV_UM_WIDE_NAME_HPP
#define AV_UM_WIDE_NAME_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class WideName {
public:
    static constexpr size_t max_chars = 128u;

    WideName();
    WideName(const char *scope, const char *kind, const char *module, uint32_t pid);

    void           append(const char *text);
    void           append(uint32_t number);
    const wchar_t *text() const;

private:
    wchar_t chars_[max_chars];
    size_t  length_;
};

}

#endif

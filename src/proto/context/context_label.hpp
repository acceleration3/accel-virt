#ifndef AV_CONTEXT_LABEL_HPP
#define AV_CONTEXT_LABEL_HPP

#include <stddef.h>

namespace av {

class ContextLabel {
public:
    static constexpr size_t capacity = 64u;

    explicit ContextLabel(const char *text);

    const char *text() const;

private:
    char text_[capacity + 1u];
};

}

#endif

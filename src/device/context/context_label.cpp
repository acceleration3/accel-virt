#include "device/context/context_label.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ContextLabel::ContextLabel(const char *text) : text_{}
{
    AV_SCOPE("ContextLabel::ContextLabel");
    for (size_t at = 0u; text != nullptr && text[at] != '\0' && at < capacity; at++) text_[at] = text[at];
}

const char *ContextLabel::text() const
{
    AV_SCOPE("ContextLabel::text");
    return text_;
}

}

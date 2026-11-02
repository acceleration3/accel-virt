#include "icd/opengl/gl_extension_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

GlExtensionList::GlExtensionList(BlockSource &memory) : names_(memory, Pool::paged), joined_(memory, Pool::paged)
{
    AV_SCOPE("GlExtensionList::GlExtensionList");
}

bool GlExtensionList::add(const char *name)
{
    AV_SCOPE("GlExtensionList::add");
    if (names_.holds(name) || names_.reserve(names_.count() + 1u) != Status::ok) return false;
    if (joined_.add(name) != Status::ok) return false;
    static_cast<void>(names_.add(name));
    return true;
}

uint32_t GlExtensionList::count() const
{
    AV_SCOPE("GlExtensionList::count");
    return names_.count();
}

const char *GlExtensionList::name(uint32_t index) const
{
    AV_SCOPE("GlExtensionList::name");
    return names_.at(index);
}

const char *GlExtensionList::joined() const
{
    AV_SCOPE("GlExtensionList::joined");
    return joined_.text();
}

bool GlExtensionList::invariants_hold() const
{
    AV_SCOPE("GlExtensionList::invariants_hold");
    if (!names_.invariants_hold() || !joined_.invariants_hold()) return false;
    const char *text = joined_.text();
    for (uint32_t index = 0u; index < names_.count(); index++) {
        if (index != 0u && *text++ != ' ') return false;
        for (const char *letter = names_.at(index); *letter != '\0'; letter++)
            if (*text++ != *letter) return false;
    }
    return *text == '\0';
}

}

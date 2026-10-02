#include "proto/resources/cl_blob_backing.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClBlobBacking::ClBlobBacking() : offset_(0u), held_(false)
{
    AV_SCOPE("ClBlobBacking::ClBlobBacking");
}

void ClBlobBacking::set(uint64_t offset)
{
    AV_SCOPE("ClBlobBacking::set");
    offset_ = offset;
    held_ = true;
}

bool ClBlobBacking::read(const uint64_t *properties)
{
    AV_SCOPE("ClBlobBacking::read");
    offset_ = 0u;
    held_ = false;
    if (properties == nullptr) return false;
    /* Bounded because the list comes from the guest: a list with no
     * terminator must end the walk rather than run off the end of the
     * decoder's arena. */
    for (uint32_t word = 0u; word + 1u < max_words && properties[word] != 0u; word += 2u) {
        if (properties[word] != property) continue;
        /* Named twice is a list the device would have to choose from, and
         * choosing is what it must not do. */
        if (held_) {
            offset_ = 0u;
            held_ = false;
            return false;
        }
        offset_ = properties[word + 1u];
        held_ = true;
    }
    return held_;
}

bool ClBlobBacking::write(uint64_t *list) const
{
    AV_SCOPE("ClBlobBacking::write");
    if (list == nullptr || !held_) return false;
    list[0] = property;
    list[1] = offset_;
    list[2] = 0u;
    return true;
}

bool ClBlobBacking::held() const
{
    AV_SCOPE("ClBlobBacking::held");
    return held_;
}

uint64_t ClBlobBacking::offset() const
{
    AV_SCOPE("ClBlobBacking::offset");
    return offset_;
}

bool ClBlobBacking::invariants_hold() const
{
    AV_SCOPE("ClBlobBacking::invariants_hold");
    return held_ || offset_ == 0u;
}

}  // namespace av

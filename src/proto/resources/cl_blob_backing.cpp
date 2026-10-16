#include "proto/resources/cl_blob_backing.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClBlobBacking::ClBlobBacking() : offset_(0u), memory_(0u), held_(false)
{
    AV_SCOPE("ClBlobBacking::ClBlobBacking");
}

void ClBlobBacking::set(uint64_t offset)
{
    AV_SCOPE("ClBlobBacking::set");
    offset_ = offset;
    memory_ = 0u;
    held_ = true;
}

void ClBlobBacking::set_in(uint64_t memory, uint64_t offset)
{
    AV_SCOPE("ClBlobBacking::set_in");
    offset_ = offset;
    memory_ = memory;
    held_ = true;
}

bool ClBlobBacking::refuse()
{
    AV_SCOPE("ClBlobBacking::refuse");
    offset_ = 0u;
    memory_ = 0u;
    held_ = false;
    return false;
}

bool ClBlobBacking::read(const uint64_t *properties)
{
    AV_SCOPE("ClBlobBacking::read");
    refuse();
    if (properties == nullptr) return false;
    bool named_memory = false;
    /* Bounded because the list comes from the guest: a list with no
     * terminator must end the walk rather than run off the end of the
     * decoder's arena. Named twice is a list the device would have to choose
     * from, and choosing is what it must not do. */
    for (uint32_t word = 0u; word + 1u < max_words && properties[word] != 0u; word += 2u) {
        if (properties[word] == property) {
            if (held_) return refuse();
            offset_ = properties[word + 1u];
            held_ = true;
        } else if (properties[word] == import_property) {
            if (named_memory || properties[word + 1u] == 0u) return refuse();
            memory_ = properties[word + 1u];
            named_memory = true;
        }
    }
    /* An import with no offset into it says nothing. */
    if (named_memory && !held_) return refuse();
    return held_;
}

bool ClBlobBacking::write(uint64_t *list) const
{
    AV_SCOPE("ClBlobBacking::write");
    if (list == nullptr || !held_) return false;
    list[0] = property;
    list[1] = offset_;
    list[2] = 0u;
    if (memory_ == 0u) return true;
    list[2] = import_property;
    list[3] = memory_;
    list[4] = 0u;
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

uint64_t ClBlobBacking::memory() const
{
    AV_SCOPE("ClBlobBacking::memory");
    return memory_;
}

bool ClBlobBacking::invariants_hold() const
{
    AV_SCOPE("ClBlobBacking::invariants_hold");
    return held_ || (offset_ == 0u && memory_ == 0u);
}

}  // namespace av

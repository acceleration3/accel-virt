#include "icd/opencl/mapped_region.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

MappedRegion::MappedRegion()
    : buffer_(0u), host_(0u), at_(nullptr), offset_(0u), bytes_(0u), flags_(0u), staged_(false)
{
    AV_SCOPE("MappedRegion::MappedRegion");
}

void MappedRegion::take(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                        cl_map_flags flags, uint8_t *at, bool staged)
{
    AV_SCOPE("MappedRegion::take");
    buffer_ = buffer;
    host_ = host;
    at_ = at;
    offset_ = offset;
    bytes_ = bytes;
    flags_ = flags;
    staged_ = staged;
}

void MappedRegion::give_up()
{
    AV_SCOPE("MappedRegion::give_up");
    buffer_ = 0u;
    host_ = 0u;
    at_ = nullptr;
    offset_ = 0u;
    bytes_ = 0u;
    flags_ = 0u;
    staged_ = false;
}

bool MappedRegion::held() const
{
    AV_SCOPE("MappedRegion::held");
    return at_ != nullptr;
}

uint64_t MappedRegion::buffer() const
{
    AV_SCOPE("MappedRegion::buffer");
    return buffer_;
}

uint64_t MappedRegion::host() const
{
    AV_SCOPE("MappedRegion::host");
    return host_;
}

size_t MappedRegion::offset() const
{
    AV_SCOPE("MappedRegion::offset");
    return offset_;
}

size_t MappedRegion::bytes() const
{
    AV_SCOPE("MappedRegion::bytes");
    return bytes_;
}

cl_map_flags MappedRegion::flags() const
{
    AV_SCOPE("MappedRegion::flags");
    return flags_;
}

uint8_t *MappedRegion::at() const
{
    AV_SCOPE("MappedRegion::at");
    return at_;
}

bool MappedRegion::staged() const
{
    AV_SCOPE("MappedRegion::staged");
    return staged_;
}

bool MappedRegion::answers(const void *pointer) const
{
    AV_SCOPE("MappedRegion::answers");
    /* The pointer the map returned, and not an address inside it: an unmap
     * takes what it was given. */
    return at_ != nullptr && pointer == at_;
}

bool MappedRegion::writes_back() const
{
    AV_SCOPE("MappedRegion::writes_back");
    /* CL_MAP_READ alone is a promise not to write, so nothing goes back;
     * anything else may have been written through. */
    return staged_ && (flags_ & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION)) != 0u;
}

bool MappedRegion::invariants_hold() const
{
    AV_SCOPE("MappedRegion::invariants_hold");
    if (at_ == nullptr) return buffer_ == 0u && bytes_ == 0u && offset_ == 0u && !staged_;
    /* A mapping of nothing is not a mapping, and one that names neither a
     * buffer nor the host's own address could never be unmapped. */
    return bytes_ != 0u && buffer_ != 0u && host_ != 0u;
}

}  // namespace av::clicd

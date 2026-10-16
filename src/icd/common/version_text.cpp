#include "icd/common/version_text.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av::icd {

VersionText::VersionText(const char *prefix, uint32_t most_major, uint32_t most_minor)
    : prefix_(prefix), most_major_(most_major), most_minor_(most_minor), major_(most_major), minor_(most_minor)
{
    AV_SCOPE("VersionText::VersionText");
}

bool VersionText::read_number(const char *host, size_t &at, size_t end, uint32_t &number) const
{
    AV_SCOPE("VersionText::read_number");
    const size_t first = at;
    number = 0u;
    while (at < end && host[at] >= '0' && host[at] <= '9' && at - first < most_digits)
        number = number * 10u + static_cast<uint32_t>(host[at++] - '0');
    return at != first && (at == end || host[at] < '0' || host[at] > '9');
}

Status VersionText::read(const char *host, size_t bytes, const char *&rest)
{
    AV_SCOPE("VersionText::read");
    if (host == nullptr) return Status::malformed;
    size_t end = 0u;
    while (end < bytes && host[end] != '\0') end++;
    /* No terminator in the bytes the host gave: whatever follows is somebody
     * else's memory, not the rest of a version. */
    if (end == bytes) return Status::malformed;
    size_t at = 0u;
    for (; prefix_[at] != '\0'; at++)
        if (at == end || host[at] != prefix_[at]) return Status::malformed;
    uint32_t major = 0u;
    uint32_t minor = 0u;
    if (!read_number(host, at, end, major) || at == end || host[at] != '.') return Status::malformed;
    at++;
    if (!read_number(host, at, end, minor)) return Status::malformed;
    cap(major, minor);
    rest = host + at;
    return Status::ok;
}

void VersionText::cap(uint32_t major, uint32_t minor)
{
    AV_SCOPE("VersionText::cap");
    const bool implemented = at_most_implemented(major, minor);
    major_ = implemented ? major : most_major_;
    minor_ = implemented ? minor : most_minor_;
}

uint32_t VersionText::major() const
{
    AV_SCOPE("VersionText::major");
    return major_;
}

uint32_t VersionText::minor() const
{
    AV_SCOPE("VersionText::minor");
    return minor_;
}

void VersionText::write(char *into, size_t bytes, const char *tail) const
{
    AV_SCOPE("VersionText::write");
    TextBuffer text(into, bytes);
    text.append(prefix_);
    text.append_decimal(major_);
    text.append(".");
    text.append_decimal(minor_);
    text.append(tail);
}

bool VersionText::at_most_implemented(uint32_t major, uint32_t minor) const
{
    AV_SCOPE("VersionText::at_most_implemented");
    return major < most_major_ || (major == most_major_ && minor <= most_minor_);
}

bool VersionText::invariants_hold() const
{
    AV_SCOPE("VersionText::invariants_hold");
    return at_most_implemented(major_, minor_);
}

}  // namespace av::icd

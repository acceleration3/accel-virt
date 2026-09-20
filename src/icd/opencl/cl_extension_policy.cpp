#include "icd/opencl/cl_extension_policy.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

size_t length_within(const char *text, size_t bytes)
{
    AV_SCOPE("length_within");
    size_t length = 0u;
    while (length < bytes && text[length] != '\0') ++length;
    return length;
}

bool same_word(const char *text, const char *name, size_t length)
{
    AV_SCOPE("same_word");
    for (size_t index = 0u; index < length; ++index) {
        if (name[index] == '\0' || text[index] != name[index]) return false;
    }
    return name[length] == '\0';
}

}  // namespace

/* The extensions this ICD reports, each with the test that proves it through
 * the ICD (tools/check_extension_policy.py).
 *
 * Empty, and deliberately so. cl_khr_icd is the loader's own contract rather
 * than something an application enables, and this ICD answers its two queries
 * without claiming the extension; cl_khr_gl_sharing waits for feature 053,
 * which is what implements sharing with the OpenGL ICD. Nothing else is
 * carried, tested or reported. */
const ClExtensionPolicy::Row *reported_extensions = nullptr;

ClExtensionPolicy::ClExtensionPolicy(const Row *rows, uint32_t count)
    : rows_(rows), count_(rows != nullptr ? count : 0u)
{
    AV_SCOPE("ClExtensionPolicy::ClExtensionPolicy");
    if (count_ > max_rows) count_ = max_rows;
}

bool ClExtensionPolicy::offered_by(const char *host, size_t host_bytes, const char *name) const
{
    AV_SCOPE("ClExtensionPolicy::offered_by");
    if (host == nullptr || name == nullptr) return false;
    const size_t bytes = length_within(host, host_bytes);
    size_t at = 0u;
    while (at < bytes) {
        while (at < bytes && host[at] == ' ') ++at;
        size_t end = at;
        while (end < bytes && host[end] != ' ') ++end;
        if (end > at && same_word(&host[at], name, end - at)) return true;
        at = end;
    }
    return false;
}

bool ClExtensionPolicy::reportable(const char *name) const
{
    AV_SCOPE("ClExtensionPolicy::reportable");
    if (name == nullptr) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        const char *const row = rows_[index].name_;
        size_t length = 0u;
        while (row[length] != '\0' && length < name_bytes) ++length;
        if (same_word(row, name, length)) return true;
    }
    return false;
}

size_t ClExtensionPolicy::report(const char *host, size_t host_bytes, char *into,
                                 size_t into_bytes) const
{
    AV_SCOPE("ClExtensionPolicy::report");
    /* Measured first, then written: a caller asks the size and asks again
     * with room, and an answer that does not fit writes nothing rather than
     * as much as happened to fit. */
    size_t needed = 1u;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (!offered_by(host, host_bytes, rows_[index].name_)) continue;
        size_t length = 0u;
        while (rows_[index].name_[length] != '\0' && length < name_bytes) ++length;
        needed += length + (needed > 1u ? 1u : 0u);
    }
    if (into == nullptr || into_bytes < needed) return needed;

    size_t at = 0u;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (!offered_by(host, host_bytes, rows_[index].name_)) continue;
        if (at != 0u) into[at++] = ' ';
        for (size_t letter = 0u; rows_[index].name_[letter] != '\0' && letter < name_bytes; ++letter) {
            into[at++] = rows_[index].name_[letter];
        }
    }
    into[at] = '\0';
    return needed;
}

uint32_t ClExtensionPolicy::rows() const
{
    AV_SCOPE("ClExtensionPolicy::rows");
    return count_;
}

bool ClExtensionPolicy::invariants_hold() const
{
    AV_SCOPE("ClExtensionPolicy::invariants_hold");
    if (count_ > max_rows) return false;
    if (count_ != 0u && rows_ == nullptr) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (rows_[index].name_ == nullptr || rows_[index].test_ == nullptr) return false;
        size_t length = 0u;
        while (length < name_bytes && rows_[index].name_[length] != '\0') ++length;
        if (length == 0u || length == name_bytes) return false;
        /* No name twice: reporting one twice would be a list no application
         * can parse into a set. */
        for (uint32_t other = index + 1u; other < count_; ++other) {
            if (same_word(rows_[index].name_, rows_[other].name_, length)) return false;
        }
    }
    return true;
}

}  // namespace av::clicd

#include "icd/opencl/cl_include_inliner.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/opencl/header_source.hpp"

namespace av::clicd {

namespace {

constexpr size_t name_room = 260u;

size_t length_of(const char *text)
{
    AV_SCOPE("length_of");
    size_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

}

ClIncludeInliner::ClIncludeInliner(BlockSource &memory, HeaderSource &headers)
    : memory_(memory), headers_(headers), host_(nullptr), directory_text_(nullptr), directory_at_{}, directories_(0u),
      out_(memory, Pool::paged), once_{}, once_count_(0u)
{
    AV_SCOPE("ClIncludeInliner::ClIncludeInliner");
}

ClIncludeInliner::~ClIncludeInliner()
{
    AV_SCOPE("ClIncludeInliner::~ClIncludeInliner");
    if (host_ != nullptr) memory_.release(host_);
    if (directory_text_ != nullptr) memory_.release(directory_text_);
    for (uint32_t index = 0u; index < once_count_; ++index) memory_.release(once_[index]);
}

Status ClIncludeInliner::read_options(const char *options)
{
    AV_SCOPE("ClIncludeInliner::read_options");
    const char *const asked = options != nullptr ? options : "";
    const size_t      length = length_of(asked);
    host_ = static_cast<char *>(memory_.allocate(length + 1u, Pool::paged));
    directory_text_ = static_cast<char *>(memory_.allocate(length + 1u, Pool::paged));
    if (host_ == nullptr || directory_text_ == nullptr) return Status::no_memory;
    size_t host_at = 0u;
    size_t dir_at = 0u;
    size_t at = 0u;
    bool   wants_directory = false;
    while (at < length) {
        while (at < length && asked[at] == ' ') ++at;
        if (at >= length) break;
        const size_t start = at;
        bool         quoted = false;
        while (at < length && (quoted || asked[at] != ' ')) {
            if (asked[at] == '"') quoted = !quoted;
            ++at;
        }
        const size_t end = at;
        size_t       from = start;
        bool         directory = wants_directory;
        if (!directory && end - start >= 2u && asked[start] == '-' && asked[start + 1u] == 'I') {
            directory = true;
            from = start + 2u;
            if (from == end) {
                wants_directory = true;
                continue;
            }
        }
        wants_directory = false;
        if (directory) {
            if (directories_ >= most_directories) return Status::invalid_parameter;
            directory_at_[directories_++] = static_cast<uint32_t>(dir_at);
            for (size_t index = from; index < end; ++index)
                if (asked[index] != '"') directory_text_[dir_at++] = asked[index];
            directory_text_[dir_at++] = '\0';
            continue;
        }
        if (host_at != 0u) host_[host_at++] = ' ';
        for (size_t index = start; index < end; ++index) host_[host_at++] = asked[index];
    }
    host_[host_at] = '\0';
    return Status::ok;
}

uint32_t ClIncludeInliner::directories() const
{
    AV_SCOPE("ClIncludeInliner::directories");
    return directories_;
}

const char *ClIncludeInliner::host_options() const
{
    AV_SCOPE("ClIncludeInliner::host_options");
    return host_ != nullptr ? host_ : "";
}

Status ClIncludeInliner::expand(const char *source, size_t bytes)
{
    AV_SCOPE("ClIncludeInliner::expand");
    out_.clear();
    const Status done = expand_into(source, bytes, 0u);
    if (done != Status::ok) return done;
    return out_.append('\0');
}

const char *ClIncludeInliner::text() const
{
    AV_SCOPE("ClIncludeInliner::text");
    return out_.size() != 0u ? out_.begin() : "";
}

size_t ClIncludeInliner::text_bytes() const
{
    AV_SCOPE("ClIncludeInliner::text_bytes");
    return out_.size() != 0u ? out_.size() - 1u : 0u;
}

Status ClIncludeInliner::add_line(const char *line, size_t bytes)
{
    AV_SCOPE("ClIncludeInliner::add_line");
    if (out_.reserve(out_.size() + static_cast<uint32_t>(bytes) + 1u) != Status::ok) return Status::no_memory;
    for (size_t index = 0u; index < bytes; ++index)
        if (out_.append(line[index]) != Status::ok) return Status::no_memory;
    return Status::ok;
}

bool ClIncludeInliner::include_name(const char *line, size_t bytes, bool &quoted, char *name, size_t room) const
{
    AV_SCOPE("ClIncludeInliner::include_name");
    static constexpr char word[] = "include";
    size_t                at = 0u;
    while (at < bytes && (line[at] == ' ' || line[at] == '\t')) ++at;
    if (at >= bytes || line[at] != '#') return false;
    ++at;
    while (at < bytes && (line[at] == ' ' || line[at] == '\t')) ++at;
    for (size_t index = 0u; word[index] != '\0'; ++index, ++at)
        if (at >= bytes || line[at] != word[index]) return false;
    while (at < bytes && (line[at] == ' ' || line[at] == '\t')) ++at;
    if (at >= bytes || (line[at] != '"' && line[at] != '<')) return false;
    quoted = line[at] == '"';
    const char close = quoted ? '"' : '>';
    ++at;
    size_t used = 0u;
    while (at < bytes && line[at] != close) {
        if (used + 1u >= room) return false;
        name[used++] = line[at++];
    }
    if (at >= bytes || used == 0u) return false;
    name[used] = '\0';
    return true;
}

bool ClIncludeInliner::once_seen(const char *directory, const char *name, const char *text, size_t bytes)
{
    AV_SCOPE("ClIncludeInliner::once_seen");
    static constexpr char pragma[] = "#pragma once";
    bool                  marked = false;
    for (size_t at = 0u; at + sizeof pragma - 1u <= bytes && !marked; ++at) {
        size_t index = 0u;
        while (index < sizeof pragma - 1u && text[at + index] == pragma[index]) ++index;
        marked = index == sizeof pragma - 1u;
    }
    if (!marked) return false;
    const size_t dir_length = length_of(directory);
    const size_t key_length = dir_length + 1u + length_of(name);
    for (uint32_t index = 0u; index < once_count_; ++index) {
        size_t letter = 0u;
        while (letter < key_length && once_[index][letter] ==
                                          (letter < dir_length ? directory[letter]
                                                               : letter == dir_length ? '|' : name[letter - dir_length - 1u]))
            ++letter;
        if (letter == key_length && once_[index][letter] == '\0') return true;
    }
    if (once_count_ < most_once) {
        char *const key = static_cast<char *>(memory_.allocate(key_length + 1u, Pool::paged));
        if (key != nullptr) {
            for (size_t letter = 0u; letter < key_length; ++letter)
                key[letter] = letter < dir_length ? directory[letter]
                                                   : letter == dir_length ? '|' : name[letter - dir_length - 1u];
            key[key_length] = '\0';
            once_[once_count_++] = key;
        }
    }
    return false;
}

Status ClIncludeInliner::expand_into(const char *source, size_t bytes, uint32_t depth)
{
    AV_SCOPE("ClIncludeInliner::expand_into");
    size_t at = 0u;
    while (at < bytes) {
        size_t end = at;
        while (end < bytes && source[end] != '\n') ++end;
        const size_t line_bytes = end < bytes ? end - at + 1u : end - at;
        char         name[name_room] = {};
        bool         quoted = false;
        bool         inlined = false;
        if (depth < most_depth && directories_ != 0u && include_name(source + at, end - at, quoted, name, sizeof name)) {
            for (uint32_t index = 0u; index < directories_ && !inlined; ++index) {
                const char *const directory = directory_text_ + directory_at_[index];
                char             *text = nullptr;
                size_t            text_bytes = 0u;
                if (!headers_.fetch(memory_, directory, name, text, text_bytes)) continue;
                inlined = true;
                Status done = Status::ok;
                if (!once_seen(directory, name, text, text_bytes)) {
                    done = expand_into(text, text_bytes, depth + 1u);
                    if (done == Status::ok && out_.size() != 0u && out_.at(out_.size() - 1u) != '\n') done = add_line("\n", 1u);
                }
                memory_.release(text);
                if (done != Status::ok) return done;
            }
        }
        if (!inlined) {
            const Status added = add_line(source + at, line_bytes);
            if (added != Status::ok) return added;
        }
        at += line_bytes;
    }
    return Status::ok;
}

}

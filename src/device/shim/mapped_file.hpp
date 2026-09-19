/* A file's bytes, mapped read-only, which is how the recording tools read a
 * recording: a recording is bounded by the ceiling the device was given, and a
 * mapping costs nothing until a page is touched, so the reader walks the file
 * without the tool ever owning a copy of it. */
#ifndef AV_MAPPED_FILE_HPP
#define AV_MAPPED_FILE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class MappedFile {
public:
    constexpr MappedFile() = default;
    ~MappedFile();
    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;

    /* not_found for a file that does not open, no_memory for one that does not
     * map, invalid_state once open. An empty file opens and maps nothing. */
    [[nodiscard]] Status open(const char *path);

    const uint8_t *bytes() const;
    size_t         length() const;

private:
    const uint8_t *bytes_ = nullptr;
    size_t         length_ = 0u;
    bool           open_ = false;
};

}  // namespace av

#endif  // AV_MAPPED_FILE_HPP

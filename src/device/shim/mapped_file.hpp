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

    [[nodiscard]] Status open(const char *path);

    const uint8_t *bytes() const;
    size_t         length() const;

private:
    const uint8_t *bytes_ = nullptr;
    size_t         length_ = 0u;
    bool           open_ = false;
};

}

#endif

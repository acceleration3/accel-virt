#ifndef AV_STAGING_BYTES_HPP
#define AV_STAGING_BYTES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class StagingBytes {
public:
    StagingBytes();
    ~StagingBytes();
    StagingBytes(const StagingBytes &) = delete;
    StagingBytes &operator=(const StagingBytes &) = delete;

    [[nodiscard]] bool reserve(size_t bytes);
    uint8_t           *bytes() const;
    size_t             held() const;

private:
    uint8_t *bytes_;
    size_t   held_;
};

}

#endif

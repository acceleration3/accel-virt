/* Private memory a transfer passes through on the CPU, kept for the next one
 * and grown to the largest asked: a buffer-rectangle read the host will not
 * make into the arena (ClBouncedRect), and a GL object's bytes on their way
 * into or out of the cl_mem that stands for it (CopiedGlSharing). Whole pages
 * of anonymous memory, given back when this goes. */
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

    /* At least `bytes`, what was held before kept or let go: false, holding
     * what it held, where `bytes` rounded to pages does not fit a size_t or
     * the memory cannot be had. */
    [[nodiscard]] bool reserve(size_t bytes);
    uint8_t           *bytes() const;
    size_t             held() const;

private:
    uint8_t *bytes_;
    size_t   held_;
};

}  // namespace av

#endif  // AV_STAGING_BYTES_HPP

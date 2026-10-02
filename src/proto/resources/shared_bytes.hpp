/* The bytes a memory object is shared as: whole pages, because the record its
 * publisher wrote names whole pages and an open is judged against it, and the
 * wire body refuses anything else (SharedObjectRequest). An application asks
 * for the size of its own buffer or image, which is rarely a page multiple, so
 * every API's import rounds the same way. */
#ifndef AV_SHARED_BYTES_HPP
#define AV_SHARED_BYTES_HPP

#include <stdint.h>

namespace av {

class SharedBytes {
public:
    explicit SharedBytes(uint64_t asked);

    /* What was asked, rounded up to whole pages; 0 when nothing was asked or
     * rounding would wrap, which names no object. */
    uint64_t pages() const;

private:
    uint64_t asked_;
};

}  // namespace av

#endif  // AV_SHARED_BYTES_HPP

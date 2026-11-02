#ifndef AV_SHARED_BYTES_HPP
#define AV_SHARED_BYTES_HPP

#include <stdint.h>

namespace av {

class SharedBytes {
public:
    explicit SharedBytes(uint64_t asked);

    uint64_t pages() const;

private:
    uint64_t asked_;
};

}

#endif

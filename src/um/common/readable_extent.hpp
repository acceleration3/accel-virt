#ifndef AV_UM_READABLE_EXTENT_HPP
#define AV_UM_READABLE_EXTENT_HPP

#include <stddef.h>

namespace av {

class ReadableExtent {
public:
    explicit ReadableExtent(size_t most);

    size_t from(const void *start) const;

private:
    size_t most_;
};

}

#endif

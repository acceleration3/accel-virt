#ifndef AV_GL_CONTEXT_ATTRIBUTES_HPP
#define AV_GL_CONTEXT_ATTRIBUTES_HPP

#include "device/abi/av_executor.h"

#include <stdint.h>

namespace av {

class GlContextAttributes {
public:
    explicit GlContextAttributes(const AvContextOptions *options);

    bool           valid() const;
    const int32_t *list() const;

private:
    static constexpr uint32_t capacity = 15u;

    bool acceptable() const;
    void add(int32_t name, int32_t value);

    AvContextOptions options_;
    int32_t  list_[capacity];
    uint32_t used_;
    bool     valid_;
};

}

#endif

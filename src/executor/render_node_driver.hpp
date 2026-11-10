#ifndef AV_RENDER_NODE_DRIVER_HPP
#define AV_RENDER_NODE_DRIVER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class RenderNodeDriver {
public:
    explicit RenderNodeDriver(const char *class_directory);

    bool named(int32_t render_minor, const char *driver) const;

private:
    static constexpr size_t directory_bytes = 192u;

    char class_directory_[directory_bytes];
};

}

#endif

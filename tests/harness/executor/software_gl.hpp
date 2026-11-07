#ifndef AV_SOFTWARE_GL_HPP
#define AV_SOFTWARE_GL_HPP

#include "executor/shim/gl_display.hpp"

#include <cstdint>

namespace av {

class SoftwareGl {
public:
    static constexpr std::uint32_t uuid_bytes = 16u;

    SoftwareGl();
    SoftwareGl(const SoftwareGl &) = delete;
    SoftwareGl &operator=(const SoftwareGl &) = delete;

    bool               usable() const;
    const GlDisplay   *display() const;
    const std::uint8_t *uuid() const;

private:
    void probe();

    GlDisplay    display_;
    std::uint8_t uuid_[uuid_bytes];
    bool         usable_;
};

}

#endif

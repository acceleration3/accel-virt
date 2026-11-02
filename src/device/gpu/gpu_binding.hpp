#ifndef AV_GPU_BINDING_HPP
#define AV_GPU_BINDING_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class TextBuffer;

class GpuBinding {
public:
    explicit GpuBinding(Host &host);
    GpuBinding(const GpuBinding &) = delete;
    GpuBinding &operator=(const GpuBinding &) = delete;

    [[nodiscard]] Status bind(const char *selection, TextBuffer &message);
    bool                 bound() const;
    uint32_t             index() const;

private:
    [[nodiscard]] Status bind_uuid(const char *selection, TextBuffer &message);
    [[nodiscard]] Status take(uint32_t index, TextBuffer &message);
    [[nodiscard]] Status refuse(Status status, TextBuffer &message);
    bool                 is_auto(const char *selection) const;

    Host    &host_;
    uint32_t index_;
    bool     bound_;
};

}

#endif

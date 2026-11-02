#ifndef AV_UM_GL_DISCOVERY_PROBE_HPP
#define AV_UM_GL_DISCOVERY_PROBE_HPP

#include "core/support/text_buffer.hpp"
#include "um/probe/probe_window.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GlDiscoveryProbe {
public:
    GlDiscoveryProbe();
    GlDiscoveryProbe(const GlDiscoveryProbe &) = delete;
    GlDiscoveryProbe &operator=(const GlDiscoveryProbe &) = delete;

    uint32_t run();

private:
    static constexpr size_t   report_bytes = 16384u;
    static constexpr uint32_t max_modules = 512u;

    void     line(const char *key, const char *text, uint64_t value, uint64_t error);
    void     modules(void **list, uint32_t &count) const;
    void     report_new_modules(void *const *before, uint32_t before_count);
    bool     make_window();
    void     describe();
    void     set_format();
    void     create_context();
    void     open_adapter();
    void     write() const;

    char        bytes_[report_bytes];
    TextBuffer  report_;
    ProbeWindow window_;
    int32_t     format_;
};

}

#endif

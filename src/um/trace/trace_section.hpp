#ifndef AV_UM_TRACE_SECTION_HPP
#define AV_UM_TRACE_SECTION_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ObjectSecurity;

class TraceSection {
public:
    static constexpr uint32_t slots = 128u;

    TraceSection();
    TraceSection(const TraceSection &) = delete;
    TraceSection &operator=(const TraceSection &) = delete;

    [[nodiscard]] Status create(const char *module, uint32_t pid, ObjectSecurity &security);
    uint8_t             *region();
    size_t               bytes() const;

private:
    void    *mapping_;
    uint8_t *view_;
};

}

#endif

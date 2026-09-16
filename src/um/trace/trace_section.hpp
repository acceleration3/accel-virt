/* The named section a process's trace table lives in (docs/trace.md):
 * `Global\rvk-trace-<module>-<pid>`, or `Local\` where the process may not
 * create a global object, with room for 128 slots. The view stays mapped for
 * the process's life - a thread may be inside a scope until the process ends.
 * Translation only. */
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

    /* invalid_state when made already; no_memory when neither name could be
     * created and mapped. */
    [[nodiscard]] Status create(const char *module, uint32_t pid, ObjectSecurity &security);
    uint8_t             *region();
    size_t               bytes() const;

private:
    void    *mapping_;
    uint8_t *view_;
};

}  // namespace av

#endif  // AV_UM_TRACE_SECTION_HPP

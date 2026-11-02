#ifndef AV_GLICD_WINDOW_TABLE_HPP
#define AV_GLICD_WINDOW_TABLE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::icd {
class WindowSurface;
}

namespace av::glicd {

struct WindowTableProbe;

class WindowTable {
public:
    explicit WindowTable(BlockSource &memory);
    WindowTable(const WindowTable &) = delete;
    WindowTable &operator=(const WindowTable &) = delete;

    [[nodiscard]] Status set_format(void *window, int32_t format);
    int32_t              format_of(void *window) const;
    uint32_t             held() const;
    void                 drop_gone(icd::WindowSurface &surface);
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    uint64_t key_of(void *window) const;

    KeyedSlots<uint64_t, int32_t> formats_;

    friend struct WindowTableProbe;
};

}

#endif

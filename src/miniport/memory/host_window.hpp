#ifndef AV_HOST_WINDOW_HPP
#define AV_HOST_WINDOW_HPP

#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct HostWindowProbe;

class HostWindow {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit HostWindow(Platform &platform);
    ~HostWindow();
    HostWindow(const HostWindow &) = delete;
    HostWindow &operator=(const HostWindow &) = delete;

    [[nodiscard]] Status initialize(uint64_t base, uint64_t bytes);
    void                 withdraw();
    bool                 offered() const;
    [[nodiscard]] Status take(uint32_t pages, uint32_t &first);
    void                 give_back(uint32_t first, uint32_t pages);
    uint64_t             physical(uint32_t first) const;
    uint32_t             pages_in_use() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t bits_per_word = 64u;

    void mark(uint32_t first, uint32_t pages);

    Platform &platform_;
    Mutex     lock_;
    uint64_t *map_;
    uint64_t  base_;
    uint32_t  pages_;
    uint32_t  used_;
    bool      offered_;

    friend struct HostWindowProbe;
};

}

#endif

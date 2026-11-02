#ifndef AV_PRESENT_PAGE_WINDOW_HPP
#define AV_PRESENT_PAGE_WINDOW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class PresentProgress;

class PresentPageWindow {
public:
    explicit PresentPageWindow(Host &host);
    ~PresentPageWindow();
    PresentPageWindow(const PresentPageWindow &) = delete;
    PresentPageWindow &operator=(const PresentPageWindow &) = delete;

    [[nodiscard]] Status map(uint64_t address);
    void                 release();
    bool                 mapped() const;
    void                 publish(const PresentProgress &progress);

private:
    uint64_t *word(uint64_t offset) const;

    Host    &host_;
    uint8_t *page_;
};

}

#endif

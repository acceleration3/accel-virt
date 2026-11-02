#ifndef AV_PRESENT_PAGE_VIEW_HPP
#define AV_PRESENT_PAGE_VIEW_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PresentProgress;

class PresentPageView {
public:
    explicit PresentPageView(Platform &platform);
    ~PresentPageView();
    PresentPageView(const PresentPageView &) = delete;
    PresentPageView &operator=(const PresentPageView &) = delete;

    [[nodiscard]] Status initialize();
    void                 clear();
    uint64_t             address() const;
    [[nodiscard]] Status read(PresentProgress &progress) const;

private:
    uint32_t *word(uint64_t offset) const;

    Platform &platform_;
    uint8_t  *page_;
};

}

#endif

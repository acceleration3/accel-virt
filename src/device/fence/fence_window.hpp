#ifndef AV_FENCE_WINDOW_HPP
#define AV_FENCE_WINDOW_HPP

#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"

#include <stdint.h>

namespace av {

class FencePage;

class FenceWindow {
public:
    FenceWindow();
    explicit FenceWindow(const BlobWindow &blob);

    [[nodiscard]] Status publish(const FencePage &page) const;
    [[nodiscard]] Status publish_beat(uint64_t beat) const;
    [[nodiscard]] Status read(FencePage &page) const;

    bool invariants_hold() const;

private:
    BlobWindow blob_;
};

}

#endif

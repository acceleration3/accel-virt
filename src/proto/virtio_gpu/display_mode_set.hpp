#ifndef AV_DISPLAY_MODE_SET_HPP
#define AV_DISPLAY_MODE_SET_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireWriter;
struct DisplayModeSetProbe;

class DisplayModeSet {
public:
    static constexpr uint32_t request_type = 0x02F3u;
    static constexpr uint32_t response_type = 0x11F3u;
    static constexpr uint32_t max_modes = 32u;
    static constexpr uint32_t max_dimension = 16384u;
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   fixed_size = header_size + 8u;
    static constexpr size_t   mode_size = 8u;
    static constexpr size_t   refresh_size = 4u;
    static constexpr size_t   max_encoded_size = fixed_size + max_modes * mode_size + refresh_size;
    static constexpr uint32_t default_refresh_millihz = 60000u;
    static constexpr uint32_t least_refresh_millihz = 1000u;
    static constexpr uint32_t most_refresh_millihz = 1000000u;

    DisplayModeSet();

    [[nodiscard]] Status add(uint32_t width, uint32_t height);
    [[nodiscard]] Status prefer(uint32_t index);
    [[nodiscard]] Status set_refresh(uint32_t millihz);
    void                 clear();

    uint32_t count() const;
    uint32_t width(uint32_t index) const;
    uint32_t height(uint32_t index) const;
    uint32_t preferred() const;
    uint32_t refresh_millihz() const;
    bool     same_as(const DisplayModeSet &other) const;

    [[nodiscard]] Status encode(WireWriter &writer) const;
    size_t               encoded_size() const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

    bool invariants_hold() const;

private:
    friend struct DisplayModeSetProbe;

    uint32_t widths_[max_modes];
    uint32_t heights_[max_modes];
    uint32_t count_;
    uint32_t preferred_;
    uint32_t refresh_millihz_;
};

}

#endif

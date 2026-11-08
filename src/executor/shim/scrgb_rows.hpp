#ifndef AV_SCRGB_ROWS_HPP
#define AV_SCRGB_ROWS_HPP

#include <stdint.h>

namespace av {

class ScrgbRows {
public:
    static constexpr uint32_t table_steps = 4096u;

    ScrgbRows();

    [[nodiscard]] bool use(uint32_t source_format, uint32_t target_format);
    uint32_t           pixel_bytes() const;
    uint8_t            encode(float linear) const;
    void               convert(const uint8_t *from, uint32_t pixels, uint8_t *to) const;

private:
    float    table_[table_steps + 1u];
    uint32_t component_bytes_;
    uint32_t components_;
    bool     red_first_;
};

}

#endif

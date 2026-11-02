#ifndef AV_HOSTILE_SWEEP_HPP
#define AV_HOSTILE_SWEEP_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class HostileSweep {
public:
    using Parser = bool (*)(const std::uint8_t *bytes, std::size_t length);

    HostileSweep(Parser parser, const std::uint8_t *valid, std::size_t length);

    void run() const;

    std::uint32_t cases_run() const;

    std::uint32_t rejections() const;

private:
    void feed(const std::uint8_t *bytes, std::size_t length) const;

    Parser              parser_;
    const std::uint8_t *valid_;
    std::size_t         length_;
    mutable std::uint32_t cases_run_;
    mutable std::uint32_t rejections_;
};

}

#endif

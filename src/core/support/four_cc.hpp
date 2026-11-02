#ifndef AV_FOUR_CC_HPP
#define AV_FOUR_CC_HPP

#include <stdint.h>

namespace av {

class FourCc {
public:
    constexpr FourCc(char first, char second, char third, char fourth);

    constexpr uint32_t value() const;

private:
    uint32_t value_;
};

constexpr FourCc::FourCc(char first, char second, char third, char fourth)
    : value_(static_cast<uint32_t>(static_cast<unsigned char>(first)) |
             static_cast<uint32_t>(static_cast<unsigned char>(second)) << 8u |
             static_cast<uint32_t>(static_cast<unsigned char>(third)) << 16u |
             static_cast<uint32_t>(static_cast<unsigned char>(fourth)) << 24u)
{
}

constexpr uint32_t FourCc::value() const
{
    return value_;
}

}

#endif

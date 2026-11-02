#ifndef AV_CL_PLATFORM_CHOICE_HPP
#define AV_CL_PLATFORM_CHOICE_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClPlatformChoice {
public:
    static constexpr size_t name_bytes = 64u;

    ClPlatformChoice();
    ClPlatformChoice(const ClPlatformChoice &) = delete;
    ClPlatformChoice &operator=(const ClPlatformChoice &) = delete;

    void consider(void *platform, void *device, const char *name);

    void *platform() const;
    void *device() const;

private:
    bool better(uint32_t rank, const char *name) const;

    void    *platform_;
    void    *device_;
    uint32_t rank_;
    char     name_[name_bytes];
};

}

#endif

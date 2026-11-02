#ifndef AV_PATH_TRANSFORMATION_HPP
#define AV_PATH_TRANSFORMATION_HPP

#include "core/support/nt_status.hpp"

#include <stdint.h>

namespace av {

struct D3dkmdtVidPnPresentPathRecord;

class PathTransformation {
public:
    explicit PathTransformation(const D3dkmdtVidPnPresentPathRecord &path);

    [[nodiscard]] NtStatus accepted() const;
    [[nodiscard]] bool     supportable() const;

private:
    enum : int32_t {
        identity = 1,
        uninitialized = 0,
        unpinned = 254,
        not_specified = 255,
        gamma_uninitialized = 0,
        gamma_default = 1,
    };

    [[nodiscard]] bool default_gamma() const;

    int32_t scaling_;
    int32_t rotation_;
    int32_t gamma_;
};

}

#endif

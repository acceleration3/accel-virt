#ifndef AV_ADAPTER_NAME_HPP
#define AV_ADAPTER_NAME_HPP

#include "proto/context/gpu_identity.hpp"

#include <stddef.h>

namespace av {

class AdapterName {
public:
    explicit AdapterName(const char *vulkan_name);

    const char *text() const;

private:
    static constexpr size_t capacity = GpuIdentity::name_bytes - 1u;

    size_t without_suffix(const char *name, size_t length) const;
    size_t fitted(const char *name, size_t length) const;

    char text_[GpuIdentity::name_bytes];
};

}

#endif

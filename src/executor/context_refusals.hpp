#ifndef AV_CONTEXT_REFUSALS_HPP
#define AV_CONTEXT_REFUSALS_HPP

#include <stdint.h>

namespace av {

class DriverLog;

class ContextRefusals {
public:
    enum class Reason : uint32_t {
        domain,
        options,
        share_api,
        sub_api_mask,
        no_backend,
        share_gone,
        bind,
        d3d11_device,
        count
    };

    explicit ContextRefusals(DriverLog &log);
    ContextRefusals(const ContextRefusals &) = delete;
    ContextRefusals &operator=(const ContextRefusals &) = delete;

    void     refuse(Reason reason, const char *api, uint64_t detail);
    uint64_t refused(Reason reason) const;

private:
    static constexpr uint64_t spoken_each = 4u;
    static constexpr uint64_t then_every = 128u;

    const char *describe(Reason reason) const;

    DriverLog &log_;
    uint64_t   seen_[static_cast<uint32_t>(Reason::count)];
};

}

#endif

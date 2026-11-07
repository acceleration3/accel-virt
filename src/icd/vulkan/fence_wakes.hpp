#ifndef AV_ICD_FENCE_WAKES_HPP
#define AV_ICD_FENCE_WAKES_HPP

#include "icd/common/service_thread.hpp"

#include <stdint.h>

namespace av::icd {

class Connection;

class FenceWakes final : public ServiceLoop {
public:
    explicit FenceWakes(ServiceThread &thread);
    ~FenceWakes();
    FenceWakes(const FenceWakes &) = delete;
    FenceWakes &operator=(const FenceWakes &) = delete;

    bool turn() override;

    [[nodiscard]] bool register_with(Connection &connection);
    uint64_t           woken() const;
    void               await(uint64_t seen, uint64_t timeout);

private:
    ServiceThread &thread_;
};

}

#endif

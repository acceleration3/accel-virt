#ifndef AV_ICD_TRANSPORT_TIMELINE_HPP
#define AV_ICD_TRANSPORT_TIMELINE_HPP

#include <stdint.h>

namespace av::icd {

class HostChannel;

class TransportTimeline {
public:
    explicit TransportTimeline(HostChannel &channel);
    TransportTimeline(const TransportTimeline &) = delete;
    TransportTimeline &operator=(const TransportTimeline &) = delete;

    uint64_t           recording() const;
    uint64_t           completed() const;
    [[nodiscard]] bool wait(uint64_t fence);
    [[nodiscard]] bool publish(uint64_t fence, uint64_t &target);
    [[nodiscard]] bool await(uint64_t target);
    void               forget();

private:
    HostChannel &channel_;
    uint64_t     waited_;
};

}

#endif

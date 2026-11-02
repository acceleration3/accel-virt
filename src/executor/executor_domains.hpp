#ifndef AV_EXECUTOR_DOMAINS_HPP
#define AV_EXECUTOR_DOMAINS_HPP

#include <stdint.h>

namespace av {

class StateWriter;

class ExecutorDomains {
public:
    static constexpr uint32_t max_domains = 16u;

    ExecutorDomains();
    ExecutorDomains(const ExecutorDomains &) = delete;
    ExecutorDomains &operator=(const ExecutorDomains &) = delete;

    uint32_t open(uint32_t gpu_index);
    bool     close(uint32_t domain, uint32_t &gpu_index);
    bool     gpu_of(uint32_t domain, uint32_t &gpu_index) const;
    uint32_t live() const;
    void     report(StateWriter &writer) const;

private:
    uint32_t entry_of(uint32_t domain) const;

    uint32_t ids_[max_domains];
    uint32_t gpus_[max_domains];
    uint32_t next_;
};

}

#endif

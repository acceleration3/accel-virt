#ifndef AV_ROOT_UNDER_TEST_HPP
#define AV_ROOT_UNDER_TEST_HPP

#include "device/abi/av_executor.h"
#include "executor/executor_log.hpp"
#include "harness/icd/recording_log.hpp"

#include <cstdint>

namespace av {

class ContextSession;
class ExecutorRoot;

class RootUnderTest {
public:
    RootUnderTest();
    ~RootUnderTest();
    RootUnderTest(const RootUnderTest &) = delete;
    RootUnderTest &operator=(const RootUnderTest &) = delete;

    ExecutorRoot &root() const;
    ExecutorRoot *operator->() const;
    ExecutorRoot &operator*() const;

    std::uint32_t   domain_of(const std::uint8_t *gpu_uuid);
    ContextSession *open(std::uint32_t sub_api, const std::uint8_t *gpu_uuid,
                         const AvContextOptions *options = nullptr, ContextSession *share = nullptr);

    const RecordingLog &lines() const;
    bool invariants_hold() const;

private:
    static constexpr std::uint32_t max_domains = 8u;

    RecordingLog  sink_;
    ExecutorLog   log_;
    bool          held_;
    std::uint8_t  gpus_[max_domains][16];
    std::uint32_t domains_[max_domains];
};

}

#endif

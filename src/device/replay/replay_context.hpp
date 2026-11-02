#ifndef AV_REPLAY_CONTEXT_HPP
#define AV_REPLAY_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLibrary;
class RecordedCommand;

struct ReplayContextProbe;

class ReplayContext {
public:
    static constexpr size_t answer_bytes = 16u * 1024u;

    ReplayContext();
    ~ReplayContext();
    ReplayContext(const ReplayContext &) = delete;
    ReplayContext &operator=(const ReplayContext &) = delete;

    [[nodiscard]] Status create(const ExecutorLibrary &library, uint32_t context, SubApi api);
    void                 destroy(const ExecutorLibrary &library);
    bool                 created() const;
    uint32_t             context() const;
    uint16_t             command_api() const;

    [[nodiscard]] Status execute(const ExecutorLibrary &library, const RecordedCommand &command);
    bool                 answered() const;
    size_t               answer_length() const;
    bool                 answers_with(const uint8_t *recorded, uint32_t length) const;
    void                 forget_answer();
    void                 answer_in_parts();

    void                 give_up();
    bool                 gave_up() const;

    bool invariants_hold() const;

private:
    AvExecutor *executor_;
    uint32_t     context_;
    SubApi       api_;
    bool         answered_;
    bool         in_parts_;
    size_t       answer_length_;
    bool         gave_up_;
    uint8_t      answer_[answer_bytes];

    friend struct ReplayContextProbe;
};

}

#endif

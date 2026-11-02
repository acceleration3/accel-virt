#ifndef AV_DEBUG_REPORT_HPP
#define AV_DEBUG_REPORT_HPP

#include "core/platform/platform.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutionNode;

class DebugReport {
public:
    static constexpr uint32_t magic = 0x00445641u;
    static constexpr uint16_t format = 2u;
    static constexpr size_t   report_bytes = 40u;

    DebugReport(Platform &platform, bool started, bool powered, uint64_t features, ExecutionNode &node);

    size_t write(uint8_t *buffer, size_t capacity) const;

private:
    Platform &platform_;
    bool      started_;
    bool      powered_;
    uint64_t       features_;
    ExecutionNode &node_;
};

}

#endif

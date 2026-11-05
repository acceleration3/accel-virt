#ifndef AV_EXECUTOR_LOG_RIG_HPP
#define AV_EXECUTOR_LOG_RIG_HPP

#include "executor/executor_log.hpp"
#include "executor/import_table.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/icd/recording_log.hpp"

namespace av {

struct ExecutorLogRig {
    ExecutorLogRig() : sink(), log(sink), heap(), imports(heap) {}
    ExecutorLogRig(const ExecutorLogRig &) = delete;
    ExecutorLogRig &operator=(const ExecutorLogRig &) = delete;

    RecordingLog sink;
    ExecutorLog  log;
    HostHeap     heap;
    ImportTable  imports;
};

}

#endif

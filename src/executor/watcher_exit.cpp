#include "executor/watcher_exit.hpp"

#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/executor_root.hpp"

#include <stdlib.h>

namespace av {

namespace {

void stop_at_exit()
{
    AV_SCOPE("stop_at_exit");
    WatcherExit::process().stop_all();
}

}

WatcherExit &WatcherExit::process()
{
    AV_SCOPE("WatcherExit::process");
    static ObjectStorage<WatcherExit> kept;
    static const Status made = kept.construct();
    static_cast<void>(made);
    return kept.object();
}

WatcherExit::WatcherExit() : gate_(PTHREAD_MUTEX_INITIALIZER), armed_{}, hooked_(false)
{
    AV_SCOPE("WatcherExit::WatcherExit");
}

WatcherExit::~WatcherExit()
{
    AV_SCOPE("WatcherExit::~WatcherExit");
    pthread_mutex_destroy(&gate_);
}

void WatcherExit::arm(ExecutorRoot &root)
{
    AV_SCOPE("WatcherExit::arm");
    pthread_mutex_lock(&gate_);
    if (!hooked_) {
        hooked_ = atexit(&stop_at_exit) == 0;
        static_cast<void>(at_quick_exit(&stop_at_exit));
    }
    bool placed = false;
    for (uint32_t index = 0u; index < most && !placed; index++) {
        if (armed_[index] != nullptr) continue;
        armed_[index] = &root;
        placed = true;
    }
    pthread_mutex_unlock(&gate_);
}

void WatcherExit::disarm(ExecutorRoot &root)
{
    AV_SCOPE("WatcherExit::disarm");
    pthread_mutex_lock(&gate_);
    for (uint32_t index = 0u; index < most; index++)
        if (armed_[index] == &root) armed_[index] = nullptr;
    pthread_mutex_unlock(&gate_);
}

void WatcherExit::stop_all()
{
    AV_SCOPE("WatcherExit::stop_all");
    pthread_mutex_lock(&gate_);
    for (uint32_t index = 0u; index < most; index++) {
        if (armed_[index] == nullptr) continue;
        armed_[index]->halt_watcher();
        armed_[index] = nullptr;
    }
    pthread_mutex_unlock(&gate_);
}

}

#include "executor/watcher_exit.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/executor_root.hpp"

#include <pthread.h>
#include <stdlib.h>

namespace av {

namespace {

pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
ExecutorRoot   *armed[16] = {};
bool            hooked = false;

}

void WatcherExit::arm(ExecutorRoot &root)
{
    AV_SCOPE("WatcherExit::arm");
    pthread_mutex_lock(&gate);
    if (!hooked) {
        hooked = atexit(&WatcherExit::stop_all) == 0;
        static_cast<void>(at_quick_exit(&WatcherExit::stop_all));
    }
    bool placed = false;
    for (uint32_t index = 0u; index < most && !placed; index++) {
        if (armed[index] != nullptr) continue;
        armed[index] = &root;
        placed = true;
    }
    pthread_mutex_unlock(&gate);
}

void WatcherExit::disarm(ExecutorRoot &root)
{
    AV_SCOPE("WatcherExit::disarm");
    pthread_mutex_lock(&gate);
    for (uint32_t index = 0u; index < most; index++)
        if (armed[index] == &root) armed[index] = nullptr;
    pthread_mutex_unlock(&gate);
}

void WatcherExit::stop_all()
{
    AV_SCOPE("WatcherExit::stop_all");
    pthread_mutex_lock(&gate);
    for (uint32_t index = 0u; index < most; index++) {
        if (armed[index] == nullptr) continue;
        armed[index]->halt_watcher();
        armed[index] = nullptr;
    }
    pthread_mutex_unlock(&gate);
}

}

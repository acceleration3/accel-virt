#ifndef AV_WATCHER_EXIT_HPP
#define AV_WATCHER_EXIT_HPP

#include <stdint.h>

namespace av {

class ExecutorRoot;

class WatcherExit {
public:
    WatcherExit() = delete;

    static void arm(ExecutorRoot &root);
    static void disarm(ExecutorRoot &root);
    static void stop_all();

private:
    static constexpr uint32_t most = 16u;
};

}

#endif

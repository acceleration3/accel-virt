#ifndef AV_WATCHER_EXIT_HPP
#define AV_WATCHER_EXIT_HPP

#include <pthread.h>
#include <stdint.h>

namespace av {

class ExecutorRoot;

class WatcherExit {
public:
    static WatcherExit &process();

    WatcherExit();
    ~WatcherExit();
    WatcherExit(const WatcherExit &) = delete;
    WatcherExit &operator=(const WatcherExit &) = delete;

    void arm(ExecutorRoot &root);
    void disarm(ExecutorRoot &root);
    void stop_all();

private:
    static constexpr uint32_t most = 16u;

    pthread_mutex_t gate_;
    ExecutorRoot   *armed_[most];
    bool            hooked_;
};

}

#endif

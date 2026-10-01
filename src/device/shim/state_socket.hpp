/* The unix socket a host binary answers its state dump on (docs/trace.md):
 * /dev/shm/av-state-<module>-<pid>.sock, where a client sends `state\n` and
 * reads the dump, or one error line, until the binary closes the connection.
 * The accept is marked as a wait, so its thread reads as waiting, not stuck.
 * One per binary; its thread runs for the life of the process. */
#ifndef AV_STATE_SOCKET_HPP
#define AV_STATE_SOCKET_HPP

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

namespace av {

class StateSource;

class StateSocket {
public:
    static constexpr size_t dump_bytes = 256u * 1024u;
    static constexpr size_t path_bytes = 108u;

    StateSocket();
    StateSocket(const StateSocket &) = delete;
    StateSocket &operator=(const StateSocket &) = delete;

    /* Listens and starts the thread; false, with nothing left behind, when the
     * socket or the thread cannot be made. */
    [[nodiscard]] bool start(const char *module, uint32_t process, StateSource &source);
    /* The socket's thread. */
    void               serve();

private:
    static constexpr uint32_t answer_seconds = 2u;
    static constexpr size_t   request_bytes = 16u;

    bool listen_on_socket(const char *module, uint32_t process);
    void answer(int client);
    void send(int client, const char *text, size_t length) const;

    StateSource *source_;
    int          listener_;
    char         path_[path_bytes];
    pthread_t    thread_;
    char         dump_[dump_bytes];
};

}  // namespace av

#endif  // AV_STATE_SOCKET_HPP

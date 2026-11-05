#ifndef AV_SESSION_BACKEND_HPP
#define AV_SESSION_BACKEND_HPP

#include <stdint.h>

namespace av {

class OwnedFd;
class SharedImageShape;
class SourceMemory;
class WireArena;
class WireReader;
class WireWriter;

class SessionBackend {
public:
    SessionBackend(const SessionBackend &) = delete;
    SessionBackend &operator=(const SessionBackend &) = delete;

    virtual bool knows(uint32_t command_id) const = 0;
    [[nodiscard]] virtual bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                                   WireArena &arena) = 0;
    virtual bool lost() const = 0;
    [[nodiscard]] virtual bool lose_device() = 0;
    virtual void end_batch() = 0;
    virtual bool poll() = 0;
    virtual void gpu_progress(uint64_t &marked, uint64_t &reached) const = 0;
    virtual void bind_signal(void (*signal)(void *context), void *context) = 0;

    [[nodiscard]] virtual bool adopt(SourceMemory &memory, uint64_t &handle) = 0;
    virtual void abandon(const SourceMemory &memory, uint64_t handle) = 0;

    [[nodiscard]] virtual bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                             OwnedFd &descriptor) = 0;
    [[nodiscard]] virtual bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                             const SharedImageShape &shape, uint64_t into,
                                             uint64_t &wire_handle, uint64_t &detail) = 0;
    virtual void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) = 0;

    virtual void shut_down() = 0;

protected:
    SessionBackend() = default;
    ~SessionBackend() = default;
};

}

#endif

/* One host API, as a session sees it.
 *
 * Virtual because there are three real implementations and they have nothing
 * in common below this line: Vulkan and OpenCL each derive from a generated
 * abstract dispatch of their own, and OpenGL has no dispatch object at all -
 * its generated decoder takes a table of function pointers. A session must not
 * know which of those it is holding, so this is where the three become one
 * thing (feature 042).
 *
 * Everything here runs on the context's worker, one call at a time. */
#ifndef AV_SESSION_BACKEND_HPP
#define AV_SESSION_BACKEND_HPP

#include <stdint.h>

namespace av {

class ImportedBlob;
class WireArena;
class WireReader;
class WireWriter;

class SessionBackend {
public:
    SessionBackend(const SessionBackend &) = delete;
    SessionBackend &operator=(const SessionBackend &) = delete;

    /* Whether this backend decodes `command_id` at all. An id the profile did
     * not select is skipped by the device, not faulted, so the two answers must
     * not be confused. */
    virtual bool knows(uint32_t command_id) const = 0;
    /* Decodes one command and runs it, writing whatever answer it has into
     * `reply`. False for a payload that does not decode - including one the
     * arena would not give room for, which is the caller's to tell apart. */
    [[nodiscard]] virtual bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                                   WireArena &arena) = 0;
    /* Whether the host API has lost the device this backend runs on. Asked
     * after every command, because nothing else would notice. */
    virtual bool lost() const = 0;
    /* The batch's last command has run: what was held across it goes. */
    virtual void end_batch() = 0;
    /* Between batches, on the worker: publishes what the host now has for the
     * guest to read without asking; true while it still waits on the GPU. */
    virtual bool poll() = 0;
    /* The mark the latest GPU work was given and the highest mark everything
     * up to which the GPU has finished; both 0 for an API whose commands have
     * finished when they return. A batch completes only once reached passes
     * the mark it left (RingConsumer). */
    virtual void gpu_progress(uint64_t &marked, uint64_t &reached) const = 0;

    /* Turns a checked handle into whatever this API holds memory as, recording
     * it on the blob. False for a handle this API cannot take - which is not a
     * fault, only a blob the guest will have to be copied through. */
    [[nodiscard]] virtual bool adopt(ImportedBlob &blob) = 0;
    /* Gives back whatever adopt made. Called for every blob still held when a
     * session ends, so a guest that never releases leaks nothing. */
    virtual void abandon(ImportedBlob &blob) = 0;

    /* A descriptor for the object `wire_handle` names in this API's commands,
     * so the device can publish it for every context to open (feature 053):
     * false for a handle this backend did not make or a kind it cannot
     * export. The descriptor is the caller's on true, and `handle_kind` says
     * which RVK_SHARED_HANDLE_* kind it is, because OpenGL and OpenCL import
     * only a dma-buf while Vulkan takes either.
     *
     * Every backend answers all three for itself, because an API that shares
     * nothing has to say so rather than inherit a silence: OpenGL and OpenCL
     * export no descriptor at all, and a guest's export through them fails
     * with that API's own error rather than with a host object nobody made. */
    [[nodiscard]] virtual bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                             int &fd) = 0;
    /* That descriptor made into an object of this API's, with `wire_handle`
     * what the guest names it by. `fd` stays the caller's whatever the answer:
     * it is the device's, and a backend that keeps one duplicates it. */
    [[nodiscard]] virtual bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                             uint64_t &wire_handle) = 0;
    /* What import_object made, destroyed. Nothing for a handle this backend
     * did not make that way. */
    virtual void               release_object(uint32_t kind, uint64_t wire_handle) = 0;

protected:
    SessionBackend() = default;
    ~SessionBackend() = default;
};

}  // namespace av

#endif  // AV_SESSION_BACKEND_HPP

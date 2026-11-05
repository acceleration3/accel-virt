#ifndef AV_OPENGL_BACKEND_HPP
#define AV_OPENGL_BACKEND_HPP

#include "device/abi/av_executor.h"
#include "executor/guest_region.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "executor/published_slots.hpp"
#include "proto/submission/gl_error_word.hpp"
#include "executor/session_backend.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/opengl/gl_batch_progress.hpp"
#include "executor/opengl/sync_tokens.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/opengl/gl_pack_buffer.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class DriverLog;
class OwnedFd;
class SharedImageShape;

class GlDisplay;
class SourceMemory;

class OpenGlBackend final : public SessionBackend {
public:
    explicit OpenGlBackend(DriverLog &lines);
    OpenGlBackend(BlockSource &blocks, DriverLog &lines);

    [[nodiscard]] bool initialize(const GlDisplay *display, const AvContextOptions *options,
                                  const OpenGlBackend *share);
    const GlContext   &context() const;
    void               shut_down() override;

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool poll() override;
    void bind_signal(void (*signal)(void *context), void *context) override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    [[nodiscard]] bool adopt(SourceMemory &memory, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;

    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     OwnedFd &descriptor) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape, uint64_t into,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    [[nodiscard]] bool bind_fence(uint64_t token, void *fence);
    void              *fence_for(uint64_t token) const;
    void              *forget_fence(uint64_t token);
    [[nodiscard]] bool pack(uint64_t offset, uint64_t bytes, void *&packed);
    void               unpack();

    uint32_t resolved() const;
    uint64_t ran() const;

private:
    [[nodiscard]] bool lend(const SourceMemory &memory, uint64_t &handle);
    void install_hooks();
    void retire_passed_fence();
    void destroy_made(uint32_t kind, uint32_t name);
    bool granted_fixed_function();
    void publish_errors();

    HostHeap    heap_;
    GlContext   context_;
    glproto::RealGl gl_;
    GlHostImport    import_;
    GlSharedBuffer  buffers_;
    GlPackBuffer    pack_;
    SyncTokens  fences_;
    GlBatchProgress progress_;
    GuestBytes  guest_;
    GuestRegion region_;
    PublishedSlots published_;
    GlErrorWord  errors_;
    uint32_t    resolved_;
    uint64_t    ran_;
    bool        lost_;
    bool        current_;
    DriverLog  &lines_;

    friend struct OpenGlBackendProbe;
};

}

#endif

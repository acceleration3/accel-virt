#ifndef AV_OPENGL_BACKEND_HPP
#define AV_OPENGL_BACKEND_HPP

#include "device/abi/av_executor.h"
#include "executor/eventfd_wake.hpp"
#include "executor/guest_region.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "executor/published_slots.hpp"
#include "proto/submission/gl_error_word.hpp"
#include "executor/session_backend.hpp"
#include "executor/budgeted_heap.hpp"
#include "executor/opengl/gl_batch_progress.hpp"
#include "executor/opengl/gl_buffer_slices.hpp"
#include "executor/opengl/gl_error_queue.hpp"
#include "executor/opengl/gl_memory_objects.hpp"
#include "executor/opengl/gl_texture_slices.hpp"
#include "executor/opengl/gl_transfer_pacer.hpp"
#include "executor/opengl/gl_vram_guard.hpp"
#include "executor/shim/gl_dma_buf_images.hpp"
#include "executor/opengl/gl_shared_fences.hpp"
#include "executor/opengl/sync_tokens.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/opengl/gl_pack_buffer.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/shim/gl_native_fences.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class DriverLog;
class OwnedFd;
class SharedImageShape;

class GlDisplay;
class GlMemoryGroup;
class SourceMemory;
class ByteLedger;

class OpenGlBackend final : public SessionBackend {
public:
    explicit OpenGlBackend(DriverLog &lines);
    OpenGlBackend(BlockSource &blocks, DriverLog &lines);

    void               use_vram(ByteLedger *vram, ByteLedger *host);
    [[nodiscard]] bool initialize(const GlDisplay *display, const AvContextOptions *options,
                                  const OpenGlBackend *share, GlMemoryGroup *group = nullptr);
    const GlContext   &context() const;
    void use_host_ram(ByteLedger *ledger, uint64_t limit) override;
    void               shut_down() override;
    void               let_go_of_thread() override;

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool drain(TeardownBudget &budget) override;
    bool poll() override;
    void bind_signal(void (*signal)(void *context), void *context) override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    [[nodiscard]] bool adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;
    void unbind_arena(const SourceMemory &memory) override;
    void retire_arena(const SourceMemory &memory) override;

    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     OwnedFd &descriptor) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape, uint64_t into,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    [[nodiscard]] bool bind_fence(uint64_t token, void *fence);
    void              *fence_for(uint64_t token) const;
    void              *forget_fence(uint64_t token);
    void               record_error(uint32_t error);
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

    BudgetedHeap    heap_;
    GlContext   context_;
    glproto::RealGl gl_;
    GlHostImport    import_;
    GlSharedBuffer  buffers_;
    GlNativeFences  natives_;
    EventfdWake     wake_;
    GlPackBuffer    pack_;
    SyncTokens  fences_;
    GlTransferPacer pacer_;
    GlSharedFences *shared_fences_;
    GlBatchProgress progress_;
    GlErrorQueue    pending_;
    GlVramGuard     vram_guard_;
    GlMemoryObjects memories_;
    GlDmaBufImages  images_;
    GlBufferSlices  slices_;
    GlTextureSlices textures_;
    GuestBytes  guest_;
    GuestRegion region_;
    PublishedSlots published_;
    GlErrorWord  errors_;
    uint32_t    resolved_;
    uint64_t    ran_;
    uint64_t    submitted_;
    uint64_t    fenced_at_;
    uint64_t    drained_at_;
    void       *drain_fence_;
    bool        current_;
    bool        poll_failure_logged_;
    DriverLog  &lines_;

    friend struct OpenGlBackendProbe;
};

}

#endif

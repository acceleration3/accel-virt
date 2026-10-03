/* One cl_context, and the av context that carries it.
 *
 * A cl_context is where an OpenCL application's world lives, so it is what a
 * transport belongs to: one av OpenCL context, its ring and reply slot, the
 * lock that lets several application threads share them, a blob arena for bulk
 * data, and the shared allocations made on it.
 *
 * Nothing reaches the device until the context is made, and a make that fails
 * holds nothing. `wire()` is the one accessor a call goes through, which is
 * where a faulted transport becomes a lost context - so loss is noticed on
 * every call rather than only where somebody remembered to look.
 *
 * It carries no creation options. AvContextRecord's reserved tail is
 * OpenGL's, and KmtTransport refuses a non-default one on any other sub-API
 * without asking the driver; anything OpenCL needs at creation travels as a
 * command once the context exists. */
#ifndef AV_CLICD_GUEST_CL_CONTEXT_HPP
#define AV_CLICD_GUEST_CL_CONTEXT_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/context_link.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/locked_transport.hpp"
#include "icd/opencl/cl_external_memory.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "icd/opencl/mapped_region.hpp"
#include "icd/opencl/svm_table.hpp"

#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class IcdConnection;
class SharedHandles;
class ThreadLock;
}  // namespace av::icd

namespace av::clicd {

struct GuestClContextProbe;

class GuestClContext {
public:
    /* Nothing is on the host until make; a make that fails leaves the context
     * created and holds nothing; a faulted transport makes it lost, and a lost
     * context never goes back. */
    enum class State : uint8_t { created, made, lost };

    /* A megabyte of ring, as the other two ICDs take: a queue's worth of
     * enqueues without waiting for room. */
    static constexpr uint8_t  ring_order = 20u;
    /* A read's bytes come back in the reply: a megabyte of them per round
     * trip rather than 60 KiB (the host reads ahead, ClReadAhead). */
    static constexpr uint32_t reply_bytes = 1024u * 1024u;
    /* One command's room. A kernel's arguments and a program's source are the
     * large ones, and anything larger crosses as a blob. */
    static constexpr size_t   staging_bytes = 256u * 1024u;
    /* The calls held before they go as one batch (icd::BatchedTransport):
     * two of the largest a command can be, well inside the ring. */
    static constexpr size_t   batch_bytes = 2u * staging_bytes;
    static constexpr uint64_t arena_bytes = KmtBlobArena::default_bytes;
    /* Mappings one context may hold at once. An application that maps more
     * buffers than this at the same time is told so at the map. Affinity
     * keeps a small buffer mapped for each of its tiles while a document is
     * open - more than 64 for a new, empty one. */
    static constexpr uint32_t max_mappings = 4096u;

    GuestClContext(icd::IcdConnection &connection, BlockSource &memory, icd::ThreadLock &send_lock,
                   LogSink &log, icd::SharedHandles &handles);
    ~GuestClContext();
    GuestClContext(const GuestClContext &) = delete;
    GuestClContext &operator=(const GuestClContext &) = delete;

    /* Opens the device, the transport and the arena. invalid_state unless
     * created; not_supported with no avgpu device; no_memory when the
     * staging cannot be had; the driver's own refusal otherwise - and
     * nothing is held after any of them. An arena the device cannot give is
     * logged once, and bulk data then crosses inline.
     *
     * `gl_share` is the share name of the OpenGL context this one shares
     * objects with (cl_khr_gl_sharing), or 0 for none: the device then makes
     * the host's context in that GL context's share group. */
    [[nodiscard]] Status make(uint32_t gl_share = 0u);
    State                state() const;
    /* Whether this context was made from an OpenGL context, so that a GL name
     * an application hands it means that context's object. */
    bool                 shares_gl() const;
    /* The list clCreateContext was given, kept for CL_CONTEXT_PROPERTIES: a
     * list already judged, so it reads. */
    void                 keep_properties(const cl_context_properties *properties);
    /* That list laid out into `out`, and how many values it is; 0 for none. */
    uint32_t             properties(cl_context_properties *out, uint32_t capacity) const;

    /* What a call sends through: null unless made and not lost. A faulted
     * transport makes the context lost here, which is why every call goes
     * through this and not around it. */
    ClientTransport *wire();
    /* The blocks this context's buffers are carved out of its blob arena.
     * Empty of blocks, and not ready, where the capset offered no
     * host-visible memory - which is what makes a buffer staged instead. */
    ClArenaBlocks   &blocks();
    /* Whether the context has been lost, without asking for a transport. */
    bool             lost() const;
    /* The shared allocations made on this context. */
    SvmTable        &svm();
    /* The objects another API made that this context imported. */
    ClExternalMemory &external();
    /* The D3DKMT device this context's transport runs on, which is what an
     * open of another process's handle is made against; 0 before the make. */
    uint32_t         guest_device() const;

    /* Takes a mapping of `bytes` at `offset` into `buffer`, staging the bytes
     * in guest memory. Null when there is no room for another mapping or no
     * memory for the block; the caller is what fills it. */
    uint8_t         *take_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                  cl_map_flags flags);
    /* Takes a mapping whose memory is the arena's rather than a staged block
     * of its own: `at` is that memory, nothing is read in at the map and
     * nothing written back at the unmap, because both sides are looking at
     * one allocation. Null when there is no room for another mapping. */
    MappedRegion    *take_shared_mapping(uint64_t buffer, uint64_t host, size_t offset,
                                         size_t bytes, uint8_t *at);
    /* Takes a mapping of a buffer made over the application's own memory
     * (CL_MEM_USE_HOST_PTR): `at` is that memory, which the caller fills and
     * the unmap writes back, and which is never freed here. Null when there
     * is no room for another mapping. */
    uint8_t         *take_application_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                              cl_map_flags flags, uint8_t *at);
    /* The mapping `pointer` answers, or null for a pointer no live mapping
     * returned. */
    MappedRegion    *mapping_at(const void *pointer);
    /* Gives one back, freeing the staged block. */
    void             give_back_mapping(MappedRegion &region);
    /* Gives back every mapping of `buffer`, which is going: an application
     * may release a buffer it never unmapped, and nothing names those
     * mappings again. Answers how many there were. */
    uint32_t         give_back_mappings_of(uint64_t buffer);
    uint32_t         mappings() const;
    /* Whether the arena opened; bulk data crosses inline when it did not. */
    bool             has_arena() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    friend struct GuestClContextProbe;

    [[nodiscard]] Status build(uint32_t gl_share);
    void                 teardown();

    BlockSource        &memory_;
    icd::ThreadLock    &send_lock_;
    LogSink       &log_;
    icd::ContextLink    link_;
    uint32_t            gl_share_;
    icd::HostChannel                     channel_;
    ObjectStorage<icd::LockedTransport>  locked_;
    ClArenaBlocks                        blocks_;
    SvmTable                             svm_;
    ClExternalMemory                     external_;
    MappedRegion                         mappings_[max_mappings];
    ClPropertyList                       properties_;
    State                                state_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_GUEST_CL_CONTEXT_HPP

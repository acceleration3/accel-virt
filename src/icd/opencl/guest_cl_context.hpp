/* One cl_context, and the rvk context that carries it.
 *
 * A cl_context is where an OpenCL application's world lives, so it is what a
 * transport belongs to: one rvk OpenCL context, its ring and reply slot, the
 * lock that lets several application threads share them, a blob arena for bulk
 * data, and the shared allocations made on it.
 *
 * Nothing reaches the device until the context is made, and a make that fails
 * holds nothing. `wire()` is the one accessor a call goes through, which is
 * where a faulted transport becomes a lost context - so loss is noticed on
 * every call rather than only where somebody remembered to look.
 *
 * It carries no creation options. RvkContextRecord's reserved tail is
 * OpenGL's, and KmtTransport refuses a non-default one on any other sub-API
 * without asking the driver; anything OpenCL needs at creation travels as a
 * command once the context exists. */
#ifndef AV_CLICD_GUEST_CL_CONTEXT_HPP
#define AV_CLICD_GUEST_CL_CONTEXT_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/locked_transport.hpp"
#include "icd/opencl/mapped_region.hpp"
#include "icd/opencl/svm_table.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class IcdConnection;
class LogSink;
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
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    /* One command's room. A kernel's arguments and a program's source are the
     * large ones, and anything larger crosses as a blob. */
    static constexpr size_t   staging_bytes = 256u * 1024u;
    static constexpr uint64_t arena_bytes = KmtBlobArena::default_bytes;
    /* Mappings one context may hold at once. An application that maps more
     * buffers than this at the same time is told so at the map. */
    static constexpr uint32_t max_mappings = 64u;

    GuestClContext(icd::IcdConnection &connection, BlockSource &memory, icd::ThreadLock &send_lock,
                   icd::LogSink &log);
    ~GuestClContext();
    GuestClContext(const GuestClContext &) = delete;
    GuestClContext &operator=(const GuestClContext &) = delete;

    /* Opens the device, the transport and the arena. invalid_state unless
     * created; not_supported with no rvkgpu device; no_memory when the
     * staging cannot be had; the driver's own refusal otherwise - and
     * nothing is held after any of them. An arena the device cannot give is
     * logged once, and bulk data then crosses inline. */
    [[nodiscard]] Status make();
    State                state() const;

    /* What a call sends through: null unless made and not lost. A faulted
     * transport makes the context lost here, which is why every call goes
     * through this and not around it. */
    ClientTransport *wire();
    /* Whether the context has been lost, without asking for a transport. */
    bool             lost() const;
    /* The shared allocations made on this context. */
    SvmTable        &svm();

    /* Takes a mapping of `bytes` at `offset` into `buffer`, staging the bytes
     * in guest memory. Null when there is no room for another mapping or no
     * memory for the block; the caller is what fills it. */
    uint8_t         *take_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                  cl_map_flags flags);
    /* The mapping `pointer` answers, or null for a pointer no live mapping
     * returned. */
    MappedRegion    *mapping_at(const void *pointer);
    /* Gives one back, freeing the staged block. */
    void             give_back_mapping(MappedRegion &region);
    uint32_t         mappings() const;
    /* Whether the arena opened; bulk data crosses inline when it did not. */
    bool             has_arena() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    friend struct GuestClContextProbe;

    [[nodiscard]] Status build();
    void                 teardown();

    icd::IcdConnection &connection_;
    BlockSource        &memory_;
    icd::ThreadLock    &send_lock_;
    icd::LogSink       &log_;
    uint32_t            link_;
    uint8_t            *staging_;
    ObjectStorage<KmtTransport>          transport_;
    ObjectStorage<icd::LockedTransport>  locked_;
    ObjectStorage<KmtBlobArena>          arena_;
    SvmTable                             svm_;
    MappedRegion                         mappings_[max_mappings];
    State                                state_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_GUEST_CL_CONTEXT_HPP

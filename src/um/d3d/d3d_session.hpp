/* The one host session a process's devices of one Direct3D runtime share on an
 * adapter: the Direct3D 11 driver keeps one per adapter, and so do the
 * Direct3D 12 and Direct3D 9 drivers. Its arena is cached host memory bulk
 * data and query results cross through; host memory for an object of its own
 * - a Direct3D 12 heap, Direct3D 11's slice store - is opened on the session
 * too (docs/host-memory.md).
 *
 * A session costs an av context - one of the adapter's doorbells - a ring and
 * a blob arena, and the desktop's processes make dozens of devices each: one
 * session per device ran the adapter out of doorbells before the shell had
 * started. So the first device a process makes on an adapter opens the
 * session, every later one joins it with a host device of its own on the same
 * stream (D3dDeviceChannel), and the last to go closes it. Object ids come
 * from here too, since the host keys a session's objects by them. */
#ifndef AV_UM_D3D_SESSION_HPP
#define AV_UM_D3D_SESSION_HPP

#include "proto/resources/host_memory_request.hpp"

#include "core/support/object_storage.hpp"
#include "um/d3d/query_results.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/id_mint.hpp"
#include "icd/d3d/buffer_slices.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/session_timeline.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "core/support/object_pool.hpp"
#include "um/common/srw_lock.hpp"

#include <stdint.h>

namespace av {
class DriverLog;
}  // namespace av

namespace av {
class BeamPosition;
class BlockSource;
class HostAllocation;
class Kmt;
}  // namespace av


namespace av::d3d11 {
class D3d11Device;
}  // namespace av::d3d11

namespace av::d3d {

class D3dSession {
public:
    /* `arena_size` is the blob arena the session opens: bulk data crosses
     * through it, and query results and the slice store's bookkeeping take
     * its end. */
    /* `select_command` and `flush_command` are what the session's stream
     * selects a device and flushes with (D3dCommandStream::speak_with):
     * Direct3D 11's for 11 and 12, Direct3D 9's for 9. */
    /* `slice_store_size` is the host upload memory dynamic buffers are drawn
     * from in place: 0 for a runtime that places none there. */
    D3dSession(Kmt &kmt, BlockSource &memory, DriverLog &log, uint64_t arena_size, uint64_t slice_store_size,
               uint32_t select_command, uint32_t flush_command);
    ~D3dSession();
    D3dSession(const D3dSession &) = delete;
    D3dSession &operator=(const D3dSession &) = delete;

    /* One more device on the session, opening it on `adapter` (a D3DKMT
     * adapter handle) for the first. The HRESULT a failed open answers the
     * runtime with; nothing joined then. */
    HRESULT join(uint32_t adapter);
    /* `size` bytes of `data` handed to the miniport's DxgkDdiEscape on the
     * session's device, and its answer copied back over them; scoped there to
     * the calling process. */
    [[nodiscard]] Status escape(uint8_t *data, uint32_t size);
    /* Where the scan-out of the adapter's VidPn `source` is now. */
    [[nodiscard]] Status scan_line(uint32_t source, BeamPosition &position);
    /* One device fewer; the last closes the session. */
    void    leave();
    uint32_t users() const;
    /* Whether the host faulted the session's context - its Direct3D device
     * was lost - as its transport found or its fence page says, a session
     * whose devices are idle included (HostChannel::lost); and why. False
     * and none for one not open. */
    bool      lost() const;
    FaultCode fault_code() const;

    /* A non-zero id no other object or device of the session has. Any thread. */
    uint32_t               next_id();
    d3d::D3dCommandStream &stream();
    ThreadLock            &lock();
    d3d11::QueryResults   &query_results();
    /* The process's view of the arena, and its size; null and 0 without one. */
    uint8_t               *arena_view() const;
    uint64_t               arena_bytes() const;
    /* What the transport lends bulk data from. */
    ClientBlobArena       &blobs();
    KmtTransport          &transport();
    /* The Kmt the session was opened through. */
    Kmt                   &kmt() const;
    /* The deferred contexts of the session's Direct3D 11 devices. */
    ObjectPool<d3d11::D3d11Device> &contexts();
    /* The slices CPU-written buffers live in, when the host draws them in
     * place (docs/mapped-buffers-plan.md); null when it cannot, and every
     * such buffer is then copied as before. */
    BufferSlices                   *slices();
    /* The slice store slices are offsets of: the handle the host names it
     * by, and this process's view; 0 and null without slices. */
    uint64_t                        slice_memory() const;
    uint8_t                        *slice_view() const;
    /* `bytes` of the host memory `memory` names, opened on the session and
     * kept until close_memory or the session's close, under the session's
     * lock (HostChannel::open_memory). */
    [[nodiscard]] Status            open_memory(uint64_t bytes, HostMemoryRequest memory, HostAllocation *&made);
    /* The process's own memory lent the same way (HostChannel::adopt_memory). */
    [[nodiscard]] Status            adopt_memory(void *address, uint64_t bytes, HostMemoryRequest memory,
                                                 HostAllocation *&made);
    void                            close_memory(HostAllocation &allocation);
    /* Lent memory an object was made over, let go of while the host's work
     * over it may still run (a Direct3D 11 resource can be destroyed with
     * draws queued): kept with the batch it was retired in, and closed once
     * the GPU has finished that batch (reclaim_retired). With no room to
     * note it the oldest is waited for and closed instead - memory is never
     * closed under work that may still use it. */
    void                            retire_memory(HostAllocation &allocation);
    /* Each retired allocation whose batch the host GPU has finished, closed;
     * called as the session's devices flush and go. */
    void                            reclaim_retired();

private:
    static constexpr uint8_t  ring_order = 22u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   staging_bytes = 512u * 1024u;
    static constexpr size_t   batch_bytes = 1u << 20u;

    HRESULT open(uint32_t adapter);
    void    close();
    /* Whether the host draws buffers in place: asked once, of a Direct3D 11
     * session with an arena. */
    bool    host_has_slices();
    /* The slice store opened and its slices taken, when the host draws in
     * place. */
    void    open_slices();

    BlockSource                    &memory_;
    DriverLog                      &lines_;
    SrwLock                              lock_;
    GuestConnection                      connection_;
    icd::HostChannel                     channel_;
    ObjectStorage<d3d::D3dCommandStream> stream_;
    SrwLock                              slices_lock_;
    ObjectStorage<SessionTimeline>        timeline_;
    ObjectStorage<BufferSlices>          slices_;
    uint64_t                             slice_store_bytes_;
    HostAllocation                      *slice_store_;
    d3d11::QueryResults                  query_results_;
    ObjectPool<d3d11::D3d11Device>       contexts_;
    uint32_t                             members_;
    icd::IdMint<uint32_t>                ids_;
    uint32_t                             select_command_;
    uint32_t                             flush_command_;
};

}  // namespace av::d3d

#endif  // AV_UM_D3D_SESSION_HPP

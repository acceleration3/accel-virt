/* The one host session a process's devices of one Direct3D runtime share on an
 * adapter: the Direct3D 11 driver keeps one per adapter, and so does the
 * Direct3D 12 driver, with a larger arena its upload and readback heaps live in.
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

#include "core/support/object_storage.hpp"
#include "um/d3d/query_results.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "core/support/object_pool.hpp"
#include "um/icd/srw_lock.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
}  // namespace av

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d11 {
class D3d11Device;
}  // namespace av::d3d11

namespace av::d3d {

class D3dSession {
public:
    /* `arena_size` is the blob arena the session opens: bulk data crosses
     * through it, and the Direct3D 12 driver places its CPU-visible heaps in
     * what it does not lend. */
    D3dSession(Kmt &kmt, BlockSource &memory, icd::LogSink &log, uint64_t arena_size);
    ~D3dSession();
    D3dSession(const D3dSession &) = delete;
    D3dSession &operator=(const D3dSession &) = delete;

    /* One more device on the session, opening it on `adapter` (a D3DKMT
     * adapter handle) for the first. The HRESULT a failed open answers the
     * runtime with; nothing joined then. */
    HRESULT join(uint32_t adapter);
    /* One device fewer; the last closes the session. */
    void    leave();

    /* A non-zero id no other object or device of the session has. Any thread. */
    uint32_t               next_id();
    d3d::D3dCommandStream &stream();
    icd::ThreadLock       &lock();
    d3d11::QueryResults   &query_results();
    /* The process's view of the arena, and its size; null and 0 without one. */
    uint8_t               *arena_view() const;
    uint64_t               arena_bytes() const;
    /* What the transport lends bulk data from. */
    ClientBlobArena       &blobs();
    KmtTransport          &transport();
    /* The deferred contexts of the session's Direct3D 11 devices. */
    ObjectPool<d3d11::D3d11Device> &contexts();

private:
    static constexpr uint8_t  ring_order = 22u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   staging_bytes = 512u * 1024u;
    static constexpr size_t   batch_bytes = 1u << 20u;

    HRESULT open(uint32_t adapter);
    void    close();

    BlockSource                         &memory_;
    icd::LogSink                        &log_;
    SrwLock                              lock_;
    GuestConnection                      connection_;
    ObjectStorage<KmtTransport>          transport_;
    ObjectStorage<KmtBlobArena>          arena_;
    ObjectStorage<d3d::D3dCommandStream> stream_;
    d3d11::QueryResults                  query_results_;
    ObjectPool<d3d11::D3d11Device>       contexts_;
    uint8_t                             *staging_;
    uint8_t                             *batch_;
    uint32_t                             members_;
    uint32_t                             next_id_;
    uint64_t                             arena_bytes_;
};

}  // namespace av::d3d

#endif  // AV_UM_D3D_SESSION_HPP

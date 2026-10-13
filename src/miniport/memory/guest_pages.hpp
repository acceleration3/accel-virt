/* Guest RAM the driver alone owns (Platform::allocate_pages), with the frame of
 * each of its pages and at most one view of it in the process that asked:
 * what the device is handed as a GUEST blob and a process reads and writes
 * with no copy (docs/protocol.md, "Context memory").
 *
 * A process's allocation was that memory once, and VidMm evicts one whenever it
 * likes - a suspended app's first of all - moving its pages while the device
 * still held their addresses. These pages stay where they are until the driver
 * lets go of them. The view goes before the pages do, so a process is never
 * left writing memory that belongs to someone else. */
#ifndef AV_GUEST_PAGES_HPP
#define AV_GUEST_PAGES_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <stdint.h>

namespace av {

struct GuestPagesProbe;

class GuestPages {
public:
    explicit GuestPages(Platform &platform);
    ~GuestPages();
    GuestPages(const GuestPages &) = delete;
    GuestPages &operator=(const GuestPages &) = delete;

    /* `bytes` of zeroed pages and their frames. invalid_parameter for no bytes,
     * a size that is not whole pages or more pages than a page list counts;
     * invalid_state while holding some; no_memory, holding nothing, when the
     * platform has none. PASSIVE. */
    [[nodiscard]] Status allocate(uint64_t bytes);
    /* The pages mapped into the calling process: its address in view().
     * invalid_state unless held and not yet mapped; the platform's refusal,
     * mapping nothing, otherwise. PASSIVE. */
    [[nodiscard]] Status map_into_process();
    /* The same, but read-only to the process: it can see the pages and not
     * change what the driver reads in them. PASSIVE. */
    [[nodiscard]] Status map_read_only_into_process();
    /* The process's view gone and the pages kept. PASSIVE. */
    void                 unmap_from_process();
    /* The process's view gone, then the pages. PASSIVE. */
    void                 release();
    /* The process's view gone and the pages kept out of use for good: a device
     * that never answered a teardown may still write them, so they are never
     * handed to anyone again. PASSIVE. */
    void                 abandon();

    bool          held() const;
    uint64_t      bytes() const;
    /* Empty unless held. */
    GuestPageList pages() const;
    /* The process's address of the pages; 0 unless mapped. */
    uint64_t      view() const;

    bool invariants_hold() const;

private:
    Platform &platform_;
    void     *record_;
    uint64_t *frames_;
    uint64_t  bytes_;
    void     *mapping_;
    uint64_t  view_;

    friend struct GuestPagesProbe;
};

}  // namespace av

#endif  // AV_GUEST_PAGES_HPP

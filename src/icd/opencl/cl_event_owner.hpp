/* Who a delivered event goes back to.
 *
 * clSetEventCallback has to keep the event alive: an application may register
 * a callback and release the event on the next line, and the OpenCL spec still
 * promises the callback runs and is handed that cl_event. So the watcher holds
 * a reference of its own, and this is how it gives that reference back once
 * the callback has run - on the watcher's thread, with no ICD lock held, which
 * is the only moment it is safe to.
 *
 * Virtual because it breaks a cycle the two classes would otherwise form: the
 * root owns the watcher and the watcher hands events back to the root. The
 * tests' double is the second implementation, and it is what lets the watcher
 * be tested without a whole ICD around it. */
#ifndef AV_CLICD_CL_EVENT_OWNER_HPP
#define AV_CLICD_CL_EVENT_OWNER_HPP

namespace av::clicd {

class ClObject;

class ClEventOwner {
public:
    ClEventOwner(const ClEventOwner &) = delete;
    ClEventOwner &operator=(const ClEventOwner &) = delete;

    /* Before the callback: anything the application is owed on this event
     * that has not been handed over yet - the bytes of a non-blocking read,
     * which a callback on that read must see. */
    virtual void deliver_watched(ClObject &event) = 0;
    /* Drops the reference taken when the callback was registered. It may be
     * the last one, in which case the event goes here. */
    virtual void release_watched(ClObject &event) = 0;

protected:
    ClEventOwner() = default;
    ~ClEventOwner() = default;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_EVENT_OWNER_HPP

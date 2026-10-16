/* The callbacks an application asked to be told with when a memory object or
 * a context is destroyed (clSetMemObjectDestructorCallback,
 * clSetContextDestructorCallback).
 *
 * They are the ICD's to keep: no guest function pointer crosses the wire, and
 * the destruction they are about is the ICD's own - the moment its object
 * table lets go of the object, which is after the host was told. The API
 * says the callbacks run in the reverse of the order they were registered in,
 * and that a registration may fail for want of memory, which is the only
 * thing that refuses one.
 *
 * The owner calls every method under the lock its object table releases
 * under, so the callbacks due for an object are taken in the same hold that
 * destroyed it - before another object can be made at its address and
 * registered for - and run once the lock is let go, since a callback may
 * well call back into the ICD. */
#ifndef AV_CLICD_CL_DESTRUCTOR_CALLBACKS_HPP
#define AV_CLICD_CL_DESTRUCTOR_CALLBACKS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::clicd {

class ClDestructorCallbacks {
public:
    using MemCallback = void(CL_CALLBACK *)(cl_mem object, void *user_data);
    using ContextCallback = void(CL_CALLBACK *)(cl_context object, void *user_data);

    /* Objects one release can destroy: the object and each one above it it
     * alone held, which the object kinds bound. */
    static constexpr uint32_t max_gone = 8u;

private:
    struct Row {
        const void     *object_;
        MemCallback     mem_;
        ContextCallback context_;
        void           *user_data_;
    };

public:
    /* The callbacks one release made due, held until its lock is let go. */
    class Due {
    public:
        explicit Due(BlockSource &memory);
        Due(const Due &) = delete;
        Due &operator=(const Due &) = delete;

        /* Each, in the order take put them. */
        void     run() const;
        uint32_t count() const;
        /* Callbacks due that there was no memory to hold until the lock was
         * let go: they never run. */
        uint32_t lost() const;

    private:
        friend class ClDestructorCallbacks;

        GrowableArray<Row> rows_;
        uint32_t           lost_;
    };

    explicit ClDestructorCallbacks(BlockSource &memory);
    ClDestructorCallbacks(const ClDestructorCallbacks &) = delete;
    ClDestructorCallbacks &operator=(const ClDestructorCallbacks &) = delete;

    /* Kept for `object`, whose handle the callback is handed; no_memory when
     * there is no memory to keep it. */
    [[nodiscard]] Status keep(cl_mem object, MemCallback callback, void *user_data);
    [[nodiscard]] Status keep(cl_context object, ContextCallback callback, void *user_data);

    /* Every callback kept for the `count` objects `gone` names, moved into
     * `due` and forgotten here: the objects in the order they went, each
     * one's callbacks the latest registered first. */
    void take(const void *const *gone, uint32_t count, Due &due);

    uint32_t kept() const;
    bool     invariants_hold() const;

private:
    [[nodiscard]] Status keep_row(const Row &row);
    bool                 names_gone(const Row &row, const void *const *gone, uint32_t count) const;

    GrowableArray<Row> rows_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_DESTRUCTOR_CALLBACKS_HPP

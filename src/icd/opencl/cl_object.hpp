/* One OpenCL object as the application holds it.
 *
 * Every handle this ICD hands out is a pointer to one of these, and the first
 * word of it is the dispatch table: that word is the whole of the ICD ABI, and
 * the Khronos loader reads it and jumps without checking anything. So it is
 * written before the object is handed over, always, whatever the object is.
 *
 * Underneath, an object is a number on the wire and a place in a tree: a
 * context's objects go when the context does, so each carries its parent.
 * Kinds are ordered so a parent's kind is always lower than its child's, which
 * is what bounds the chain. */
#ifndef AV_CLICD_CL_OBJECT_HPP
#define AV_CLICD_CL_OBJECT_HPP

#include "icd/opencl/cl_info_answers.hpp"

#include <CL/cl.h>
#include <CL/cl_icd.h>

#include <stdint.h>

namespace av {
class HostAllocation;
}  // namespace av

namespace av::clicd {

class GuestClContext;

struct ClObjectProbe;

class ClObject {
public:
    /* In dependency order: a parent's kind is lower than its children's, so
     * the chain from any object to its context is finite and short. */
    enum class Kind : uint8_t {
        platform, device, context, command_queue, program, mem, sampler, kernel, event
    };

    ClObject(const cl_icd_dispatch &table, BlockSource &memory, Kind kind, uint64_t wire, GuestClContext *context,
             ClObject *parent);
    ClObject(const ClObject &) = delete;
    ClObject &operator=(const ClObject &) = delete;

    Kind            kind() const;
    uint64_t        wire() const;
    GuestClContext *context() const;
    ClObject       *parent() const;

    /* For a mem alone: where in its context's blob arena its memory is, and
     * how much of it. A buffer backed there is one the device made over the
     * same bytes, so a map hands the application this memory and an unmap
     * writes nothing back. `backed()` is false for every other kind and for
     * a buffer the arena had no room for, which is staged instead. */
    void     back_with(uint64_t offset, uint64_t bytes);
    bool     backed() const;
    uint64_t backing_offset() const;
    uint64_t backing_bytes() const;

    /* For a buffer made over the application's own memory
     * (CL_MEM_USE_HOST_PTR): that memory, which a map hands back and the
     * host's buffer was filled from. Null for every other object. */
    void     use_application_memory(void *memory);
    uint8_t *application_memory() const;
    /* For a buffer whose application memory the device took as its own
     * (GuestClContext::lend): what was lent, which the buffer's release
     * gives back, and which makes its maps cost no copy. Null otherwise; a
     * sub-buffer names its buffer's. */
    void            lend(HostAllocation &lent);
    HostAllocation *lent() const;

    /* For a sub-buffer: the buffer it is a region of, on which the ICD holds
     * one of the application's references for as long as the sub-buffer
     * lives, as OpenCL keeps a buffer while a sub-buffer of it does. The
     * buffer is not the sub-buffer's parent: a parent is of a lower kind, and
     * both are memory. Null for every other object. */
    void      slice_of(ClObject &buffer);
    ClObject *sliced_from() const;

    /* Whether this object is `other` or is somewhere below it. */
    bool descends_from(const ClObject &other) const;
    /* This object or the closest one above it of `kind`, or null: a buffer's
     * context, a sub-buffer's buffer. */
    ClObject *nearest(Kind kind);

    /* The application's own reference count, which is not the host's: the
     * guest retains and releases, and only the last release destroys. */
    void     retain();
    /* Whether that was the last one. */
    bool     release();
    uint32_t references() const;
    /* The objects made on this one hold it, as OpenCL's objects hold what
     * they were made on: a context outlives the application's last reference
     * while a buffer of it lives, and a program while a kernel of it does. */
    void     adopt();
    void     orphan();
    /* Held by neither the application nor an object made on it. */
    bool     unheld() const;

    /* A command queue's: whether everything sent to it is known to have
     * completed. A new queue is; anything that leaves work on it is not, until
     * a drain or a transfer the host answered only once done says so again. A
     * transfer to an idle queue needs no drain first - which is most of what
     * an application reading tiles back one blocking rect at a time pays. */
    void     busy();
    void     idle();
    bool     known_idle() const;
    /* A queue's count of the events made on it, in the order they were: a
     * queue here runs in order, so an event's place says which of a queue's
     * events completes first. `take_ordinal` is the queue's, the next one;
     * `stamp` gives an event its own; 0 is an event of no queue. */
    uint64_t take_ordinal();
    void     stamp(uint64_t ordinal);
    uint64_t ordinal() const;

    /* A mem's answers to the info queries that cannot change once it is made
     * (ClInfoAnswers): `kept_answer` gives one kept to `asked`, false when
     * none is, and `keep_answer` keeps the host's. */
    [[nodiscard]] bool kept_answer(cl_mem_info name, const ClInfoRequest &asked, cl_int &result) const;
    void               keep_answer(cl_mem_info name, const void *value, size_t bytes);

    bool invariants_hold() const;

private:
    friend struct ClObjectProbe;

    /* First, and it stays first: the loader reads this word and calls through
     * it. Nothing may be declared before it. */
    const cl_icd_dispatch *dispatch_;
    GuestClContext        *context_;
    ClObject              *parent_;
    uint64_t               wire_;
    uint64_t               backing_offset_;
    uint64_t               backing_bytes_;
    uint8_t               *application_memory_;
    HostAllocation        *lent_;
    ClObject              *sliced_from_;
    uint32_t               references_;
    uint32_t               children_;
    Kind                   kind_;
    /* Atomic: an application's threads may share a queue, and the API leaves
     * ordering their enqueues to it - each only needs to see the other's. */
    bool                   idle_;
    /* Atomic for a queue, which threads may share; an event's is written once,
     * before it is handed out. */
    uint64_t               ordinal_;
    /* One each for the type, the flags, the size, the offset and whether it
     * uses shared memory: none is longer than a word. */
    ClInfoAnswers<sizeof(uint64_t)> fixed_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_OBJECT_HPP
